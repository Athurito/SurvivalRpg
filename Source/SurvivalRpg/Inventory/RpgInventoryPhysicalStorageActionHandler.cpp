#include "RpgInventoryUiActionComponent.h"
#include "RpgInventoryUiActionDomainHandlers.h"
#include "RpgInventoryContainerActor.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryFragment_StorageProfile.h"
#include "RpgInventoryItemInstance.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "PhysicalStorage"

namespace
{
// Fragment hooks may run while a batch is staged. Pin source membership and settings as well as item revisions.
struct FStorageContextGuard
{
	UWorld* World;
	FVector Location;
	TWeakObjectPtr<ARpgBaseCampActor> Base;
	FVector BaseLocation = FVector::ZeroVector;
	float BaseRadius = 0.0f;
	TArray<URpgInventoryManagerComponent*> Sources;
	TMap<TWeakObjectPtr<URpgInventoryManagerComponent>, int32> Revisions;
	TMap<TWeakObjectPtr<URpgInventoryContainerComponent>, int32> Settings;

	FStorageContextGuard(UWorld* InWorld, FVector InLocation) : World(InWorld), Location(InLocation)
	{
		Base = RpgStorageAccessRules::ResolveBaseAtLocation(World, Location);
		if (Base.IsValid()) { BaseLocation = Base->GetActorLocation(); BaseRadius = Base->GetBuildRadius(); }
		RpgStorageAccessRules::ResolveStorageSources(World, Location, 0.0f, Sources);
		for (URpgInventoryManagerComponent* Source : Sources)
		{
			Revisions.Add(Source, Source->GetInventoryRevision());
			if (auto* Container = Source->GetOwner()->FindComponentByClass<URpgInventoryContainerComponent>())
			{ Settings.Add(Container, Container->GetSettingsRevision()); }
		}
	}
	bool IsCurrent() const
	{
		if (RpgStorageAccessRules::ResolveBaseAtLocation(World, Location) != Base.Get() ||
			(Base.IsValid() && (!Base->GetActorLocation().Equals(BaseLocation) || Base->GetBuildRadius() != BaseRadius))) { return false; }
		TArray<URpgInventoryManagerComponent*> Current;
		RpgStorageAccessRules::ResolveStorageSources(World, Location, 0.0f, Current);
		if (Current != Sources) { return false; }
		for (const auto& Pair : Revisions) { if (!Pair.Key.IsValid() || Pair.Key->GetInventoryRevision() != Pair.Value) { return false; } }
		for (const auto& Pair : Settings) { if (!Pair.Key.IsValid() || Pair.Key->GetSettingsRevision() != Pair.Value) { return false; } }
		return true;
	}
};

ARpgInventoryContainerActor* FindChest(const UWorld* World, FName Id)
{
	if (!World || Id.IsNone()) { return nullptr; }
	ARpgInventoryContainerActor* Found = nullptr;
	for (TActorIterator<ARpgInventoryContainerActor> It(World); It; ++It)
	{
		if (It->GetContainerComponent()->GetPersistentContainerId() == Id)
		{
			if (Found) { return nullptr; }
			Found = *It;
		}
	}
	return Found;
}

bool SameRequest(const FRpgPhysicalStorageRequest& A, const FRpgPhysicalStorageRequest& B)
{
	if (A.Command != B.Command || A.ContainerId != B.ContainerId ||
		A.ExpectedSettingsRevision != B.ExpectedSettingsRevision || A.RelocationSessionId != B.RelocationSessionId || A.BuildableDefinition != B.BuildableDefinition ||
		!A.Transform.Equals(B.Transform, 0.0f) || A.Assignments.Num() != B.Assignments.Num()) { return false; }
	for (int32 Index = 0; Index < A.Assignments.Num(); ++Index)
	{
		if (A.Assignments[Index].ItemDefinition != B.Assignments[Index].ItemDefinition ||
			A.Assignments[Index].Category != B.Assignments[Index].Category ||
			A.Assignments[Index].AssignmentOrder != B.Assignments[Index].AssignmentOrder) { return false; }
	}
	return true;
}

void DirtyChest(ARpgInventoryContainerActor* Chest)
{
	if (Chest)
	{
		Chest->ForceNetUpdate();
		if (ARpgGameModeBase* Mode = Chest->GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
		{
			Mode->MarkWorldContainerSaveDirty(Chest->GetContainerComponent()->GetPersistentContainerId(), Chest->GetInventoryManager());
		}
	}
}

bool PlanCosts(AActor* Player, const FVector& Location, const TArray<FRpgBaseBuildResourceCost>& Costs,
	TArray<FRpgInventoryBatchOperation>& OutOperations)
{
	TArray<URpgInventoryManagerComponent*> Sources;
	RpgStorageAccessRules::ResolveStorageSources(Player->GetWorld(), Location, 0.0f, Sources);
	TArray<FRpgCraftingResourceCost> ResourceCosts;
	for (const FRpgBaseBuildResourceCost& Cost : Costs)
	{
		FRpgCraftingResourceCost& Row = ResourceCosts.AddDefaulted_GetRef();
		Row.ItemDefinition = Cost.ItemDefinition;
		Row.Count = Cost.Count;
	}
	TArray<FRpgCraftingRefundEntry> Receipts;
	return URpgCraftingStationComponent::BuildResourceConsumptionPlan(Player, Sources, ResourceCosts, 1, OutOperations, Receipts);
}
}

void URpgInventoryUiActionComponent::ClientPhysicalStorageCommandCompleted_Implementation(
	FGuid RequestId, bool bSucceeded, const FText& Message)
{
	OnPhysicalStorageCommandCompleted.Broadcast(RequestId, bSucceeded, Message);
}

void URpgInventoryUiActionComponent::RequestPhysicalStorageCommand_Implementation(FRpgPhysicalStorageRequest Request)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Player || !GetOwner()->HasAuthority() || !Request.RequestId.IsValid() || Request.Assignments.Num() > 128)
	{
		ClientPhysicalStorageCommandCompleted(Request.RequestId, false, LOCTEXT("InvalidRequest", "Ungültige Lageranfrage."));
		return;
	}
	// Revalidate access before exposing cached results, including after a chest has moved.
	if (Request.Command != ERpgPhysicalStorageCommand::Build &&
		Request.Command != ERpgPhysicalStorageCommand::CancelRelocate)
	{
		ARpgInventoryContainerActor* Chest = FindChest(GetWorld(), Request.ContainerId);
		const bool bAccess = Request.Command == ERpgPhysicalStorageCommand::Relocate
			? Chest && Chest->GetContainerComponent()->IsContainerAccessible() &&
				RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Chest->GetActorLocation()) ==
				RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Player->GetActorLocation())
			: Chest && Chest->GetContainerComponent()->CanActorAccess(Player);
		if (!bAccess)
		{
			ClientPhysicalStorageCommandCompleted(Request.RequestId, false, LOCTEXT("NoAccess", "Kiste nicht erreichbar."));
			return;
		}
	}
	URpgInventoryManagerComponent* CurrentInventory = FindPlayerInventory();
	PhysicalStorageCommands.RemoveAll([CurrentInventory](const FRpgPhysicalStorageCommandRecord& Record) {
		return !Record.bInFlight && (!CurrentInventory || Record.PlayerInventory != CurrentInventory ||
			Record.PlayerMutationEpoch != CurrentInventory->GetMutationEpoch());
	});
	for (const FRpgPhysicalStorageCommandRecord& Existing : PhysicalStorageCommands)
	{
		if (Existing.Request.RequestId == Request.RequestId)
		{
			if (!SameRequest(Existing.Request, Request))
			{
				ClientPhysicalStorageCommandCompleted(Request.RequestId, false, LOCTEXT("Collision", "Anfrage-ID wurde bereits verwendet."));
			}
			else if (!Existing.bInFlight)
			{
				ClientPhysicalStorageCommandCompleted(Request.RequestId, Existing.bSucceeded, Existing.Message);
			}
			return;
		}
	}
	while (PhysicalStorageCommands.Num() >= 64)
	{
		const int32 Evict = PhysicalStorageCommands.IndexOfByPredicate([](const FRpgPhysicalStorageCommandRecord& Row) { return !Row.bInFlight; });
		if (Evict == INDEX_NONE) { return; }
		PhysicalStorageCommands.RemoveAt(Evict);
	}
	FRpgPhysicalStorageCommandRecord& Admission = PhysicalStorageCommands.AddDefaulted_GetRef();
	Admission.Request = Request;
	Admission.PlayerInventory = CurrentInventory;
	Admission.PlayerMutationEpoch = CurrentInventory ? CurrentInventory->GetMutationEpoch() : 0;
	FText Message;
	const bool bSucceeded = ExecutePhysicalStorageCommand(Request, Message);
	// Synchronous gameplay callbacks can append records; never retain an array reference across execution.
	for (FRpgPhysicalStorageCommandRecord& Row : PhysicalStorageCommands)
	{
		if (Row.Request.RequestId == Request.RequestId)
		{
			Row.bInFlight = false;
			Row.bSucceeded = bSucceeded;
			Row.Message = Message;
			break;
		}
	}
	ClientPhysicalStorageCommandCompleted(Request.RequestId, bSucceeded, Message);
}

