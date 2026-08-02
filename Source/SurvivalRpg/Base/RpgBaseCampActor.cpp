#include "RpgBaseCampActor.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseBuildableDefinition.h"
#include "RpgBaseStorageComponent.h"
#include "RpgBaseStorageDomainAnchorComponent.h"
#include "RpgBaseStorageStationComponent.h"
#include "RpgBaseStorageUpgradeDefinition.h"
#include "RpgPersonalStorageLockerActor.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseCampActor)

namespace
{
	FRpgInventoryGridSize GetMinimumPreservedRootGrid(
		const FRpgInventoryGraphSaveData& Graph,
		const URpgInventoryManagerComponent* Inventory)
	{
		FRpgInventoryGridSize Required = Inventory
			? Inventory->GetDefaultGridSize() : FRpgInventoryGridSize();
		if (!Inventory)
		{
			return Required;
		}

		const FRpgInventoryContainerHandle Root =
			FRpgInventoryContainerHandle::MakeRoot(
				Inventory->GetDefaultContainerId());
		for (const FRpgInventorySavedItem& Item : Graph.Items)
		{
			if (Item.Container != Root || !Item.Placement.IsValid())
			{
				continue;
			}
			const FRpgInventoryGridSize Occupied =
				Item.Placement.GetOccupiedSize();
			Required.Width = FMath::Max(
				Required.Width,
				Item.Placement.X + Occupied.Width);
			Required.Height = FMath::Max(
				Required.Height,
				Item.Placement.Y + Occupied.Height);
		}
		return Required;
	}
}

ARpgBaseCampActor::ARpgBaseCampActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicatingMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BaseStorageComponent = CreateDefaultSubobject<URpgBaseStorageComponent>(TEXT("BaseStorageComponent"));

	ArmoryInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("ArmoryInventoryComponent"));
	ArmoryInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	ArmoryInventoryComponent->SetFixedMaxEntries(48);
	FRpgInventoryGridSize ArmoryGridSize;
	ArmoryGridSize.Width = 8;
	ArmoryGridSize.Height = 6;
	ArmoryInventoryComponent->SetDefaultGridSize(ArmoryGridSize);

	ContainmentInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("ContainmentInventoryComponent"));
	ContainmentInventoryComponent->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	ContainmentInventoryComponent->SetFixedMaxEntries(0);
	FRpgInventoryGridSize ContainmentGridSize;
	ContainmentGridSize.Width = 2;
	ContainmentGridSize.Height = 1;
	ContainmentInventoryComponent->SetDefaultGridSize(ContainmentGridSize);

	MaterialDepotAnchor = CreateDefaultSubobject<
		URpgBaseStorageDomainAnchorComponent>(TEXT("MaterialDepotAnchor"));
	MaterialDepotAnchor->ConfigureAnchor(
		TEXT("MaterialDepot"),
		RpgGameplayTags::Storage_Domain_Materials);
	ArmoryAnchor = CreateDefaultSubobject<
		URpgBaseStorageDomainAnchorComponent>(TEXT("ArmoryAnchor"));
	ArmoryAnchor->ConfigureAnchor(
		TEXT("Armory"),
		RpgGameplayTags::Storage_Domain_Armory);
	RiftVaultAnchor = CreateDefaultSubobject<
		URpgBaseStorageDomainAnchorComponent>(TEXT("RiftVaultAnchor"));
	RiftVaultAnchor->ConfigureAnchor(
		TEXT("RiftVault"),
		RpgGameplayTags::Storage_Domain_RiftContainment);
}

void ARpgBaseCampActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		RefreshStorageAnchorVisuals();
	}
}

void ARpgBaseCampActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DestroyPersonalLockers();
	}
	Super::EndPlay(EndPlayReason);
}

void ARpgBaseCampActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, BaseId);
}

