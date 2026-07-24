#include "RpgInventoryUiActionComponent.h"
#include "RpgInventoryUiActionDomainHandlers.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

DEFINE_LOG_CATEGORY(LogRpgInventoryUiActions);

namespace
{
	bool IsExactUiActionPlacementSnapshot(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	bool IsReplayEpochCurrent(
		const TWeakObjectPtr<URpgInventoryManagerComponent>& Inventory,
		bool bHadInventory,
		uint64 ExpectedEpoch)
	{
		if (!bHadInventory)
		{
			return true;
		}

		const URpgInventoryManagerComponent* CurrentInventory =
			Inventory.Get();
		return CurrentInventory &&
			CurrentInventory->GetMutationEpoch() == ExpectedEpoch;
	}

	FGameplayTag GetActionTagForEquipmentIntent(
		ERpgInventoryEquipmentIntentOperation Operation)
	{
		switch (Operation)
		{
		case ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate:
			return RpgGameplayTags::
				Rpg_Inventory_Action_EquipAndActivate;
		case ERpgInventoryEquipmentIntentOperation::MoveToCarry:
			return RpgGameplayTags::Rpg_Inventory_Action_MoveToCarry;
		case ERpgInventoryEquipmentIntentOperation::EquipToSlot:
		case ERpgInventoryEquipmentIntentOperation::UnequipToContent:
		case ERpgInventoryEquipmentIntentOperation::ClearActiveSelection:
		default:
			return RpgGameplayTags::Rpg_Inventory_Action_Equip;
		}
	}
}

URpgInventoryUiActionComponent::URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ManualDropActorClass = ARpgDroppedInventoryActor::StaticClass();
}

