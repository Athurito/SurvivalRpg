#include "RpgInventoryContainerComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenStorageContainer.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContainerComponent)

namespace
{
	void FlushContainerReplication(AActor& OwnerActor)
	{
		OwnerActor.FlushNetDormancy();
		OwnerActor.ForceNetUpdate();
	}
}

URpgInventoryContainerComponent::URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	OpenContainerOption.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	OpenContainerOption.Prompt.ActionText = NSLOCTEXT("RpgInventory", "OpenStorageContainerText", "Open");
	OpenContainerOption.Prompt.TargetText = NSLOCTEXT("RpgInventory", "OpenStorageContainerSubText", "Storage");
	OpenContainerOption.Prompt.InteractionPriority = 50;
	OpenContainerOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenStorageContainer::StaticClass();
}

void URpgInventoryContainerComponent::BeginPlay()
{
	Super::BeginPlay();
	// Death loot and transient drops opt out of crafting and retain their own persistence lifecycle.
	if (!bConstructionPending && bAllowCraftingAccess && TransferPolicy == ERpgInventoryContainerTransferPolicy::Bidirectional)
	{
		if (ARpgGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
		{
			GameMode->RegisterPersistentWorldContainer(this);
			return;
		}
		EnsurePersistentContainerId();
		const ARpgBaseCampActor* Base = RpgStorageAccessRules::ResolveBaseAtLocation(GetWorld(), GetOwner()->GetActorLocation());
		SetResolvedBaseId(Base ? Base->GetBaseId() : NAME_None);
		const TArray<FRpgStorageAssignment> AuthoredAssignments = PhysicalStorageMetadata.Assignments;
		SetAssignments(AuthoredAssignments);
	}
}

void URpgInventoryContainerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>())
		{
			GameMode->UnregisterPersistentWorldContainer(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void URpgInventoryContainerComponent::EnsurePersistentContainerId()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !PersistentContainerId.IsNone())
	{
		return;
	}
	PersistentContainerId = !PhysicalStorageMetadata.bRuntimeBuilt &&
		(Owner->HasAnyFlags(RF_WasLoaded) || Owner->IsNetStartupActor())
		? FName(*FString::Printf(TEXT("Chest_%s"), *Owner->GetName()))
		: FName(*FString::Printf(TEXT("Chest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	PhysicalStorageMetadata.PersistentContainerId = PersistentContainerId;
	NotifyPhysicalStorageSettingsChanged();
}

FRpgPhysicalStorageMetadata URpgInventoryContainerComponent::ExportPhysicalStorageMetadata() const
{
	FRpgPhysicalStorageMetadata Result = PhysicalStorageMetadata;
	Result.PersistentContainerId = PersistentContainerId;
	if (const URpgInventoryManagerComponent* Inventory = GetInventoryManager())
	{
		Result.GridSize = Inventory->GetDefaultGridSize();
	}
	return Result;
}

const TArray<FRpgStorageAssignment>& URpgInventoryContainerComponent::GetAssignments() const
{
	return PhysicalStorageMetadata.Assignments;
}

int32 URpgInventoryContainerComponent::GetSettingsRevision() const
{
	return PhysicalStorageMetadata.SettingsRevision;
}

FName URpgInventoryContainerComponent::GetBaseId() const
{
	return PhysicalStorageMetadata.BaseId;
}

bool URpgInventoryContainerComponent::RestorePhysicalStorageMetadata(const FRpgPhysicalStorageMetadata& Metadata)
{
	URpgInventoryManagerComponent* Inventory = GetInventoryManager();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Inventory ||
		Metadata.PersistentContainerId.IsNone() || Metadata.UpgradeTier < 0 ||
		Metadata.SettingsRevision < 0 || Metadata.SettingsRevision == MAX_int32 ||
		Metadata.AssignmentOrderHighWaterMark < 0 || Metadata.AssignmentOrderHighWaterMark == MAX_int64 ||
		!Metadata.GridSize.IsValid() || !Inventory->CanSetDefaultGridSize(Metadata.GridSize))
	{
		return false;
	}
	for (int32 Index = 0; Index < Metadata.Assignments.Num(); ++Index)
	{
		const FRpgStorageAssignment& Rule = Metadata.Assignments[Index];
		if (!Rule.IsValid() || Rule.AssignmentOrder <= 0 ||
			Rule.AssignmentOrder > Metadata.AssignmentOrderHighWaterMark)
		{
			return false;
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if ((Metadata.Assignments[Other].ItemDefinition == Rule.ItemDefinition &&
				Metadata.Assignments[Other].Category == Rule.Category) ||
				Metadata.Assignments[Other].AssignmentOrder == Rule.AssignmentOrder)
			{
				return false;
			}
		}
	}
	if (!Inventory->SetDefaultGridSize(Metadata.GridSize))
	{
		return false;
	}
	PhysicalStorageMetadata = Metadata;
	PersistentContainerId = Metadata.PersistentContainerId;
	FlushContainerReplication(*GetOwner());
	OnPhysicalStorageSettingsChanged.Broadcast(this);
	return true;
}

void URpgInventoryContainerComponent::SetResolvedBaseId(FName NewBaseId)
{
	if (GetOwner() && GetOwner()->HasAuthority() && PhysicalStorageMetadata.BaseId != NewBaseId)
	{
		PhysicalStorageMetadata.BaseId = NewBaseId;
		NotifyPhysicalStorageSettingsChanged();
	}
}

void URpgInventoryContainerComponent::SetRuntimeBuilt(bool bNewRuntimeBuilt)
{
	if (GetOwner() && GetOwner()->HasAuthority() && PhysicalStorageMetadata.bRuntimeBuilt != bNewRuntimeBuilt)
	{
		PhysicalStorageMetadata.bRuntimeBuilt = bNewRuntimeBuilt;
		if (bNewRuntimeBuilt)
		{
			// A Blueprint's authored default id must never be reused by two constructed actors.
			// Disk reconstruction restores its explicitly saved identity through RestorePhysicalStorageMetadata.
			PersistentContainerId = FName(*FString::Printf(TEXT("Chest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			PhysicalStorageMetadata.PersistentContainerId = PersistentContainerId;
		}
		NotifyPhysicalStorageSettingsChanged();
	}
}

void URpgInventoryContainerComponent::SetUpgradeTier(int32 NewTier)
{
	if (GetOwner() && GetOwner()->HasAuthority() && NewTier >= 0 && PhysicalStorageMetadata.UpgradeTier != NewTier)
	{
		PhysicalStorageMetadata.UpgradeTier = NewTier;
		NotifyPhysicalStorageSettingsChanged();
	}
}

void URpgInventoryContainerComponent::MarkPhysicalStorageMoved()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		NotifyPhysicalStorageSettingsChanged();
	}
}

bool URpgInventoryContainerComponent::TryRelocatePhysicalStorage(const FTransform& Transform, int32 ExpectedRevision)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsContainerAccessible() ||
		ExpectedRevision != GetSettingsRevision()) { return false; }
	{
		TGuardValue<bool> Guard(bPhysicalMoveInProgress, true);
		Owner->SetReplicateMovement(true);
		if (!Owner->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics)) { return false; }
	}
	// Publish only after access is restored, so read models observe the committed state.
	MarkPhysicalStorageMoved();
	return true;
}