#if WITH_EDITOR
EDataValidationResult ARpgBaseCampActor::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const auto AddError =
		[&Context, &Result](const FText& Message)
		{
			Context.AddError(Message);
			Result = EDataValidationResult::Invalid;
		};

	struct FRequiredAnchorDomain
	{
		FGameplayTag DomainTag;
		const TCHAR* DisplayName;
	};
	const FRequiredAnchorDomain RequiredDomains[] =
	{
		{ RpgGameplayTags::Storage_Domain_Materials, TEXT("Materials") },
		{ RpgGameplayTags::Storage_Domain_Armory, TEXT("Armory") },
		{ RpgGameplayTags::Storage_Domain_RiftContainment, TEXT("RiftContainment") }
	};

	TInlineComponentArray<URpgBaseStorageDomainAnchorComponent*> Anchors(this);
	TSet<FName> RequiredAnchorIds;
	for (const FRequiredAnchorDomain& RequiredDomain : RequiredDomains)
	{
		int32 DomainAnchorCount = 0;
		for (const URpgBaseStorageDomainAnchorComponent* Anchor : Anchors)
		{
			if (!IsValid(Anchor) ||
				Anchor->GetDomainTag() != RequiredDomain.DomainTag)
			{
				continue;
			}

			++DomainAnchorCount;
			const FName AnchorId = Anchor->GetAnchorId();
			if (AnchorId.IsNone())
			{
				AddError(FText::FromString(FString::Printf(
					TEXT("The required %s storage anchor must have a non-empty stable AnchorId."),
					RequiredDomain.DisplayName)));
			}
			else if (RequiredAnchorIds.Contains(AnchorId))
			{
				AddError(FText::FromString(FString::Printf(
					TEXT("Required base storage anchors must use distinct ids; '%s' is repeated."),
					*AnchorId.ToString())));
			}
			else
			{
				RequiredAnchorIds.Add(AnchorId);
			}
		}

		if (DomainAnchorCount != 1)
		{
			AddError(FText::FromString(FString::Printf(
				TEXT("Base camps require exactly one %s storage-domain anchor; found %d."),
				RequiredDomain.DisplayName,
				DomainAnchorCount)));
		}
	}

	return Result;
}
#endif