void URpgInventoryUiActionComponent::RequestInventoryMutation_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMutationRequest Request)
{
	FRpgInventoryTransactionActionHandler(*this).InventoryMutation(
		Inventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestMoveInventoryItem_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryMoveIntent Intent)
{
	FRpgInventoryTransactionActionHandler(*this).MoveInventoryItem(
		Inventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestTransferInventoryItem_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryTransferIntent Intent)
{
	FRpgInventoryTransactionActionHandler(*this).TransferInventoryItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestExecuteInventoryItemAction_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemActionRequest Request)
{
	FRpgInventoryItemUseActionHandler(*this).ExecuteItemAction(
		Inventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::
	RequestApplyInventoryEquipmentIntent_Implementation(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryEquipmentIntent Intent)
{
	FRpgInventoryEquipmentActionHandler(*this).ApplyInventoryEquipmentIntent(
		Inventory,
		MoveTemp(Intent));
}

void URpgInventoryUiActionComponent::RequestQuickTransferItem_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	FRpgInventoryTransactionActionHandler(*this).QuickTransferItem(
		SourceInventory,
		TargetInventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	FRpgInventoryEquipmentActionHandler(*this).AssignItemToEquipmentSlot(
		EquipmentSlot,
		Item);
}

void URpgInventoryUiActionComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	FRpgInventoryEquipmentActionHandler(*this).ClearEquipmentSlot(EquipmentSlot);
}

void URpgInventoryUiActionComponent::RequestTransferItemStack_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	FRpgInventoryTransactionActionHandler(*this).TransferItemStack(
		SourceInventory,
		TargetInventory,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestTransferItemStackToPlacement_Implementation(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	FRpgInventoryGridPlacement TargetPlacement)
{
	FRpgInventoryTransactionActionHandler(*this).
		TransferItemStackToPlacement(
			SourceInventory,
			TargetInventory,
			Item,
			StackCount,
			TargetPlacement);
}

void URpgInventoryUiActionComponent::RequestApplyInventorySort_Implementation(
	URpgInventoryManagerComponent* Inventory,
	ERpgInventorySortMode SortMode)
{
	FRpgInventoryTransactionActionHandler(*this).ApplyInventorySort(
		Inventory,
		SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntry_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FGuid EntryId,
	int32 TargetIndex)
{
	FRpgInventoryTransactionActionHandler(*this).MoveInventoryEntry(
		Inventory,
		EntryId,
		TargetIndex);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntryToPlacement_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FGuid EntryId,
	FRpgInventoryGridPlacement TargetPlacement)
{
	FRpgInventoryTransactionActionHandler(*this).
		MoveInventoryEntryToPlacement(
			Inventory,
			EntryId,
			TargetPlacement);
}

void URpgInventoryUiActionComponent::RequestMoveItemToInventorySlotAddress_Implementation(URpgInventoryItemInstance* Item, FRpgInventorySlotAddress TargetAddress)
{
	FRpgInventoryEquipmentActionHandler(*this).MoveItemToInventorySlotAddress(
		Item,
		TargetAddress);
}

void URpgInventoryUiActionComponent::RequestEquipSlotContainerItem_Implementation(ERpgEquipmentSlot ContainerSlot, URpgInventoryItemInstance* Item)
{
	FRpgInventoryEquipmentActionHandler(*this).EquipSlotContainerItem(
		ContainerSlot,
		Item);
}

void URpgInventoryUiActionComponent::RequestUnequipSlotContainerItem_Implementation(
	ERpgEquipmentSlot ContainerSlot,
	FRpgInventoryItemId ExpectedProviderItemId)
{
	FRpgInventoryEquipmentActionHandler(*this).UnequipSlotContainerItem(
		ContainerSlot,
		ExpectedProviderItemId);
}

void URpgInventoryUiActionComponent::RequestActivateCarrySlot_Implementation(
	int32 ActionBarSlotIndex,
	FGameplayTag ExpectedCarrySemanticRole)
{
	FRpgInventoryQuickAccessActionHandler(*this).ActivateCarrySlot(
		ActionBarSlotIndex,
		ExpectedCarrySemanticRole);
}

void URpgInventoryUiActionComponent::RequestClearActiveHands_Implementation()
{
	FRpgInventoryQuickAccessActionHandler(*this).ClearActiveHands();
}

void URpgInventoryUiActionComponent::RequestMutateQuickAccessBinding_Implementation(FRpgQuickAccessMutationRequest Request)
{
	FRpgInventoryQuickAccessActionHandler(*this).MutateBinding(
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestBindActionBarToInventorySlot_Implementation(int32 ActionBarSlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	FRpgInventoryQuickAccessActionHandler(*this).BindInventorySlot(
		ActionBarSlotIndex,
		SlotAddress);
}

void URpgInventoryUiActionComponent::RequestBindActionBarToCarrySlot_Implementation(int32 ActionBarSlotIndex, FRpgInventorySlotAddress CarrySlotAddress)
{
	FRpgInventoryQuickAccessActionHandler(*this).BindCarrySlot(
		ActionBarSlotIndex,
		CarrySlotAddress);
}

void URpgInventoryUiActionComponent::RequestClearActionBarCarryBinding_Implementation(
	int32 ActionBarSlotIndex,
	FGameplayTag ExpectedCarrySemanticRole)
{
	FRpgInventoryQuickAccessActionHandler(*this).ClearCarryBinding(
		ActionBarSlotIndex,
		ExpectedCarrySemanticRole);
}

void URpgInventoryUiActionComponent::RequestClearActionBarConsumableBinding_Implementation(
	int32 ActionBarSlotIndex,
	TSubclassOf<URpgInventoryItemDefinition> ExpectedConsumableDefinition)
{
	FRpgInventoryQuickAccessActionHandler(*this).ClearConsumableBinding(
		ActionBarSlotIndex,
		ExpectedConsumableDefinition);
}

void URpgInventoryUiActionComponent::RequestSplitItemStack_Implementation(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement)
{
	FRpgInventoryTransactionActionHandler(*this).SplitItemStack(
		Inventory,
		Item,
		SplitCount,
		TargetPlacement);
}

void URpgInventoryUiActionComponent::RequestSplitItemStackById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemId ItemId,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	FGuid RequestId)
{
	FRpgInventoryTransactionActionHandler(*this).SplitItemStackById(
		Inventory,
		ItemId,
		SplitCount,
		TargetPlacement,
		RequestId);
}

void URpgInventoryUiActionComponent::RequestUseInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgInventoryItemUseActionHandler(*this).UseInventoryItem(
		Inventory,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestEquipInventoryItem_Implementation(URpgInventoryItemInstance* Item)
{
	FRpgInventoryEquipmentActionHandler(*this).EquipInventoryItem(Item);
}

void URpgInventoryUiActionComponent::RequestUnequipInventoryItemToContentSlot_Implementation(URpgInventoryItemInstance* Item)
{
	FRpgInventoryEquipmentActionHandler(*this).UnequipInventoryItemToContentSlot(Item);
}

void URpgInventoryUiActionComponent::RequestDropInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount, bool bConfirmed)
{
	FRpgInventoryManualDropActionHandler(*this).DropInventoryItem(
		Inventory,
		Item,
		StackCount,
		bConfirmed);
}

void URpgInventoryUiActionComponent::RequestDropInventoryItemById_Implementation(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryManualDropRequest Request)
{
	FRpgInventoryManualDropActionHandler(*this).DropInventoryItemById(
		Inventory,
		MoveTemp(Request));
}

void URpgInventoryUiActionComponent::RequestDepositAllMaterialsToBase_Implementation(URpgBaseStorageStationComponent* Station)
{
	FRpgBaseStorageActionHandler(*this).DepositAllMaterials(Station);
}

void URpgInventoryUiActionComponent::RequestDepositMaterialStackToBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).DepositMaterialStack(
		Station,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestWithdrawResourceFromBase_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).WithdrawResource(
		Station,
		ItemDefinition,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestStoreItemInstanceInBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).StoreItemInstance(
		Station,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestTakeItemInstanceFromBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	FRpgBaseStorageActionHandler(*this).TakeItemInstance(
		Station,
		Item,
		StackCount);
}

void URpgInventoryUiActionComponent::RequestInstallBaseStorageUpgrade_Implementation(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	FRpgBaseStorageActionHandler(*this).InstallUpgrade(
		Station,
		UpgradeDefinition);
}

void URpgInventoryUiActionComponent::RequestApplyBaseResourceSort_Implementation(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode)
{
	FRpgBaseStorageActionHandler(*this).ApplyResourceSort(
		Station,
		SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveBaseResourceEntry_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	FRpgBaseStorageActionHandler(*this).MoveResourceEntry(
		Station,
		ItemDefinition,
		TargetIndex);
}

void URpgInventoryUiActionComponent::RequestPlaceBaseBuildable_Implementation(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase)
{
	FRpgBaseBuildingActionHandler(*this).PlaceBuildable(
		BaseCamp,
		BuildableDefinition,
		BuildTransform,
		bAutoContributeFromBase);
}

void URpgInventoryUiActionComponent::RequestContributeAllToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage)
{
	FRpgBaseBuildingActionHandler(*this).ContributeAll(
		ConstructionSite,
		bAllowBaseStorage);
}

void URpgInventoryUiActionComponent::RequestContributeMaterialToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage)
{
	FRpgBaseBuildingActionHandler(*this).ContributeMaterial(
		ConstructionSite,
		ItemDefinition,
		StackCount,
		bAllowBaseStorage);
}

void URpgInventoryUiActionComponent::RequestCraftRecipe_Implementation(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	FRpgCraftingActionHandler(*this).CraftRecipe(
		CraftingStation,
		RecipeDefinition,
		Quantity);
}

void URpgInventoryUiActionComponent::RequestCancelCraftJob_Implementation(URpgCraftingStationComponent* CraftingStation, FGuid JobId)
{
	FRpgCraftingActionHandler(*this).CancelCraftJob(
		CraftingStation,
		JobId);
}

void URpgInventoryUiActionComponent::RequestPauseCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	FRpgCraftingActionHandler(*this).PauseStation(CraftingStation);
}

void URpgInventoryUiActionComponent::RequestResumeCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	FRpgCraftingActionHandler(*this).ResumeStation(CraftingStation);
}

void URpgInventoryUiActionComponent::RequestSetCraftingOutputAutoDepositEnabled_Implementation(URpgCraftingStationComponent* CraftingStation, bool bEnabled)
{
	FRpgCraftingActionHandler(*this).SetOutputAutoDepositEnabled(
		CraftingStation,
		bEnabled);
}

bool URpgInventoryUiActionComponent::CanAccessInventory(URpgInventoryManagerComponent* Inventory) const
{
	return FRpgInventoryUiActionDomainHandler(*this).
		EvaluateInventoryAccess(Inventory);
}

URpgInventoryManagerComponent* URpgInventoryUiActionComponent::FindPlayerInventory() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	return nullptr;
}

URpgEquipmentLoadoutComponent* URpgInventoryUiActionComponent::FindEquipmentLoadout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetEquipmentLoadoutComponent();
	}

	return nullptr;
}