bool URpgInventoryUiActionComponent::CanPlacePhysicalStorage(URpgBaseBuildableDefinition* Definition,
	FTransform Transform, FName RelocatingContainerId, FText& OutReason) const
{
	OutReason = LOCTEXT("PlacementBlocked", "Hier ist keine Kistenplatzierung möglich.");
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Definition || !Definition->BuildActorClass || !Definition->BuildActorClass->IsChildOf(ARpgInventoryContainerActor::StaticClass()) ||
		!Player || Transform.ContainsNaN() || !Transform.GetScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER)) { return false; }
	const ARpgInventoryContainerActor* Template = Cast<ARpgInventoryContainerActor>(Definition->BuildActorClass->GetDefaultObject());
	if (!Template || Template->GetContainerComponent()->GetBuildableDefinition() != Definition) { return false; }
	const FRotator Rotation = Transform.Rotator();
	if (!FMath::IsNearlyZero(Rotation.Pitch) || !FMath::IsNearlyZero(Rotation.Roll)) { return false; }
	ARpgBaseCampActor* Base = RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Transform.GetLocation());
	if (!Base || !Base->CanPlaceBuildableAtTransform(Definition, Transform, Player)) { return false; }
	ARpgInventoryContainerActor* Moving = nullptr;
	if (!RelocatingContainerId.IsNone())
	{
		Moving = FindChest(GetWorld(), RelocatingContainerId);
		if (!Moving || RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Moving->GetActorLocation()) != Base) { return false; }
		if (RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Player->GetActorLocation()) != Base)
		{ OutReason = LOCTEXT("LeaveBase", "Bleibe zum Versetzen innerhalb derselben Basis."); return false; }
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PhysicalStoragePlacement), false);
	Query.AddIgnoredActor(Base);
	if (Moving) { Query.AddIgnoredActor(Moving); }
	const FVector Location = Transform.GetLocation();
	FHitResult Ground;
	const float TraceDistance = Definition->GroundTraceDistance;
	if (!FMath::IsFinite(TraceDistance) || TraceDistance <= 0.0f ||
		!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0, 0, TraceDistance),
			Location - FVector(0, 0, TraceDistance), ECC_WorldStatic, Query) ||
		FMath::Abs(Ground.ImpactPoint.Z - Location.Z) > 10.0f ||
		Ground.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(Definition->MaxGroundSlopeDegrees))) { return false; }
	const FVector HalfExtent = Definition->PlacementHalfExtent;
	if (HalfExtent.ContainsNaN() || HalfExtent.GetMin() <= 0.0f) { return false; }
	for (const float X : { -HalfExtent.X, HalfExtent.X })
	{
		for (const float Y : { -HalfExtent.Y, HalfExtent.Y })
		{
			if (!Base->ContainsLocation(Transform.TransformPosition(FVector(X, Y, 0)))) { return false; }
		}
	}
	const FVector Center = Location + FVector(0, 0, HalfExtent.Z + Definition->GroundClearance);
	if (GetWorld()->OverlapBlockingTestByChannel(Center, Transform.GetRotation(), ECC_WorldStatic,
		FCollisionShape::MakeBox(HalfExtent), Query)) { return false; }
	OutReason = FText::GetEmpty();
	return true;
}