void URpgInventoryContainerComponent::SetConstructionPending(bool bPending)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bConstructionPending == bPending) { return; }
	bConstructionPending = bPending;
	if (!bPending)
	{
		EnsurePersistentContainerId();
		const TArray<FRpgStorageAssignment> AuthoredAssignments = PhysicalStorageMetadata.Assignments;
		SetAssignments(AuthoredAssignments);
		NotifyPhysicalStorageSettingsChanged();
	}
}

bool URpgInventoryContainerComponent::SetAssignments(const TArray<FRpgStorageAssignment>& NewAssignments, int32 ExpectedRevision)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() ||
		(ExpectedRevision != INDEX_NONE && ExpectedRevision != GetSettingsRevision()))
	{
		return false;
	}
	for (int32 Index = 0; Index < NewAssignments.Num(); ++Index)
	{
		if (!NewAssignments[Index].IsValid()) { return false; }
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (NewAssignments[Index].ItemDefinition == NewAssignments[Other].ItemDefinition &&
				NewAssignments[Index].Category == NewAssignments[Other].Category) { return false; }
		}
	}
	TArray<FRpgStorageAssignment> Resolved = NewAssignments;
	int64 NewOrder = RpgStorageAccessRules::AllocateAssignmentOrder(GetWorld());
	if (NewOrder <= 0 || NewOrder >= MAX_int64 - NewAssignments.Num()) { return false; }
	int64 HighWaterMark = PhysicalStorageMetadata.AssignmentOrderHighWaterMark;
	bool bChanged = Resolved.Num() != PhysicalStorageMetadata.Assignments.Num();
	for (int32 Index = 0; Index < Resolved.Num(); ++Index)
	{
		FRpgStorageAssignment& Rule = Resolved[Index];
		const FRpgStorageAssignment* Existing = PhysicalStorageMetadata.Assignments.FindByPredicate(
			[&Rule](const FRpgStorageAssignment& Entry)
			{
				return Entry.ItemDefinition == Rule.ItemDefinition && Entry.Category == Rule.Category && Entry.AssignmentOrder > 0;
			});
		Rule.AssignmentOrder = Existing ? Existing->AssignmentOrder : NewOrder++;
		if (Rule.AssignmentOrder <= 0) { return false; }
		HighWaterMark = FMath::Max(HighWaterMark, Rule.AssignmentOrder);
		bChanged |= !PhysicalStorageMetadata.Assignments.IsValidIndex(Index) ||
			PhysicalStorageMetadata.Assignments[Index].ItemDefinition != Rule.ItemDefinition ||
			PhysicalStorageMetadata.Assignments[Index].Category != Rule.Category ||
			PhysicalStorageMetadata.Assignments[Index].AssignmentOrder != Rule.AssignmentOrder;
	}
	if (bChanged)
	{
		PhysicalStorageMetadata.Assignments = MoveTemp(Resolved);
		PhysicalStorageMetadata.AssignmentOrderHighWaterMark = HighWaterMark;
		NotifyPhysicalStorageSettingsChanged();
	}
	return true;
}