URpgPlayerInventoryLayoutComponent* URpgInventoryUiActionComponent::FindPlayerInventoryLayout() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetPlayerInventoryLayoutComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>() : nullptr;
}

URpgActionBarComponent* URpgInventoryUiActionComponent::FindActionBar() const
{
	if (const ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetOwner()))
	{
		return PlayerController->GetActionBarComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<URpgActionBarComponent>() : nullptr;
}

URpgAbilitySystemComponent* URpgInventoryUiActionComponent::FindPlayerAbilitySystem() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetRpgAbilitySystemComponent();
		}
	}

	return nullptr;
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanExactTransferPlacement(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		PlanExactTransferPlacement(
			SourceInventory,
			TargetInventory,
			Intent);
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanEquipmentIntentPlacement(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryEquipmentIntent& Intent,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryEquipmentQueryHandler(*this).PlanEquipmentIntentPlacement(
		Inventory,
		Intent,
		OutTargetPlacement);
}

FRpgInventoryPlacementPlan
URpgInventoryUiActionComponent::PlanQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		PlanQuickTransferDestination(
			SourceInventory,
			TargetInventory,
			Request,
			OutTargetContainer,
			OutTargetPlacement);
}

bool URpgInventoryUiActionComponent::FindQuickTransferDestination(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	FRpgInventoryContainerHandle& OutTargetContainer,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		FindQuickTransferDestination(
			SourceInventory,
			TargetInventory,
			Request,
			OutTargetContainer,
			OutTargetPlacement);
}