bool URpgInventoryUiActionComponent::CanContinuePhysicalStorageRelocation(const FRpgPhysicalStorageRequest& Request) const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	const ARpgInventoryContainerActor* Chest = FindChest(GetWorld(), Request.ContainerId);
	const URpgInventoryManagerComponent* Inventory = FindPlayerInventory();
	return Request.RelocationSessionId.IsValid() && Request.RelocationSessionId == PhysicalStorageRelocationSessionId &&
		Player && Player == PhysicalStorageRelocationPawn.Get() && Chest && Chest == PhysicalStorageRelocationChest.Get() &&
		Inventory && Inventory == PhysicalStorageRelocationInventory.Get() && Inventory->GetMutationEpoch() == PhysicalStorageRelocationEpoch &&
		PhysicalStorageRelocationBase.IsValid() && Chest->GetContainerComponent()->IsContainerAccessible() &&
		Request.ExpectedSettingsRevision == PhysicalStorageRelocationRevision &&
		Chest->GetContainerComponent()->GetSettingsRevision() == PhysicalStorageRelocationRevision &&
		RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Player->GetActorLocation()) == PhysicalStorageRelocationBase.Get() &&
		RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Chest->GetActorLocation()) == PhysicalStorageRelocationBase.Get();
}

bool URpgInventoryUiActionComponent::ExecutePhysicalStorageCommand(const FRpgPhysicalStorageRequest& Request, FText& OutMessage)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!Player || !PlayerInventory) { OutMessage = LOCTEXT("NoPlayer", "Spielerinventar nicht verfügbar."); return false; }
	if (Request.Command == ERpgPhysicalStorageCommand::CancelRelocate)
	{
		if (!Request.RelocationSessionId.IsValid() || Request.RelocationSessionId != PhysicalStorageRelocationSessionId) { return false; }
		PhysicalStorageRelocationSessionId.Invalidate();
		OutMessage = FText::GetEmpty();
		return true;
	}
	if (Request.Command == ERpgPhysicalStorageCommand::Build)
	{
		URpgBaseBuildableDefinition* Definition = Request.BuildableDefinition;
		if (!CanPlacePhysicalStorage(Definition, Request.Transform, NAME_None, OutMessage)) { return false; }
		const FStorageContextGuard Context(GetWorld(), Request.Transform.GetLocation());
		TArray<FRpgInventoryBatchOperation> Costs;
		if (!PlanCosts(Player, Request.Transform.GetLocation(), Definition->BuildCosts, Costs))
		{
			UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Physical storage construction has insufficient reachable materials: Definition=%s Player=%s"),
				*GetNameSafe(Definition), *GetNameSafe(Player));
			OutMessage = LOCTEXT("CostsMissing", "Materialien fehlen."); return false;
		}
		ERpgInventoryMutationResultCode Code = ERpgInventoryMutationResultCode::InvalidRequest;
		if (!Costs.IsEmpty() && !PlayerInventory->CanApplyInventoryBatch(Costs, Code))
		{
			UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Physical storage construction cost preflight rejected: Definition=%s Result=%s"),
				*GetNameSafe(Definition), *UEnum::GetValueAsString(Code));
			OutMessage = LOCTEXT("CostsChanged", "Materialbestand hat sich geändert."); return false;
		}
		ARpgBaseCampActor* Base = RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Request.Transform.GetLocation());
		ARpgInventoryContainerActor* NewChest = GetWorld()->SpawnActorDeferred<ARpgInventoryContainerActor>(
			Definition->BuildActorClass, Request.Transform, Base, Player, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!NewChest) { OutMessage = LOCTEXT("SpawnFailed", "Kiste konnte nicht erstellt werden."); return false; }
		NewChest->GetContainerComponent()->SetConstructionPending(true);
		NewChest->GetContainerComponent()->SetRuntimeBuilt(true);
		// Construction scripts run before costs are committed. Only the fully initialized actor may be published.
		NewChest->SetActorHiddenInGame(true);
		NewChest->SetActorEnableCollision(false);
		NewChest->FinishSpawning(Request.Transform);
		const bool bActorValid = IsValid(NewChest) && !NewChest->IsActorBeingDestroyed();
		const bool bDefinitionValid = bActorValid && NewChest->GetContainerComponent()->GetBuildableDefinition() == Definition;
		const bool bEmptyInventory = bActorValid && NewChest->GetInventoryManager()->GetAllEntries().IsEmpty();
		const bool bInitialGridValid = bEmptyInventory && !Definition->ChestUpgradeTiers.IsEmpty() &&
			NewChest->GetInventoryManager()->SetDefaultGridSize(Definition->ChestUpgradeTiers[0].GridSize);
		const bool bTransformValid = bActorValid && NewChest->GetActorTransform().Equals(Request.Transform);
		const bool bPlacementValid = bActorValid && CanPlacePhysicalStorage(Definition, Request.Transform, NAME_None, OutMessage);
		const bool bContextValid = Context.IsCurrent();
		if (!bDefinitionValid || !bInitialGridValid || !bTransformValid || !bPlacementValid || !bContextValid)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning,
				TEXT("Physical storage construction rejected after actor initialization: Actor=%s Valid=%d Definition=%d Empty=%d Grid=%d Transform=%d Placement=%d Context=%d Pending=%d"),
				*GetNameSafe(NewChest), bActorValid, bDefinitionValid, bEmptyInventory, bInitialGridValid,
				bTransformValid, bPlacementValid, bContextValid, bActorValid && NewChest->GetContainerComponent()->IsConstructionPending());
			if (bActorValid) { NewChest->Destroy(); }
			OutMessage = LOCTEXT("StagedChestInvalid", "Die Kiste konnte an dieser Stelle nicht fertiggestellt werden.");
			return false;
		}
		const FName BuiltBaseId = Base->GetBaseId();
		auto Finish = [NewChest, BuiltBaseId]() {
			NewChest->GetContainerComponent()->SetResolvedBaseId(BuiltBaseId);
			NewChest->GetContainerComponent()->SetConstructionPending(false);
			NewChest->SetActorHiddenInGame(false); NewChest->SetActorEnableCollision(true); DirtyChest(NewChest);
		};
		const FRpgPhysicalStorageMetadata PreparedMetadata = NewChest->GetContainerComponent()->ExportPhysicalStorageMetadata();
		FRpgInventoryBatchCapacityChange PreparedChest;
		PreparedChest.Inventory = NewChest->GetInventoryManager();
		PreparedChest.NewGridSize = PreparedMetadata.GridSize;
		PreparedChest.ExpectedRevision = PreparedChest.Inventory->GetInventoryRevision();
		auto Revalidate = [this, Definition, Request, NewChest, Context, PreparedMetadata]() {
			FText Reason;
			return IsValid(NewChest) && !NewChest->IsActorBeingDestroyed() && Context.IsCurrent() &&
				NewChest->GetActorTransform().Equals(Request.Transform) &&
				NewChest->GetContainerComponent()->IsConstructionPending() &&
				NewChest->GetContainerComponent()->GetBuildableDefinition() == Definition &&
				NewChest->GetContainerComponent()->GetSettingsRevision() == PreparedMetadata.SettingsRevision &&
				NewChest->GetContainerComponent()->GetPersistentContainerId() == PreparedMetadata.PersistentContainerId &&
				NewChest->GetInventoryManager()->GetAllEntries().IsEmpty() &&
				NewChest->GetInventoryManager()->GetDefaultGridSize() == PreparedMetadata.GridSize &&
				CanPlacePhysicalStorage(Definition, Request.Transform, NAME_None, Reason);
		};
		const FRpgInventoryMutationResult BuildResult = PlayerInventory->ApplyInventoryBatch(Costs, Request.RequestId, { PreparedChest }, Finish, Revalidate);
		if (!BuildResult.IsSuccess())
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Physical storage construction payment rejected: Actor=%s Result=%s"),
				*GetNameSafe(NewChest), *UEnum::GetValueAsString(BuildResult.Code));
			NewChest->Destroy(); OutMessage = LOCTEXT("CostsChanged", "Materialbestand hat sich geändert."); return false;
		}
		OutMessage = LOCTEXT("Built", "Kiste gebaut.");
		return true;
	}
	ARpgInventoryContainerActor* Chest = FindChest(GetWorld(), Request.ContainerId);
	URpgInventoryContainerComponent* Container = Chest ? Chest->GetContainerComponent() : nullptr;
	const bool bAccess = Request.Command == ERpgPhysicalStorageCommand::Relocate
		? CanContinuePhysicalStorageRelocation(Request) : Container && Container->CanActorAccess(Player);
	if (!Container || !bAccess || Request.ExpectedSettingsRevision < 0 ||
		Container->GetSettingsRevision() != Request.ExpectedSettingsRevision)
	{ OutMessage = LOCTEXT("StaleChest", "Kiste hat sich geändert. Bitte erneut versuchen."); return false; }
	switch (Request.Command)
	{
	case ERpgPhysicalStorageCommand::BeginRelocate:
	{
		ARpgBaseCampActor* Base = RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Chest->GetActorLocation());
		if (!Base || !Container->GetBuildableDefinition() ||
			RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Player->GetActorLocation()) != Base) { return false; }
		PhysicalStorageRelocationSessionId = Request.RequestId;
		PhysicalStorageRelocationPawn = Player;
		PhysicalStorageRelocationChest = Chest;
		PhysicalStorageRelocationBase = Base;
		PhysicalStorageRelocationInventory = PlayerInventory;
		PhysicalStorageRelocationEpoch = PlayerInventory->GetMutationEpoch();
		PhysicalStorageRelocationRevision = Container->GetSettingsRevision();
		OutMessage = FText::GetEmpty();
		return true;
	}
	case ERpgPhysicalStorageCommand::SetAssignments:
		if (!Container->SetAssignments(Request.Assignments, Request.ExpectedSettingsRevision))
		{ OutMessage = LOCTEXT("InvalidAssignments", "Ungültige oder doppelte Zuordnung."); return false; }
		OutMessage = LOCTEXT("AssignmentsSaved", "Zuordnungen gespeichert.");
		return true;
	case ERpgPhysicalStorageCommand::Relocate:
		if (!CanPlacePhysicalStorage(Container->GetBuildableDefinition(), Request.Transform, Request.ContainerId, OutMessage)) { return false; }
		// Transform callbacks must not reuse this authorization before the settings revision advances.
		PhysicalStorageRelocationSessionId.Invalidate();
		if (!Container->TryRelocatePhysicalStorage(Request.Transform, Request.ExpectedSettingsRevision)) { return false; }
		DirtyChest(Chest);
		OutMessage = LOCTEXT("Moved", "Kiste versetzt.");
		return true;
	case ERpgPhysicalStorageCommand::Upgrade:
	{
		const FStorageContextGuard Context(GetWorld(), Chest->GetActorLocation());
		URpgBaseBuildableDefinition* Definition = Container->GetBuildableDefinition();
		const int32 Tier = Container->ExportPhysicalStorageMetadata().UpgradeTier;
		if (!Definition || !Definition->ChestUpgradeTiers.IsValidIndex(Tier + 1))
		{ OutMessage = LOCTEXT("MaximumTier", "Kein weiterer Ausbau verfügbar."); return false; }
		const FRpgBaseChestUpgradeTier& Upgrade = Definition->ChestUpgradeTiers[Tier + 1];
		const FRpgInventoryGridSize OldGrid = Container->ExportPhysicalStorageMetadata().GridSize;
		if (Upgrade.GridSize.Width < OldGrid.Width || Upgrade.GridSize.Height < OldGrid.Height || Upgrade.GridSize == OldGrid)
		{ OutMessage = LOCTEXT("InvalidTier", "Ungültiger Kistenausbau."); return false; }
		TArray<FRpgInventoryBatchOperation> Costs;
		if (!PlanCosts(Player, Chest->GetActorLocation(), Upgrade.Costs, Costs))
		{ OutMessage = LOCTEXT("CostsMissing", "Materialien fehlen."); return false; }
		FRpgInventoryBatchCapacityChange Capacity;
		Capacity.Inventory = Chest->GetInventoryManager();
		Capacity.NewGridSize = Upgrade.GridSize;
		Capacity.ExpectedRevision = Capacity.Inventory->GetInventoryRevision();
		const FVector OriginalLocation = Chest->GetActorLocation();
		if (!PlayerInventory->ApplyInventoryBatch(Costs, Request.RequestId, { Capacity },
			[Container, Tier]() { Container->SetUpgradeTier(Tier + 1); },
			[Container, Player, Request, Chest, OriginalLocation, Context]() {
				return Context.IsCurrent() && IsValid(Chest) && Container->CanActorAccess(Player) && Chest->GetActorLocation().Equals(OriginalLocation) &&
					Container->GetSettingsRevision() == Request.ExpectedSettingsRevision;
			}).IsSuccess())
		{ OutMessage = LOCTEXT("UpgradeFailed", "Ausbau konnte nicht bestätigt werden."); return false; }
		DirtyChest(Chest);
		OutMessage = LOCTEXT("Upgraded", "Kiste aufgewertet.");
		return true;
	}
	case ERpgPhysicalStorageCommand::DepositMaterials:
	{
		const FStorageContextGuard Context(GetWorld(), Chest->GetActorLocation());
		const FVector OriginalLocation = Chest->GetActorLocation();
		TArray<FRpgInventoryBatchOperation> Operations;
		int32 RequestedCount = 0;
		int32 AcceptedCount = 0;
		for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
		{
			const URpgInventoryFragment_StorageProfile* Profile = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_StorageProfile>() : nullptr;
			const URpgInventoryFragment_ItemTraits* Traits = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
			if (!Profile || !Profile->CanAutoDepositPhysical() || !Traits || !Traits->IsMaterial() || Entry.StackCount <= 0) { continue; }
			RequestedCount += Entry.StackCount;
			TArray<URpgInventoryContainerComponent*> Targets;
			if (RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), Chest->GetActorLocation()))
			{ Targets = RpgStorageAccessRules::GetPhysicalStorageTargets(GetWorld(), Chest->GetActorLocation(), 0.0f, Entry.Instance->GetItemDef()); }
			else
			{
				int64 Order = 0;
				if (Container->GetAssignmentRank(Entry.Instance->GetItemDef(), Order) != INDEX_NONE) { Targets.Add(Container); }
			}
			int32 Remaining = Entry.StackCount;
			for (URpgInventoryContainerComponent* Target : Targets)
			{
				if (Remaining <= 0) { break; }
				if (!Target || !Target->IsContainerAccessible() || Target->GetTransferPolicy() != ERpgInventoryContainerTransferPolicy::Bidirectional) { continue; }
				FRpgInventoryBatchOperation Candidate;
				Candidate.SourceInventory = PlayerInventory;
				Candidate.TargetInventory = Target->GetInventoryManager();
				Candidate.ItemId = Entry.ItemId;
				Candidate.ExpectedSourceRevision = PlayerInventory->GetInventoryRevision();
				Candidate.ExpectedTargetRevision = Candidate.TargetInventory->GetInventoryRevision();
				int32 Low = 0, High = Remaining;
				while (Low < High)
				{
					Candidate.Quantity = Low + (High - Low + 1) / 2;
					Operations.Add(Candidate);
					ERpgInventoryMutationResultCode Code;
					const bool bFits = PlayerInventory->CanApplyInventoryBatch(Operations, Code);
					Operations.Pop();
					if (bFits) { Low = Candidate.Quantity; } else { High = Candidate.Quantity - 1; }
				}
				if (Low > 0) { Candidate.Quantity = Low; Operations.Add(Candidate); Remaining -= Low; AcceptedCount += Low; }
			}
		}
		if (Operations.IsEmpty()) { OutMessage = LOCTEXT("NoDestinations", "Kein passendes Lagerziel oder kein freier Platz."); return false; }
		if (!PlayerInventory->ApplyInventoryBatch(Operations, Request.RequestId, {}, {},
			[Container, Player, Request, Chest, OriginalLocation, Context]() {
				return Context.IsCurrent() && IsValid(Chest) && Container->CanActorAccess(Player) &&
					Container->GetSettingsRevision() == Request.ExpectedSettingsRevision && Chest->GetActorLocation().Equals(OriginalLocation);
			}).IsSuccess())
		{ OutMessage = LOCTEXT("DepositChanged", "Lagerbestand hat sich geändert. Bitte erneut versuchen."); return false; }
		OutMessage = FText::Format(LOCTEXT("DepositSummary", "{0} eingelagert · {1} verbleiben."), FText::AsNumber(AcceptedCount), FText::AsNumber(RequestedCount - AcceptedCount));
		return true;
	}
	default: OutMessage = LOCTEXT("InvalidRequest", "Ungültige Lageranfrage."); return false;
	}
}

#undef LOCTEXT_NAMESPACE