bool ARpgBaseCampActor::EnsureClaimedByController(APlayerController* Controller)
{
	if (!HasAuthority() || !Controller)
	{
		return false;
	}

	const FString ProfileKey = ResolveProfileKey(Controller);
	if (ProfileKey.IsEmpty())
	{
		return false;
	}
	if (OwnerProfileKey.IsEmpty())
	{
		OwnerProfileKey = ProfileKey;
		ForceNetUpdate();
		if (ARpgGameModeBase* GameMode =
				GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
		{
			GameMode->MarkBaseStorageSaveDirty(this);
		}
	}
	return OwnerProfileKey == ProfileKey;
}

bool ARpgBaseCampActor::IsBaseOwner(const AActor* RequestingActor) const
{
	const APlayerController* Controller = ResolvePlayerController(RequestingActor);
	return Controller && !OwnerProfileKey.IsEmpty() &&
		ResolveProfileKey(Controller) == OwnerProfileKey;
}

ARpgPersonalStorageLockerActor* ARpgBaseCampActor::GetOrCreatePersonalLocker(
	APlayerController* Controller)
{
	if (!HasAuthority() || !Controller || BaseId.IsNone())
	{
		return nullptr;
	}

	const FString ProfileKey = ResolveProfileKey(Controller);
	if (ProfileKey.IsEmpty())
	{
		return nullptr;
	}

	if (TWeakObjectPtr<ARpgPersonalStorageLockerActor>* Existing =
			PersonalLockersByProfile.Find(ProfileKey))
	{
		if (ARpgPersonalStorageLockerActor* ExistingLocker = Existing->Get())
		{
			ExistingLocker->ReassignOwningController(Controller);
			return ExistingLocker;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Controller;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgPersonalStorageLockerActor* Locker = GetWorld()->SpawnActor<
		ARpgPersonalStorageLockerActor>(
		ARpgPersonalStorageLockerActor::StaticClass(),
		GetActorTransform(),
		SpawnParams);
	if (!Locker)
	{
		return nullptr;
	}

	Locker->InitializeLocker(this, Controller, ProfileKey);
	PersonalLockersByProfile.Add(ProfileKey, Locker);
	if (FRpgInventoryGraphSaveData* PendingGraph =
			PendingPersonalLockerGraphs.Find(ProfileKey))
	{
		FRpgInventoryMutationResult RestoreResult;
		if (!Locker->GetInventoryManager()->RestoreInventoryGraph(
				*PendingGraph,
				RestoreResult))
		{
			Locker->Destroy();
			PersonalLockersByProfile.Remove(ProfileKey);
			return nullptr;
		}
		PendingPersonalLockerGraphs.Remove(ProfileKey);
	}
	if (ARpgGameModeBase* GameMode =
			GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
	{
		GameMode->MarkBaseStorageSaveDirty(this);
	}
	return Locker;
}

ARpgPersonalStorageLockerActor* ARpgBaseCampActor::FindPersonalLocker(
	APlayerController* Controller) const
{
	if (!Controller)
	{
		return nullptr;
	}

	if (HasAuthority())
	{
		const FString ProfileKey = ResolveProfileKey(Controller);
		if (const TWeakObjectPtr<ARpgPersonalStorageLockerActor>* Existing =
				PersonalLockersByProfile.Find(ProfileKey))
		{
			return Existing->Get();
		}
	}

	for (TActorIterator<ARpgPersonalStorageLockerActor> It(GetWorld()); It; ++It)
	{
		ARpgPersonalStorageLockerActor* Locker = *It;
		if (Locker && Locker->GetBaseId() == BaseId &&
			Locker->GetOwner() == Controller)
		{
			return Locker;
		}
	}
	return nullptr;
}

bool ARpgBaseCampActor::IsContainmentItemStabilized(
	FRpgInventoryItemId ItemId) const
{
	if (!ItemId.IsValid())
	{
		return false;
	}

	const auto FindInInventory =
		[ItemId](const URpgInventoryManagerComponent* Inventory)
			-> const URpgInventoryItemInstance*
		{
			return Inventory ? Inventory->FindItemById(ItemId) : nullptr;
		};
	if (const URpgInventoryItemInstance* Item =
		FindInInventory(ContainmentInventoryComponent))
	{
		return Item->IsContainmentStabilized();
	}
	if (const URpgInventoryItemInstance* Item =
		FindInInventory(ArmoryInventoryComponent))
	{
		return Item->IsContainmentStabilized();
	}
	for (const TPair<FString, TWeakObjectPtr<ARpgPersonalStorageLockerActor>>& Pair :
		PersonalLockersByProfile)
	{
		const ARpgPersonalStorageLockerActor* Locker = Pair.Value.Get();
		if (const URpgInventoryItemInstance* Item = FindInInventory(
				Locker ? Locker->GetInventoryManager() : nullptr))
		{
			return Item->IsContainmentStabilized();
		}
	}
	return false;
}

bool ARpgBaseCampActor::SetContainmentItemStabilized(
	FRpgInventoryItemId ItemId,
	bool bStabilized)
{
	if (!HasAuthority() || !ItemId.IsValid() ||
		(BaseStorageComponent && BaseStorageComponent->IsMutationTainted()) ||
		!ContainmentInventoryComponent->FindItemById(ItemId))
	{
		return false;
	}

	URpgInventoryItemInstance* Item =
		ContainmentInventoryComponent->FindItemById(ItemId);
	if (!Item)
	{
		return false;
	}
	const ERpgInventoryContainmentState NewState = bStabilized
		? ERpgInventoryContainmentState::Stabilized
		: ERpgInventoryContainmentState::Unstable;
	if (Item->GetContainmentState() == NewState)
	{
		return true;
	}
	if (!Item->SetContainmentState(NewState))
	{
		return false;
	}
	ContainmentStates.Reset();
	ForceNetUpdate();
	if (BaseStorageComponent)
	{
		BaseStorageComponent->NotifyExternalStorageStateMutation();
	}
	return true;
}

bool ARpgBaseCampActor::ForgetContainmentItemState(
	FRpgInventoryItemId ItemId)
{
	if (!HasAuthority() || !ItemId.IsValid() ||
		(BaseStorageComponent && BaseStorageComponent->IsMutationTainted()) ||
		!ContainmentInventoryComponent)
	{
		return false;
	}

	// Stabilization belongs to the concrete item runtime payload and deliberately survives leaving the Vault.
	// Removing a legacy shadow row is only a V2 migration cleanup and never resets the item instance.
	ContainmentStates.RemoveAll(
		[ItemId](const FRpgBaseContainmentItemStateSaveData& Candidate)
		{
			return Candidate.ItemId == ItemId;
		});
	return true;
}

bool ARpgBaseCampActor::ExportBaseStorageSaveData(
	FRpgBaseStorageSaveData& OutSaveData,
	FString& OutError) const
{
	OutError.Reset();
	if (BaseId.IsNone() || !BaseStorageComponent || !ArmoryInventoryComponent ||
		!ContainmentInventoryComponent)
	{
		OutError = TEXT("Base actor has no stable id or required storage components.");
		return false;
	}

	OutSaveData = FRpgBaseStorageSaveData();
	OutSaveData.BaseId = BaseId;
	OutSaveData.OwnerProfileKey = OwnerProfileKey;
	BaseStorageComponent->ExportStorageState(OutSaveData);
	OutSaveData.ContainmentStates.Reset();
	for (const FRpgInventoryEntryView& Entry :
		ContainmentInventoryComponent->GetAllEntries())
	{
		if (!Entry.Instance || !Entry.ItemId.IsValid())
		{
			continue;
		}
		FRpgBaseContainmentItemStateSaveData& State =
			OutSaveData.ContainmentStates.AddDefaulted_GetRef();
		State.ItemId = Entry.ItemId;
		State.bStabilized = Entry.Instance->IsContainmentStabilized();
	}

	OutSaveData.ArmoryGraph = ArmoryInventoryComponent->ExportInventoryGraph();
	OutSaveData.bHasArmoryGraph =
		OutSaveData.ArmoryGraph.Items.Num() ==
		ArmoryInventoryComponent->GetAllEntries().Num();
	OutSaveData.ContainmentGraph =
		ContainmentInventoryComponent->ExportInventoryGraph();
	OutSaveData.bHasContainmentGraph =
		OutSaveData.ContainmentGraph.Items.Num() ==
		ContainmentInventoryComponent->GetAllEntries().Num();
	if (!OutSaveData.bHasArmoryGraph || !OutSaveData.bHasContainmentGraph)
	{
		OutError = TEXT("A base inventory graph could not be exported completely.");
		return false;
	}

	OutSaveData.PersonalLockerGraphs = PendingPersonalLockerGraphs;
	for (const TPair<FString, TWeakObjectPtr<ARpgPersonalStorageLockerActor>>& Pair :
		PersonalLockersByProfile)
	{
		const ARpgPersonalStorageLockerActor* Locker = Pair.Value.Get();
		const URpgInventoryManagerComponent* LockerInventory =
			Locker ? Locker->GetInventoryManager() : nullptr;
		if (!LockerInventory)
		{
			continue;
		}
		const FRpgInventoryGraphSaveData Graph =
			LockerInventory->ExportInventoryGraph();
		if (Graph.Items.Num() != LockerInventory->GetAllEntries().Num())
		{
			OutError = FString::Printf(
				TEXT("Personal locker graph export failed for profile '%s'."),
				*Pair.Key);
			return false;
		}
		OutSaveData.PersonalLockerGraphs.Add(Pair.Key, Graph);
	}
	return true;
}

bool ARpgBaseCampActor::RestoreBaseStorageSaveData(
	const FRpgBaseStorageSaveData& SaveData,
	FString& OutError)
{
	OutError.Reset();
	if (bStorageRestoreTainted)
	{
		OutError = TEXT("Base storage was tainted by an earlier failed rollback.");
		return false;
	}
	if (!HasAuthority() || SaveData.BaseId != BaseId ||
		!BaseStorageComponent || !ArmoryInventoryComponent ||
		!ContainmentInventoryComponent)
	{
		OutError = TEXT("Base restore target or authority is invalid.");
		return false;
	}

	FRpgBaseStorageSaveData PreviousStorage;
	BaseStorageComponent->ExportStorageState(PreviousStorage);
	const FString PreviousOwner = OwnerProfileKey;
	const TArray<FRpgBaseContainmentItemStateSaveData> PreviousStates =
		ContainmentStates;
	const FRpgInventoryGraphSaveData PreviousArmory =
		ArmoryInventoryComponent->ExportInventoryGraph();
	const FRpgInventoryGraphSaveData PreviousContainment =
		ContainmentInventoryComponent->ExportInventoryGraph();
	if (PreviousArmory.Items.Num() !=
			ArmoryInventoryComponent->GetAllEntries().Num() ||
		PreviousContainment.Items.Num() !=
			ContainmentInventoryComponent->GetAllEntries().Num())
	{
		bStorageRestoreTainted = true;
		BaseStorageComponent->TaintAfterRollbackFailure();
		OutError = TEXT("Base restore cannot capture a complete pre-candidate inventory checkpoint.");
		return false;
	}
	const TMap<FString, FRpgInventoryGraphSaveData>
		PreviousPendingPersonalLockerGraphs = PendingPersonalLockerGraphs;

	auto Rollback = [&]()
	{
		bool bRolledBack = ArmoryInventoryComponent->ExpandDefaultGridToMinimum(
			GetMinimumPreservedRootGrid(
				PreviousArmory,
				ArmoryInventoryComponent));
		bRolledBack = ContainmentInventoryComponent->ExpandDefaultGridToMinimum(
			GetMinimumPreservedRootGrid(
				PreviousContainment,
				ContainmentInventoryComponent)) && bRolledBack;
		ArmoryInventoryComponent->SetFixedMaxEntries(FMath::Max(
			ArmoryInventoryComponent->GetFixedMaxEntries(),
			PreviousArmory.Items.Num()));
		ContainmentInventoryComponent->SetFixedMaxEntries(FMath::Max(
			ContainmentInventoryComponent->GetFixedMaxEntries(),
			PreviousContainment.Items.Num()));
		FRpgInventoryMutationResult IgnoredResult;
		const bool bArmoryRestored = ArmoryInventoryComponent->RestoreRuntimeCheckpoint(
			PreviousArmory,
			IgnoredResult) && IgnoredResult.IsSuccess();
		const bool bContainmentRestored = ContainmentInventoryComponent->RestoreRuntimeCheckpoint(
			PreviousContainment,
			IgnoredResult) && IgnoredResult.IsSuccess();
		FString IgnoredStorageError;
		const bool bStorageRestored = BaseStorageComponent->RestoreStorageState(
			PreviousStorage,
			IgnoredStorageError);
		OwnerProfileKey = PreviousOwner;
		ContainmentStates = PreviousStates;
		PendingPersonalLockerGraphs =
			PreviousPendingPersonalLockerGraphs;
		bRolledBack = bRolledBack && bArmoryRestored &&
			bContainmentRestored && bStorageRestored;
		if (!bRolledBack)
		{
			bStorageRestoreTainted = true;
			BaseStorageComponent->TaintAfterRollbackFailure();
		}
		return bRolledBack;
	};

	if (!BaseStorageComponent->RestoreStorageState(SaveData, OutError))
	{
		if (!Rollback())
		{
			OutError += TEXT(" Exact pre-candidate rollback failed.");
		}
		return false;
	}

	const int32 ConfiguredArmoryEntryBudget =
		ArmoryInventoryComponent->GetFixedMaxEntries();
	const int32 ConfiguredContainmentEntryBudget =
		ContainmentInventoryComponent->GetFixedMaxEntries();
	if (SaveData.bHasArmoryGraph)
	{
		ArmoryInventoryComponent->ExpandDefaultGridToMinimum(
			GetMinimumPreservedRootGrid(
				SaveData.ArmoryGraph,
				ArmoryInventoryComponent));
		ArmoryInventoryComponent->SetFixedMaxEntries(FMath::Max(
			ConfiguredArmoryEntryBudget,
			SaveData.ArmoryGraph.Items.Num()));
	}
	if (SaveData.bHasContainmentGraph)
	{
		ContainmentInventoryComponent->ExpandDefaultGridToMinimum(
			GetMinimumPreservedRootGrid(
				SaveData.ContainmentGraph,
				ContainmentInventoryComponent));
		ContainmentInventoryComponent->SetFixedMaxEntries(FMath::Max(
			ConfiguredContainmentEntryBudget,
			SaveData.ContainmentGraph.Items.Num()));
	}

	FRpgInventoryMutationResult RestoreResult;
	if (SaveData.bHasArmoryGraph &&
		!ArmoryInventoryComponent->RestoreInventoryGraph(
			SaveData.ArmoryGraph,
			RestoreResult))
	{
		OutError = TEXT("Armory graph restore failed.");
		if (!Rollback())
		{
			OutError += TEXT(" Exact pre-candidate rollback failed.");
		}
		return false;
	}
	if (SaveData.bHasContainmentGraph &&
		!ContainmentInventoryComponent->RestoreInventoryGraph(
			SaveData.ContainmentGraph,
			RestoreResult))
	{
		OutError = TEXT("Containment graph restore failed.");
		if (!Rollback())
		{
			OutError += TEXT(" Exact pre-candidate rollback failed.");
		}
		return false;
	}

	for (const FRpgBaseContainmentItemStateSaveData& State :
		SaveData.ContainmentStates)
	{
		URpgInventoryItemInstance* Item = State.ItemId.IsValid()
			? ContainmentInventoryComponent->FindItemById(State.ItemId)
			: nullptr;
		if (!Item || !Item->SetContainmentState(
				State.bStabilized
					? ERpgInventoryContainmentState::Stabilized
					: ERpgInventoryContainmentState::Unstable))
		{
			OutError = TEXT("Containment state references an incompatible item outside the restored graph.");
			if (!Rollback())
			{
				OutError += TEXT(" Exact pre-candidate rollback failed.");
			}
			return false;
		}
	}

	URpgInventoryManagerComponent* LockerValidationInventory =
		NewObject<URpgInventoryManagerComponent>(this);
	if (!LockerValidationInventory)
	{
		OutError = TEXT("Could not allocate the personal-locker validation inventory.");
		if (!Rollback())
		{
			OutError += TEXT(" Exact pre-candidate rollback failed.");
		}
		return false;
	}
	LockerValidationInventory->RegisterComponent();
	LockerValidationInventory->SetReplicationPolicy(
		ERpgInventoryReplicationPolicy::OwnerOnly);
	LockerValidationInventory->SetCapacityMode(
		ERpgInventoryCapacityMode::FixedEntries);
	LockerValidationInventory->SetFixedMaxEntries(20);
	FRpgInventoryGridSize LockerGridSize;
	LockerGridSize.Width = 4;
	LockerGridSize.Height = 5;
	const bool bLockerGridConfigured =
		LockerValidationInventory->SetDefaultGridSize(LockerGridSize);
	bool bLockerGraphsValid = bLockerGridConfigured;
	for (const TPair<FString, FRpgInventoryGraphSaveData>& Pair :
		SaveData.PersonalLockerGraphs)
	{
		FRpgInventoryMutationResult LockerValidationResult;
		if (Pair.Key.IsEmpty() ||
			!LockerValidationInventory->RestoreInventoryGraph(
				Pair.Value,
				LockerValidationResult))
		{
			bLockerGraphsValid = false;
			break;
		}
	}
	LockerValidationInventory->DestroyComponent();
	if (!bLockerGraphsValid)
	{
		OutError = TEXT("A personal-locker graph is incompatible with the private 4x5 inventory contract.");
		if (!Rollback())
		{
			OutError += TEXT(" Exact pre-candidate rollback failed.");
		}
		return false;
	}

	OwnerProfileKey = SaveData.OwnerProfileKey;
	ContainmentStates.Reset();
	PendingPersonalLockerGraphs = SaveData.PersonalLockerGraphs;
	ArmoryInventoryComponent->SetFixedMaxEntries(
		ConfiguredArmoryEntryBudget);
	ContainmentInventoryComponent->SetFixedMaxEntries(
		ConfiguredContainmentEntryBudget);
	BaseStorageComponent->RefreshConcreteDomainOverCapacityState();
	RefreshStorageAnchorVisuals();
	ForceNetUpdate();
	return true;
}

void ARpgBaseCampActor::RefreshStorageAnchorVisuals()
{
	if (!HasAuthority() || !BaseStorageComponent)
	{
		return;
	}

	FRpgBaseStorageDomainAnchorVisualState MaterialState;
	const int32 MaterialCapacity = BaseStorageComponent->GetMaterialCapacityPoints();
	MaterialState.FillRatio = MaterialCapacity > 0
		? FMath::Clamp(
			static_cast<float>(BaseStorageComponent->GetUsedMaterialCapacityPoints()) /
				MaterialCapacity,
			0.0f,
			1.0f)
		: 0.0f;
	MaterialState.Status = BaseStorageComponent->IsMaterialDomainOverCapacity()
		? ERpgBaseStorageDomainAnchorVisualStatus::Critical
		: (MaterialState.FillRatio >= 0.8f
			? ERpgBaseStorageDomainAnchorVisualStatus::Strained
			: (MaterialState.FillRatio > 0.0f
				? ERpgBaseStorageDomainAnchorVisualStatus::Active
				: ERpgBaseStorageDomainAnchorVisualStatus::Ready));
	if (MaterialDepotAnchor)
	{
		MaterialDepotAnchor->SetVisualState(MaterialState);
	}

	FRpgBaseStorageDomainAnchorVisualState ArmoryState;
	const FRpgInventoryGridSize ArmoryGrid = ArmoryInventoryComponent
		? ArmoryInventoryComponent->GetDefaultGridSize()
		: FRpgInventoryGridSize();
	const int32 ArmoryCells = ArmoryGrid.Width * ArmoryGrid.Height;
	ArmoryState.FillRatio = ArmoryInventoryComponent && ArmoryCells > 0
		? FMath::Clamp(
			static_cast<float>(ArmoryInventoryComponent->GetUsedEntryCount()) /
				ArmoryCells,
			0.0f,
			1.0f)
		: 0.0f;
	ArmoryState.Status = BaseStorageComponent->IsArmoryDomainOverCapacity()
		? ERpgBaseStorageDomainAnchorVisualStatus::Critical
		: (ArmoryState.FillRatio >= 0.8f
			? ERpgBaseStorageDomainAnchorVisualStatus::Strained
			: (ArmoryState.FillRatio > 0.0f
				? ERpgBaseStorageDomainAnchorVisualStatus::Active
				: ERpgBaseStorageDomainAnchorVisualStatus::Ready));
	if (ArmoryAnchor)
	{
		ArmoryAnchor->SetVisualState(ArmoryState);
	}

	FRpgBaseStorageDomainAnchorVisualState RiftState;
	const int32 RiftSlots = BaseStorageComponent->GetContainmentSlotCapacity();
	RiftState.FillRatio = ContainmentInventoryComponent && RiftSlots > 0
		? FMath::Clamp(
			static_cast<float>(ContainmentInventoryComponent->GetUsedEntryCount()) /
				RiftSlots,
			0.0f,
			1.0f)
		: 0.0f;
	RiftState.StrainRatio =
		FMath::Clamp(BaseStorageComponent->GetRiftStrain() / 100.0f, 0.0f, 1.0f);
	if (!BaseStorageComponent->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftContainment) || RiftSlots <= 0)
	{
		RiftState.Status = ERpgBaseStorageDomainAnchorVisualStatus::Offline;
	}
	else if (BaseStorageComponent->IsContainmentDomainOverCapacity() ||
		RiftState.StrainRatio >= 1.0f)
	{
		RiftState.Status = ERpgBaseStorageDomainAnchorVisualStatus::Critical;
	}
	else if (RiftState.StrainRatio >= 0.8f || RiftState.FillRatio >= 0.8f)
	{
		RiftState.Status = ERpgBaseStorageDomainAnchorVisualStatus::Strained;
	}
	else
	{
		RiftState.Status = ContainmentInventoryComponent &&
			ContainmentInventoryComponent->GetUsedEntryCount() > 0
			? ERpgBaseStorageDomainAnchorVisualStatus::Active
			: ERpgBaseStorageDomainAnchorVisualStatus::Ready;
	}
	if (RiftVaultAnchor)
	{
		RiftVaultAnchor->SetVisualState(RiftState);
	}
}

bool ARpgBaseCampActor::CanPlaceBuildableAtTransform(const URpgBaseBuildableDefinition* BuildableDefinition, const FTransform& BuildTransform, const AActor* RequestingActor) const
{
	if (!BuildableDefinition || !BuildableDefinition->BuildActorClass)
	{
		return false;
	}

	if (!BuildableDefinition->RequiredUnlockTags.IsEmpty() && !GetGrantedStorageUpgradeTags().HasAllExact(BuildableDefinition->RequiredUnlockTags))
	{
		return false;
	}

	const float EffectiveBaseRadius = BuildableDefinition->MaxPlacementDistanceFromBase > 0.0f
		? BuildableDefinition->MaxPlacementDistanceFromBase
		: BuildRadius;
	if (EffectiveBaseRadius > 0.0f &&
		FVector::DistSquared(GetActorLocation(), BuildTransform.GetLocation()) > FMath::Square(EffectiveBaseRadius))
	{
		return false;
	}

	if (RequestingActor && BuildableDefinition->MaxPlacementDistanceFromBuilder > 0.0f &&
		FVector::DistSquared(RequestingActor->GetActorLocation(), BuildTransform.GetLocation()) > FMath::Square(BuildableDefinition->MaxPlacementDistanceFromBuilder))
	{
		return false;
	}

	return true;
}

void ARpgBaseCampActor::RegisterStorageStation(URpgBaseStorageStationComponent* Station)
{
	if (!Station)
	{
		return;
	}

	RegisteredStorageStations.RemoveAll([](const TWeakObjectPtr<URpgBaseStorageStationComponent>& ExistingStation)
	{
		return !ExistingStation.IsValid();
	});
	RegisteredStorageStations.AddUnique(Station);
}

void ARpgBaseCampActor::UnregisterStorageStation(URpgBaseStorageStationComponent* Station)
{
	RegisteredStorageStations.RemoveAll([Station](const TWeakObjectPtr<URpgBaseStorageStationComponent>& ExistingStation)
	{
		return !ExistingStation.IsValid() || ExistingStation.Get() == Station;
	});
}

TArray<URpgBaseStorageStationComponent*> ARpgBaseCampActor::GetStorageStations() const
{
	TArray<URpgBaseStorageStationComponent*> Results;
	for (const TWeakObjectPtr<URpgBaseStorageStationComponent>& StationPtr : RegisteredStorageStations)
	{
		if (URpgBaseStorageStationComponent* Station = StationPtr.Get())
		{
			Results.Add(Station);
		}
	}
	return Results;
}

FGameplayTagContainer ARpgBaseCampActor::GetGrantedStorageUpgradeTags() const
{
	FGameplayTagContainer GrantedTags;
	if (BaseStorageComponent)
	{
		for (const URpgBaseStorageUpgradeDefinition* Upgrade :
			BaseStorageComponent->GetInstalledUpgrades())
		{
			if (Upgrade)
			{
				GrantedTags.AppendTags(Upgrade->GrantedUpgradeTags);
			}
		}
	}
	for (const TWeakObjectPtr<URpgBaseStorageStationComponent>& StationPtr : RegisteredStorageStations)
	{
		if (const URpgBaseStorageStationComponent* Station = StationPtr.Get())
		{
			GrantedTags.AppendTags(Station->GetGrantedUpgradeTags());
		}
	}
	return GrantedTags;
}

bool ARpgBaseCampActor::HasStorageUpgradeTag(FGameplayTag UpgradeTag) const
{
	return UpgradeTag.IsValid() && GetGrantedStorageUpgradeTags().HasTagExact(UpgradeTag);
}

FString ARpgBaseCampActor::ResolveProfileKey(
	const APlayerController* Controller) const
{
	if (!Controller || !GetWorld())
	{
		return FString();
	}

	const ARpgGameModeBase* GameMode =
		GetWorld()->GetAuthGameMode<ARpgGameModeBase>();
	return GameMode ? GameMode->GetPlayerProfileKey(Controller) : FString();
}

APlayerController* ARpgBaseCampActor::ResolvePlayerController(
	const AActor* RequestingActor) const
{
	if (!RequestingActor)
	{
		return nullptr;
	}
	if (APlayerController* Controller =
		Cast<APlayerController>(const_cast<AActor*>(RequestingActor)))
	{
		return Controller;
	}
	if (const APawn* Pawn = Cast<APawn>(RequestingActor))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	return Cast<APlayerController>(RequestingActor->GetOwner());
}

void ARpgBaseCampActor::DestroyPersonalLockers()
{
	for (const TPair<FString, TWeakObjectPtr<ARpgPersonalStorageLockerActor>>& Pair :
		PersonalLockersByProfile)
	{
		if (ARpgPersonalStorageLockerActor* Locker = Pair.Value.Get())
		{
			Locker->Destroy();
		}
	}
	PersonalLockersByProfile.Reset();
}