bool URpgInventoryUiActionComponent::CanTransferItemStack(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		CanTransferItemStack(
			SourceInventory,
			TargetInventory,
			Item,
			StackCount);
}

bool URpgInventoryUiActionComponent::CanTransferItemStackToPlacement(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	FRpgInventoryGridPlacement TargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		CanTransferItemStackToPlacement(
			SourceInventory,
			TargetInventory,
			Item,
			StackCount,
			TargetPlacement);
}

bool URpgInventoryUiActionComponent::CanSplitItemStack(
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 SplitCount,
	FRpgInventoryGridPlacement TargetPlacement,
	int32& OutSplitCount,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryTransactionQueryHandler(*this).
		CanSplitItemStack(
			Inventory,
			Item,
			SplitCount,
			TargetPlacement,
			OutSplitCount,
			OutTargetPlacement);
}


bool URpgInventoryUiActionComponent::CanAccessBaseStorageStation(const URpgBaseStorageStationComponent* Station) const
{
	if (!Station)
	{
		return false;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	return Station->CanActorAccess(RequestingActor);
}

bool URpgInventoryUiActionComponent::CanMoveItemToFirstCompatibleContentSlot(
	URpgInventoryItemInstance* Item,
	FRpgInventoryGridPlacement& OutTargetPlacement) const
{
	return FRpgInventoryEquipmentQueryHandler(*this)
		.CanMoveItemToFirstCompatibleContentSlot(Item, OutTargetPlacement);
}

bool URpgInventoryUiActionComponent::IsPlayerEquipmentPlacement(
	const FRpgInventoryGridPlacement& Placement) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	FRpgInventorySlotAddress Address;
	return InventoryLayout && Placement.IsValid() &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(
			Placement,
			Address) &&
		(InventoryLayout->IsGearSlotAddress(Address) ||
			InventoryLayout->IsCarrySlotAddress(Address));
}

void URpgInventoryUiActionComponent::SyncEquipmentLoadoutFromGearSlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !PlayerInventory || !InventoryLayout)
	{
		return;
	}

	const ERpgEquipmentSlot PhysicalSlots[] =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet,
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	bool bPhysicalMirrorChanged = false;
	for (const ERpgEquipmentSlot EquipmentSlot : PhysicalSlots)
	{
		FRpgInventorySlotAddress Address;
		URpgInventoryItemInstance* PhysicalItem = nullptr;
		if (InventoryLayout->TryMakeGearSlotAddress(
				EquipmentSlot,
				Address))
		{
			PhysicalItem =
				InventoryLayout->GetItemInSlotAddress(Address);
		}

		if (EquipmentLoadout->GetItemInEquipmentSlot(
				EquipmentSlot) != PhysicalItem)
		{
			bPhysicalMirrorChanged = true;
			break;
		}
	}

	if (bPhysicalMirrorChanged)
	{
		// Rebuild once from the complete Gear snapshot so observers never see per-slot intermediate states.
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
		return;
	}

	// Carry changes affect load without changing the non-hand Gear mirror. Do not rebuild runtime actors or GAS grants.
	EquipmentLoadout->ReconcileEquipmentLoadFromInventory();
}