int32 URpgInventoryContainerComponent::GetAssignmentRank(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int64& OutOrder) const
{
	OutOrder = MAX_int64;
	const URpgInventoryItemDefinition* Definition = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
	if (!Definition) { return INDEX_NONE; }
	const URpgInventoryFragment_ItemTraits* Traits = Cast<URpgInventoryFragment_ItemTraits>(Definition->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass()));
	int32 Rank = INDEX_NONE;
	for (const FRpgStorageAssignment& Rule : PhysicalStorageMetadata.Assignments)
	{
		const int32 Candidate = Rule.ItemDefinition == ItemDefinition ? 0 :
			(Rule.Category.IsValid() && Traits && Traits->ItemTags.HasTag(Rule.Category) ? 1 : INDEX_NONE);
		if (Candidate != INDEX_NONE && (Rank == INDEX_NONE || Candidate < Rank))
		{
			Rank = Candidate;
			OutOrder = Rule.AssignmentOrder;
		}
		else if (Candidate != INDEX_NONE && Candidate == Rank) { OutOrder = FMath::Min(OutOrder, Rule.AssignmentOrder); }
	}
	return Rank != INDEX_NONE ? Rank :
		(GetInventoryManager() && GetInventoryManager()->GetTotalItemCountByDefinition(ItemDefinition) > 0 ? 2 : INDEX_NONE);
}

void URpgInventoryContainerComponent::NotifyPhysicalStorageSettingsChanged()
{
	PhysicalStorageMetadata.SettingsRevision = PhysicalStorageMetadata.SettingsRevision < MAX_int32 - 1
		? PhysicalStorageMetadata.SettingsRevision + 1 : 0;
	FlushContainerReplication(*GetOwner());
	OnPhysicalStorageSettingsChanged.Broadcast(this);
}

void URpgInventoryContainerComponent::OnRep_PhysicalStorageMetadata()
{
	OnPhysicalStorageSettingsChanged.Broadcast(this);
}

void URpgInventoryContainerComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	if (bConstructionPending) { return; }
	// Dropped inventories expose their owner-sensitive Collect option through the actor itself.
	// This component still supplies authoritative transfer/access checks after the loot screen opens.
	if (GetOwner() && GetOwner()->IsA<ARpgDroppedInventoryActor>())
	{
		return;
	}
	if (!bAccessible && bHideInteractionWhenInaccessible)
	{
		return;
	}

	FInteractionOption Option = OpenContainerOption;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	Option.TargetRef.TargetActor = GetOwner();
	Option.TargetRef.TargetComponent = Cast<UPrimitiveComponent>(InteractionAnchor.Get());
	Option.TargetRef.WorldLocation = GetInteractionWorldLocation();
	Option.Prompt.InteractionRange = InteractionRadius > 0.0f
		? InteractionRadius
		: Option.Prompt.InteractionRange;
	const bool bSemanticallyAccessible = bAccessible && GetOwner() && InteractQuery.RequestingAvatar.IsValid();
	Option.Availability = bSemanticallyAccessible
		? ERpgInteractionAvailability::Available
		: ERpgInteractionAvailability::Blocked;
	if (!bSemanticallyAccessible)
	{
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgInventory", "StorageUnavailable", "Storage is unavailable");
	}
	InteractionBuilder.AddInteractionOption(Option);
}

void URpgInventoryContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bAccessible);
	DOREPLIFETIME(ThisClass, InteractionRadius);
	DOREPLIFETIME(ThisClass, PersistentContainerId);
	DOREPLIFETIME(ThisClass, TransferPolicy);
	DOREPLIFETIME(ThisClass, PhysicalStorageMetadata);
}

URpgInventoryManagerComponent* URpgInventoryContainerComponent::GetInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

bool URpgInventoryContainerComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsContainerAccessible() || OwnerActor == nullptr || RequestingActor == nullptr)
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(GetInteractionWorldLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgInventoryContainerComponent::SetContainerAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner();
		OwnerActor && OwnerActor->HasAuthority() && bAccessible != bNewAccessible)
	{
		bAccessible = bNewAccessible;
		FlushContainerReplication(*OwnerActor);
	}
}

void URpgInventoryContainerComponent::SetInteractionRadius(float NewInteractionRadius)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		const float SanitizedRadius = FMath::IsFinite(NewInteractionRadius)
			? FMath::Max(0.0f, NewInteractionRadius)
			: 0.0f;
		if (InteractionRadius != SanitizedRadius)
		{
			InteractionRadius = SanitizedRadius;
			FlushContainerReplication(*OwnerActor);
		}
	}
}

void URpgInventoryContainerComponent::SetInteractionAnchor(USceneComponent* NewInteractionAnchor)
{
	if (!NewInteractionAnchor || NewInteractionAnchor->GetOwner() == GetOwner())
	{
		InteractionAnchor = NewInteractionAnchor;
	}
}

FVector URpgInventoryContainerComponent::GetInteractionWorldLocation() const
{
	if (const USceneComponent* Anchor = InteractionAnchor.Get();
		Anchor && Anchor->GetOwner() == GetOwner())
	{
		return Anchor->GetComponentLocation();
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void URpgInventoryContainerComponent::SetTransferPolicy(
	ERpgInventoryContainerTransferPolicy NewTransferPolicy)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		if (static_cast<uint8>(NewTransferPolicy) <=
				static_cast<uint8>(ERpgInventoryContainerTransferPolicy::WithdrawOnly) &&
			TransferPolicy != NewTransferPolicy)
		{
			TransferPolicy = NewTransferPolicy;
			FlushContainerReplication(*OwnerActor);
		}
	}
}

bool URpgInventoryContainerComponent::CanReceiveTransferFrom(
	const URpgInventoryManagerComponent* SourceInventory) const
{
	const URpgInventoryManagerComponent* ManagedInventory = GetInventoryManager();
	return SourceInventory && ManagedInventory &&
		!bConstructionPending && (SourceInventory == ManagedInventory ||
		 TransferPolicy == ERpgInventoryContainerTransferPolicy::Bidirectional);
}

void URpgInventoryContainerComponent::ConfigureAsDeathLootContainer()
{
	bAccessible = false;
	bHideInteractionWhenInaccessible = true;
	TransferPolicy = ERpgInventoryContainerTransferPolicy::WithdrawOnly;
	bAllowCraftingAccess = false;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryContainerComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (!FMath::IsFinite(InteractionRadius) || InteractionRadius < 0.0f)
	{
		Context.AddError(NSLOCTEXT(
			"RpgInventoryContainer",
			"InvalidInteractionRadius",
			"Interaction Radius must be finite and at least zero centimeters."));
		Result = EDataValidationResult::Invalid;
	}
	if (static_cast<uint8>(TransferPolicy) >
		static_cast<uint8>(ERpgInventoryContainerTransferPolicy::WithdrawOnly))
	{
		Context.AddError(NSLOCTEXT(
			"RpgInventoryContainer",
			"InvalidTransferPolicy",
			"Transfer Policy contains an unknown value."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