void URpgInventoryUiActionComponent::SyncActiveHandsFromCarrySlots() const
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!EquipmentLoadout || !InventoryLayout)
	{
		return;
	}

	auto IsItemInCarrySlot = [InventoryLayout](
		const URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot EquipmentSlotRole)
	{
		if (!Item)
		{
			return false;
		}

		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Carry ||
				Group.EquipmentSlotRole != EquipmentSlotRole)
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					if (InventoryLayout->GetItemInSlotAddress(Group.MakeAddress(X, Y)) == Item)
					{
						return true;
					}
				}
			}
		}

		return false;
	};

	if (URpgInventoryItemInstance* MainHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
		MainHandItem &&
		!IsItemInCarrySlot(
			MainHandItem,
			ERpgEquipmentSlot::MainHand))
	{
		EquipmentLoadout->ClearActiveMainHand();
	}

	if (URpgInventoryItemInstance* OffHandItem = EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
		OffHandItem &&
		!IsItemInCarrySlot(
			OffHandItem,
			ERpgEquipmentSlot::OffHand))
	{
		EquipmentLoadout->ClearActiveOffHand(true);
	}
}

bool URpgInventoryUiActionComponent::AreEquipmentIntentsEquivalent(
	const FRpgInventoryEquipmentIntent& A,
	const FRpgInventoryEquipmentIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedQuantity == B.ExpectedQuantity &&
		A.Operation == B.Operation &&
		A.TargetEquipmentSlot == B.TargetEquipmentSlot;
}

bool URpgInventoryUiActionComponent::
	TryReplayRecentEquipmentIntentResult(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentEquipmentIntentResult* CachedResult =
		RecentEquipmentIntentResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch))
	{
		RecentEquipmentIntentResults.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreEquipmentIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected equipment RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			GetActionTagForEquipmentIntent(Intent.Operation),
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			Inventory
				? Inventory->FindItemById(Intent.ItemId)
				: nullptr,
			Intent.ExpectedQuantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		GetActionTagForEquipmentIntent(
			CachedResult->Intent.Operation),
		CachedResult->Result,
		CachedInventory,
		CachedInventory
			? CachedInventory->FindItemById(
				CachedResult->Intent.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::
	SendAndCacheEquipmentIntentFeedback(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEquipmentIntent& Intent,
		ERpgInventoryActionFeedbackResult Result,
		URpgInventoryItemInstance* Item,
		int32 FeedbackStackCount)
{
	if (Intent.RequestId.IsValid())
	{
		FRecentEquipmentIntentResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentEquipmentIntentResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentEquipmentIntentOrder.Remove(Intent.RequestId);
		RecentEquipmentIntentOrder.Add(Intent.RequestId);
		while (RecentEquipmentIntentOrder.Num() >
			MaxRecentEquipmentIntentResults)
		{
			RecentEquipmentIntentResults.Remove(
				RecentEquipmentIntentOrder[0]);
			RecentEquipmentIntentOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		GetActionTagForEquipmentIntent(Intent.Operation),
		Result,
		Inventory,
		Item,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreExactTransferIntentsEquivalent(
	const FRpgInventoryTransferIntent& A,
	const FRpgInventoryTransferIntent& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.TargetContainer == B.TargetContainer &&
		A.TargetPlacement == B.TargetPlacement &&
		A.Quantity == B.Quantity;
}

bool URpgInventoryUiActionComponent::TryReplayRecentExactTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent)
{
	if (!Intent.RequestId.IsValid())
	{
		return false;
	}

	const FRecentExactTransferResult* CachedResult =
		RecentExactTransferResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentExactTransferResults.Remove(Intent.RequestId);
		RecentExactTransferOrder.Remove(Intent.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreExactTransferIntentsEquivalent(
			CachedResult->Intent,
			Intent))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected exact-transfer RequestId collision for %s."),
			*Intent.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			SourceInventory
				? SourceInventory->FindItemById(Intent.ItemId)
				: nullptr,
			Intent.Quantity,
			Intent.RequestId,
			Intent.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedSource
			? CachedSource->FindItemById(
				CachedResult->Intent.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Intent.RequestId,
		CachedResult->Intent.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheExactTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryTransferIntent& Intent,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	if (Intent.RequestId.IsValid())
	{
		FRecentExactTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Intent = Intent;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentExactTransferResults.Add(
			Intent.RequestId,
			MoveTemp(CachedResult));
		RecentExactTransferOrder.Remove(Intent.RequestId);
		RecentExactTransferOrder.Add(Intent.RequestId);
		while (RecentExactTransferOrder.Num() >
			MaxRecentExactTransferResults)
		{
			RecentExactTransferResults.Remove(
				RecentExactTransferOrder[0]);
			RecentExactTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		Item,
		FeedbackStackCount,
		Intent.RequestId,
		Intent.ItemId);
}

bool URpgInventoryUiActionComponent::AreQuickTransferRequestsEquivalent(
	const FRpgInventoryQuickTransferRequest& A,
	const FRpgInventoryQuickTransferRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.StackCount == B.StackCount &&
		A.PreferredTargetContainers == B.PreferredTargetContainers;
}

bool URpgInventoryUiActionComponent::TryReplayRecentQuickTransferResult(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentQuickTransferResult* CachedResult =
		RecentQuickTransferResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->SourceInventory,
			CachedResult->bHadSourceInventory,
			CachedResult->SourceMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentQuickTransferResults.Remove(Request.RequestId);
		RecentQuickTransferOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->SourceInventory.Get() != SourceInventory ||
		CachedResult->TargetInventory.Get() != TargetInventory ||
		!AreQuickTransferRequestsEquivalent(
			CachedResult->Request,
			Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected quick-transfer RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			SourceInventory,
			SourceInventory
				? SourceInventory->FindItemById(Request.ItemId)
				: nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedSource =
		CachedResult->SourceInventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		CachedResult->Result,
		CachedSource,
		CachedSource
			? CachedSource->FindItemById(
				CachedResult->Request.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheQuickTransferFeedback(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	const FRpgInventoryQuickTransferRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount)
{
	if (Request.RequestId.IsValid())
	{
		FRecentQuickTransferResult CachedResult;
		CachedResult.SourceInventory = SourceInventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadSourceInventory = SourceInventory != nullptr;
		CachedResult.bHadTargetInventory = TargetInventory != nullptr;
		CachedResult.SourceMutationEpoch = SourceInventory
			? SourceInventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentQuickTransferResults.Add(
			Request.RequestId,
			MoveTemp(CachedResult));
		RecentQuickTransferOrder.Remove(Request.RequestId);
		RecentQuickTransferOrder.Add(Request.RequestId);
		while (RecentQuickTransferOrder.Num() >
			MaxRecentQuickTransferResults)
		{
			RecentQuickTransferResults.Remove(
				RecentQuickTransferOrder[0]);
			RecentQuickTransferOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		Result,
		SourceInventory,
		Item,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

bool URpgInventoryUiActionComponent::AreManualDropRequestsEquivalent(
	const FRpgInventoryManualDropRequest& A,
	const FRpgInventoryManualDropRequest& B)
{
	return A.RequestId == B.RequestId &&
		A.EntryId == B.EntryId &&
		A.ItemId == B.ItemId &&
		IsExactUiActionPlacementSnapshot(
			A.ExpectedSourcePlacement,
			B.ExpectedSourcePlacement) &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.StackCount == B.StackCount &&
		A.bConfirmed == B.bConfirmed;
}

bool URpgInventoryUiActionComponent::TryReplayRecentManualDropResult(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request)
{
	if (!Request.RequestId.IsValid())
	{
		return false;
	}

	const FRecentManualDropResult* CachedResult =
		RecentManualDropResults.Find(Request.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	if (!IsReplayEpochCurrent(
			CachedResult->Inventory,
			CachedResult->bHadInventory,
			CachedResult->InventoryMutationEpoch) ||
		!IsReplayEpochCurrent(
			CachedResult->TargetInventory,
			CachedResult->bHadTargetInventory,
			CachedResult->TargetMutationEpoch))
	{
		RecentManualDropResults.Remove(Request.RequestId);
		RecentManualDropOrder.Remove(Request.RequestId);
		return false;
	}

	if (CachedResult->Inventory.Get() != Inventory ||
		!AreManualDropRequestsEquivalent(CachedResult->Request, Request))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Rejected manual-drop RequestId collision for %s."),
			*Request.RequestId.ToString());
		SendActionFeedback(
			RpgGameplayTags::Rpg_Inventory_Action_Drop,
			ERpgInventoryActionFeedbackResult::InvalidRequest,
			Inventory,
			Inventory ? Inventory->FindItemById(Request.ItemId) : nullptr,
			Request.StackCount,
			Request.RequestId,
			Request.ItemId);
		return true;
	}

	URpgInventoryManagerComponent* CachedInventory =
		CachedResult->Inventory.Get();
	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		CachedResult->Result,
		CachedInventory,
		CachedInventory
			? CachedInventory->FindItemById(CachedResult->Request.ItemId)
			: nullptr,
		CachedResult->FeedbackStackCount,
		CachedResult->Request.RequestId,
		CachedResult->Request.ItemId);
	return true;
}

void URpgInventoryUiActionComponent::SendAndCacheManualDropFeedback(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryManualDropRequest& Request,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryItemInstance* Item,
	int32 FeedbackStackCount,
	URpgInventoryManagerComponent* TargetInventory)
{
	if (Request.RequestId.IsValid())
	{
		FRecentManualDropResult CachedResult;
		CachedResult.Inventory = Inventory;
		CachedResult.TargetInventory = TargetInventory;
		CachedResult.bHadInventory = Inventory != nullptr;
		CachedResult.bHadTargetInventory =
			TargetInventory != nullptr;
		CachedResult.InventoryMutationEpoch = Inventory
			? Inventory->GetMutationEpoch()
			: 0;
		CachedResult.TargetMutationEpoch = TargetInventory
			? TargetInventory->GetMutationEpoch()
			: 0;
		CachedResult.Request = Request;
		CachedResult.Result = Result;
		CachedResult.FeedbackStackCount = FeedbackStackCount;
		RecentManualDropResults.Add(Request.RequestId, MoveTemp(CachedResult));
		RecentManualDropOrder.Remove(Request.RequestId);
		RecentManualDropOrder.Add(Request.RequestId);
		while (RecentManualDropOrder.Num() > MaxRecentManualDropResults)
		{
			RecentManualDropResults.Remove(RecentManualDropOrder[0]);
			RecentManualDropOrder.RemoveAt(
				0,
				1,
				EAllowShrinking::No);
		}
	}

	SendActionFeedback(
		RpgGameplayTags::Rpg_Inventory_Action_Drop,
		Result,
		Inventory,
		Item,
		FeedbackStackCount,
		Request.RequestId,
		Request.ItemId);
}

void URpgInventoryUiActionComponent::SendActionFeedback(
	FGameplayTag ActionTag,
	ERpgInventoryActionFeedbackResult Result,
	URpgInventoryManagerComponent* Inventory,
	URpgInventoryItemInstance* Item,
	int32 StackCount,
	const FGuid& RequestId,
	FRpgInventoryItemId ItemId) const
{
	FRpgInventoryActionFeedbackMessage Message;
	Message.RequestId = RequestId;
	Message.ItemId = ItemId.IsValid() ? ItemId : (Item ? Item->GetItemId() : FRpgInventoryItemId());
	Message.ActionTag = ActionTag;
	Message.Result = Result;
	Message.InventoryOwner = Inventory;
	// Full transfers and merges can unregister the source subobject before this reliable RPC serializes. Stable ItemId
	// remains authoritative; include the UObject only while it is still owned by the reported inventory.
	Message.Item = Inventory && Item && Inventory->ContainsItemInstance(Item) ? Item : nullptr;
	Message.StackCount = StackCount;

	const_cast<URpgInventoryUiActionComponent*>(this)->ClientBroadcastInventoryActionFeedback(Message);
}

void URpgInventoryUiActionComponent::ClientBroadcastInventoryActionFeedback_Implementation(const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!GetWorld())
	{
		return;
	}

	APlayerController* LocalRecipient = Cast<APlayerController>(GetOwner());
	if (!ensureMsgf(
			LocalRecipient,
			TEXT("Inventory UI feedback requires a PlayerController-owned action component: %s"),
			*GetNameSafe(this)))
	{
		return;
	}

	FRpgInventoryActionFeedbackMessage LocalMessage = Message;
	LocalMessage.Recipient = LocalRecipient;
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		LocalMessage);
}
