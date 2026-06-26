#include "RpgInventoryUiActionComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemUseContext.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_ApplyItemEffects.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseConstructionSiteActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUiActionComponent)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInventoryUiActions, Log, All);

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(const URpgInventoryItemInstance* Item)
	{
		return Item ? Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	}

	const URpgInventoryFragment_ItemTraits* GetUiActionItemTraitsForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->IsMaterial();
	}

	bool IsMaterialItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetUiActionItemTraitsForDefinition(ItemDefinition);
		return Traits && Traits->IsMaterial();
	}

	bool IsStackableItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits && Traits->GetMaxStackSize() > 1;
	}

	bool IsUiActionHandEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
	{
		return EquipmentSlot == ERpgEquipmentSlot::MainHand || EquipmentSlot == ERpgEquipmentSlot::OffHand;
	}

	bool IsUiActionManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
	{
		return IsUiActionHandEquipmentSlot(EquipmentSlot) ||
			EquipmentSlot == ERpgEquipmentSlot::Head ||
			EquipmentSlot == ERpgEquipmentSlot::Chest ||
			EquipmentSlot == ERpgEquipmentSlot::Hands ||
			EquipmentSlot == ERpgEquipmentSlot::Legs ||
			EquipmentSlot == ERpgEquipmentSlot::Feet;
	}

	ERpgInventoryManualDropPolicy GetManualDropPolicy(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(Item);
		return Traits ? Traits->GetResolvedManualDropPolicy() : ERpgInventoryManualDropPolicy::Direct;
	}

	bool CanTargetAcceptTransferredStack(URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 TransferCount, bool bTransfersWholeEntry)
	{
		if (!TargetInventory || !Item || TransferCount <= 0)
		{
			return false;
		}

		if (IsStackableItem(Item))
		{
			return TargetInventory->CanAddItemDefinition(Item->GetItemDef(), TransferCount);
		}

		return bTransfersWholeEntry && TargetInventory->CanAddItemInstance(Item, TransferCount);
	}

	int32 GetAvailableUpgradeCostCount(
		const URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		int32 AvailableCount = 0;

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly && PlayerInventory)
		{
			AvailableCount += PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition);
		}

		if (ConsumeOrder != ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly && BaseStorage)
		{
			AvailableCount += BaseStorage->GetResourceCount(ItemDefinition);
		}

		return AvailableCount;
	}

	bool ConsumeUpgradeCostFromPlayer(URpgInventoryManagerComponent* PlayerInventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (PlayerInventory && PlayerInventory->ConsumeItemsByDefinition(ItemDefinition, CountToConsume));
	}

	bool ConsumeUpgradeCostFromBase(URpgBaseStorageComponent* BaseStorage, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 CountToConsume)
	{
		return CountToConsume <= 0 || (BaseStorage && BaseStorage->WithdrawResource(ItemDefinition, CountToConsume));
	}

	bool ConsumeUpgradeCost(
		URpgInventoryManagerComponent* PlayerInventory,
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = Count;
		auto ConsumeFromBase = [&]()
		{
			const int32 AvailableInBase = BaseStorage ? BaseStorage->GetResourceCount(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInBase, RemainingCount);
			if (!ConsumeUpgradeCostFromBase(BaseStorage, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer = PlayerInventory ? PlayerInventory->GetTotalItemCountByDefinition(ItemDefinition) : 0;
			const int32 CountToConsume = FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(PlayerInventory, ItemDefinition, CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseThenPlayer:
			return ConsumeFromBase() && ConsumeFromPlayer() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerThenBase:
			return ConsumeFromPlayer() && ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount <= 0;
		}

		return false;
	}
}

URpgInventoryUiActionComponent::URpgInventoryUiActionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ManualDropActorClass = ARpgDroppedInventoryActor::StaticClass();
}

void URpgInventoryUiActionComponent::RequestAssignItemToQuickBar_Implementation(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (!IsUiActionHandEquipmentSlot(EquipmentSlot))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, FindPlayerInventory(), Item, 1);
		return;
	}

	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		if (!EquipmentLoadout->AssignItemToEquipmentSlot(EquipmentSlot, Item))
		{
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, FindPlayerInventory(), Item, 1);
		}
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::ServerRejected, FindPlayerInventory(), Item, 1);
}

void URpgInventoryUiActionComponent::RequestSwapQuickBarSlots_Implementation(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot)
{
	if (!IsUiActionHandEquipmentSlot(SourceEquipmentSlot) || !IsUiActionHandEquipmentSlot(TargetEquipmentSlot))
	{
		return;
	}

	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		URpgInventoryItemInstance* SourceItem = EquipmentLoadout->GetItemInEquipmentSlot(SourceEquipmentSlot);
		URpgInventoryItemInstance* TargetItem = EquipmentLoadout->GetItemInEquipmentSlot(TargetEquipmentSlot);

		EquipmentLoadout->ClearEquipmentSlot(SourceEquipmentSlot);
		EquipmentLoadout->ClearEquipmentSlot(TargetEquipmentSlot);

		if (SourceItem)
		{
			EquipmentLoadout->AssignItemToEquipmentSlot(TargetEquipmentSlot, SourceItem);
		}

		if (TargetItem)
		{
			EquipmentLoadout->AssignItemToEquipmentSlot(SourceEquipmentSlot, TargetItem);
		}
	}
}

void URpgInventoryUiActionComponent::RequestClearQuickBarSlot_Implementation(int32 QuickBarSlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	if (IsUiActionHandEquipmentSlot(EquipmentSlot))
	{
		RequestClearEquipmentSlot(EquipmentSlot);
	}
}

void URpgInventoryUiActionComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		if (!EquipmentLoadout->AssignItemToEquipmentSlot(EquipmentSlot, Item))
		{
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, FindPlayerInventory(), Item, 1);
		}
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::ServerRejected, FindPlayerInventory(), Item, 1);
}

void URpgInventoryUiActionComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearEquipmentSlot(EquipmentSlot);
	}
}

void URpgInventoryUiActionComponent::RequestTransferItemStack_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	if (!CanTransferItemStack(SourceInventory, TargetInventory, Item, StackCount))
	{
		const ERpgInventoryActionFeedbackResult Result = (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
			? ERpgInventoryActionFeedbackResult::NoAccess
			: ERpgInventoryActionFeedbackResult::InventoryFull;
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, Result, SourceInventory, Item, StackCount);
		return;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
	if (TransferCount <= 0)
	{
		return;
	}

	const bool bTransfersWholeEntry = TransferCount >= AvailableCount;
	const bool bTransferAsStackableDefinition = IsStackableItem(Item);

	if (TransferCount >= AvailableCount)
	{
		if (SourceInventory == FindPlayerInventory())
		{
			ClearPlayerAssignmentsForItem(Item);
		}

		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		SourceInventory->RemoveItemInstance(Item);
		if (bTransferAsStackableDefinition)
		{
			TargetInventory->AddItemDefinition(ItemDefinition, AvailableCount);
		}
		else
		{
			TargetInventory->AddItemInstanceWithStack(Item, AvailableCount);
		}
		return;
	}

	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
	{
		TargetInventory->AddItemDefinition(ItemDefinition, TransferCount);
	}
}

void URpgInventoryUiActionComponent::RequestTransferItemStackToInventorySlot_Implementation(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex)
{
	if (!CanTransferItemStackToInventorySlot(SourceInventory, TargetInventory, Item, StackCount, TargetSlotIndex))
	{
		const ERpgInventoryActionFeedbackResult Result = TargetSlotIndex < 0
			? ERpgInventoryActionFeedbackResult::InvalidSlot
			: ((!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
				? ERpgInventoryActionFeedbackResult::NoAccess
				: ERpgInventoryActionFeedbackResult::InventoryFull);
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Transfer, Result, SourceInventory, Item, StackCount);
		return;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	URpgInventoryItemInstance* TargetItem = TargetInventory->GetItemInSlot(TargetSlotIndex);

	if (TargetItem && TargetItem->GetItemDef() == Item->GetItemDef())
	{
		const int32 FreeStackCapacity = TargetInventory->GetFreeStackCapacity(TargetItem);
		if (FreeStackCapacity > 0)
		{
			const int32 TransferCount = FMath::Min3(AvailableCount, RequestedCount, FreeStackCapacity);
			if (TransferCount <= 0)
			{
				return;
			}

			if (SourceInventory == FindPlayerInventory() && TransferCount >= AvailableCount)
			{
				ClearPlayerAssignmentsForItem(Item);
			}

			if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
			{
				TargetInventory->AddStackToExistingItem(TargetItem, TransferCount);
			}
			return;
		}
	}

	if (!TargetItem)
	{
		const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
		if (TransferCount <= 0)
		{
			return;
		}

		if (TransferCount >= AvailableCount)
		{
			if (SourceInventory == FindPlayerInventory())
			{
				ClearPlayerAssignmentsForItem(Item);
			}

			SourceInventory->RemoveItemInstance(Item);
			TargetInventory->AddItemInstanceWithStackToSlot(Item, AvailableCount, TargetSlotIndex);
			return;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		if (SourceInventory->RemoveItemInstanceStack(Item, TransferCount))
		{
			TargetInventory->AddItemDefinitionToSlot(ItemDefinition, TransferCount, TargetSlotIndex);
		}
		return;
	}

	if (RequestedCount < AvailableCount)
	{
		return;
	}

	const int32 SourceSlotIndex = SourceInventory->GetItemSlotIndex(Item);
	const int32 TargetStackCount = TargetInventory->GetItemStackCount(TargetItem);
	if (SourceSlotIndex == INDEX_NONE || TargetStackCount <= 0)
	{
		return;
	}

	if (SourceInventory == FindPlayerInventory())
	{
		ClearPlayerAssignmentsForItem(Item);
	}

	if (TargetInventory == FindPlayerInventory())
	{
		ClearPlayerAssignmentsForItem(TargetItem);
	}

	SourceInventory->RemoveItemInstance(Item);
	TargetInventory->RemoveItemInstance(TargetItem);
	SourceInventory->AddItemInstanceWithStackToSlot(TargetItem, TargetStackCount, SourceSlotIndex);
	TargetInventory->AddItemInstanceWithStackToSlot(Item, AvailableCount, TargetSlotIndex);
}

void URpgInventoryUiActionComponent::RequestApplyInventorySort_Implementation(URpgInventoryManagerComponent* Inventory, ERpgInventorySortMode SortMode)
{
	if (!CanAccessInventory(Inventory))
	{
		return;
	}

	Inventory->ApplyInventorySort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntry_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetIndex)
{
	if (!CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntry(EntryId, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestMoveInventoryEntryToSlot_Implementation(URpgInventoryManagerComponent* Inventory, FGuid EntryId, int32 TargetSlotIndex)
{
	if (!CanAccessInventory(Inventory) || !Inventory->ContainsEntry(EntryId))
	{
		return;
	}

	Inventory->MoveInventoryEntryToSlot(EntryId, TargetSlotIndex);
}

void URpgInventoryUiActionComponent::RequestSplitItemStack_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, int32 TargetSlotIndex)
{
	int32 ActualSplitCount = 0;
	int32 ActualTargetSlotIndex = INDEX_NONE;
	if (!CanSplitItemStack(Inventory, Item, SplitCount, TargetSlotIndex, ActualSplitCount, ActualTargetSlotIndex))
	{
		const ERpgInventoryActionFeedbackResult Result = !Item || !IsStackableItem(Item)
			? ERpgInventoryActionFeedbackResult::NotStackable
			: ERpgInventoryActionFeedbackResult::InventoryFull;
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Split, Result, Inventory, Item, SplitCount);
		return;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (!Inventory->RemoveItemInstanceStack(Item, ActualSplitCount))
	{
		return;
	}

	if (!Inventory->AddItemDefinitionToSlot(ItemDefinition, ActualSplitCount, ActualTargetSlotIndex))
	{
		Inventory->AddStackToExistingItem(Item, ActualSplitCount);
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Split, ERpgInventoryActionFeedbackResult::InventoryFull, Inventory, Item, ActualSplitCount);
	}
}

void URpgInventoryUiActionComponent::RequestUseInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount)
{
	if (!Inventory || !Item)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::InvalidRequest, Inventory, Item, StackCount);
		return;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::NoAccess, Inventory, Item, StackCount);
		return;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::MissingItem, Inventory, Item, StackCount);
		return;
	}

	const URpgInventoryFragment_UsableItem* UsableFragment = Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>();
	if (!UsableFragment || !UsableFragment->UseAbility)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::CannotUse, Inventory, Item, StackCount);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (UsableFragment->bOnlyFromPlayerInventory && Inventory != PlayerInventory)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::WrongInventory, Inventory, Item, StackCount);
		return;
	}

	const int32 UseCount = FMath::Max(1, StackCount);
	const int32 ConsumeCount = FMath::Max(0, UsableFragment->ConsumeCount) * UseCount;
	if (ConsumeCount > AvailableCount)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::MissingItem, Inventory, Item, ConsumeCount);
		return;
	}

	URpgAbilitySystemComponent* AbilitySystem = FindPlayerAbilitySystem();
	if (!AbilitySystem)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::ServerRejected, Inventory, Item, StackCount);
		return;
	}

	AController* OwnerController = Cast<AController>(GetOwner());
	AActor* AvatarActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	FGameplayEventData EventData;
	EventData.EventTag = RpgGameplayTags::Rpg_Inventory_Action_Use;
	EventData.Instigator = AvatarActor;
	EventData.Target = AvatarActor;
	EventData.OptionalObject = Item;
	EventData.EventMagnitude = static_cast<float>(UseCount);

	URpgInventoryItemUseContext* UseContext = NewObject<URpgInventoryItemUseContext>(this);
	UseContext->Initialize(Inventory, Item, UseCount, ConsumeCount);

	const bool bUsesApplyEffectsContext = UsableFragment->UseAbility->IsChildOf(URpgGameplayAbility_ApplyItemEffects::StaticClass());
	if (bUsesApplyEffectsContext)
	{
		URpgGameplayAbility_ApplyItemEffects::RegisterPendingUseContext(AbilitySystem, Item, UseContext);
	}

	FGameplayAbilitySpec UseSpec(UsableFragment->UseAbility, FMath::Max(1, UsableFragment->AbilityLevel), INDEX_NONE, Item);
	const FGameplayAbilitySpecHandle ActivatedHandle = AbilitySystem->GiveAbilityAndActivateOnce(UseSpec, &EventData);
	if (!ActivatedHandle.IsValid())
	{
		if (bUsesApplyEffectsContext)
		{
			URpgGameplayAbility_ApplyItemEffects::ClearPendingUseContext(AbilitySystem, Item);
		}

		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::AbilityRejected, Inventory, Item, StackCount);
		return;
	}

	if (UsableFragment->bConsumeOnActivationAccepted && ConsumeCount > 0)
	{
		if (Inventory == PlayerInventory && ConsumeCount >= AvailableCount)
		{
			ClearPlayerAssignmentsForItem(Item);
		}

		if (!UseContext->TryConsume())
		{
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::ServerRejected, Inventory, Item, ConsumeCount);
			return;
		}
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Use, ERpgInventoryActionFeedbackResult::Success, Inventory, Item, ConsumeCount);
}

void URpgInventoryUiActionComponent::RequestEquipInventoryItem_Implementation(URpgInventoryItemInstance* Item)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || !Item || PlayerInventory->GetItemStackCount(Item) <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::MissingItem, PlayerInventory, Item, 1);
		return;
	}

	if (Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() == nullptr)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NotEquippable, PlayerInventory, Item, 1);
		return;
	}

	if (!TryAssignItemToDefaultEquipmentDestination(Item))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::NoValidSlot, PlayerInventory, Item, 1);
		return;
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Equip, ERpgInventoryActionFeedbackResult::Success, PlayerInventory, Item, 1);
}

void URpgInventoryUiActionComponent::RequestDropInventoryItem_Implementation(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount, bool bConfirmed)
{
	if (!Inventory || !Item)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::InvalidRequest, Inventory, Item, StackCount);
		return;
	}

	if (!CanAccessInventory(Inventory))
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::NoAccess, Inventory, Item, StackCount);
		return;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::MissingItem, Inventory, Item, StackCount);
		return;
	}

	const ERpgInventoryManualDropPolicy DropPolicy = GetManualDropPolicy(Item);
	if (DropPolicy == ERpgInventoryManualDropPolicy::Disabled)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::CannotDrop, Inventory, Item, StackCount);
		return;
	}

	if (DropPolicy == ERpgInventoryManualDropPolicy::Confirm && !bConfirmed)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::RequiresConfirmation, Inventory, Item, StackCount);
		return;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 DropCount = FMath::Min(AvailableCount, RequestedCount);
	if (DropCount <= 0)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::InvalidRequest, Inventory, Item, StackCount);
		return;
	}

	const bool bDropAsStackTemplate = IsStackableItem(Item);
	if (!bDropAsStackTemplate && DropCount < AvailableCount)
	{
		SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::InvalidRequest, Inventory, Item, StackCount);
		return;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const bool bDropsWholePlayerEntry = Inventory == PlayerInventory && DropCount >= AvailableCount;

	if (bDropAsStackTemplate)
	{
		if (!Inventory->RemoveItemInstanceStack(Item, DropCount))
		{
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::ServerRejected, Inventory, Item, DropCount);
			return;
		}

		if (!TrySpawnManualDrop(Item, DropCount, false))
		{
			Inventory->AddItemDefinition(ItemDefinition, DropCount);
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::ServerRejected, Inventory, Item, DropCount);
			return;
		}

		if (bDropsWholePlayerEntry)
		{
			ClearPlayerAssignmentsForItem(Item);
		}
	}
	else
	{
		Inventory->RemoveItemInstance(Item);
		if (!TrySpawnManualDrop(Item, AvailableCount, true))
		{
			Inventory->AddItemInstanceWithStack(Item, AvailableCount);
			SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::ServerRejected, Inventory, Item, AvailableCount);
			return;
		}

		if (bDropsWholePlayerEntry)
		{
			ClearPlayerAssignmentsForItem(Item);
		}
	}

	SendActionFeedback(RpgGameplayTags::Rpg_Inventory_Action_Drop, ERpgInventoryActionFeedbackResult::Success, Inventory, Item, DropCount);
}

void URpgInventoryUiActionComponent::RequestDepositAllMaterialsToBase_Implementation(URpgBaseStorageStationComponent* Station)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries = PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0 || !IsMaterialItem(Item))
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
		if (!Station->AllowsResourceDefinition(ItemDefinition))
		{
			continue;
		}

		const int32 CountToDeposit = FMath::Min(Entry.StackCount, BaseStorage->GetFreeResourceCapacity(ItemDefinition));
		if (CountToDeposit <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, CountToDeposit))
		{
			continue;
		}

		if (PlayerInventory->RemoveItemInstanceStack(Item, CountToDeposit))
		{
			BaseStorage->StoreResource(ItemDefinition, CountToDeposit);
		}
	}
}

void URpgInventoryUiActionComponent::RequestDepositMaterialStackToBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !Item || !IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount = FMath::Min(AvailableCount, RequestedCount);
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (TransferCount <= 0 || !BaseStorage->CanStoreResource(ItemDefinition, TransferCount))
	{
		return;
	}

	if (PlayerInventory->RemoveItemInstanceStack(Item, TransferCount))
	{
		BaseStorage->StoreResource(ItemDefinition, TransferCount);
	}
}

void URpgInventoryUiActionComponent::RequestWithdrawResourceFromBase_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !PlayerInventory || !BaseStorage || !ItemDefinition || StackCount <= 0)
	{
		return;
	}

	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (BaseStorage->GetResourceCount(ItemDefinition) < StackCount || !PlayerInventory->CanAddItemDefinition(ItemDefinition, StackCount))
	{
		return;
	}

	if (BaseStorage->WithdrawResource(ItemDefinition, StackCount))
	{
		PlayerInventory->AddItemDefinition(ItemDefinition, StackCount);
	}
}

void URpgInventoryUiActionComponent::RequestStoreItemInstanceInBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item || IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount = PlayerInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount || !ArmoryInventory->CanAddItemInstance(Item, AvailableCount))
	{
		return;
	}

	ClearPlayerAssignmentsForItem(Item);
	PlayerInventory->RemoveItemInstance(Item);
	ArmoryInventory->AddItemInstanceWithStack(Item, AvailableCount);
}

void URpgInventoryUiActionComponent::RequestTakeItemInstanceFromBase_Implementation(URpgBaseStorageStationComponent* Station, URpgInventoryItemInstance* Item, int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ArmoryInventory = Station ? Station->GetArmoryInventory() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || Station->GetStationMode() != ERpgBaseStorageStationMode::Terminal || !PlayerInventory || !ArmoryInventory || !Item)
	{
		return;
	}

	const int32 AvailableCount = ArmoryInventory->GetItemStackCount(Item);
	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (AvailableCount <= 0 || RequestedCount != AvailableCount || !PlayerInventory->CanAddItemInstance(Item, AvailableCount))
	{
		return;
	}

	ArmoryInventory->RemoveItemInstance(Item);
	PlayerInventory->AddItemInstanceWithStack(Item, AvailableCount);
}

void URpgInventoryUiActionComponent::RequestInstallBaseStorageUpgrade_Implementation(URpgBaseStorageStationComponent* Station, URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!Station)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: Station is null. Owner=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!CanAccessBaseStorageStation(Station))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station access denied. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!PlayerInventory)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: player inventory missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!BaseStorage)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: base storage missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!UpgradeDefinition)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: upgrade definition is null. Owner=%s Station=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station));
		return;
	}

	if (!Station->CanInstallUpgrade(UpgradeDefinition))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: station cannot install upgrade, maybe already installed or station tags do not match. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder = Station->GetUpgradeCostConsumeOrder();
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade requested: Owner=%s Station=%s Upgrade=%s CostCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		UpgradeDefinition->Costs.Num());

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!Cost.ItemDefinition)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: empty cost item definition. Upgrade=%s"),
				*GetNameSafe(UpgradeDefinition));
			return;
		}

		if (Cost.Count <= 0)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: invalid cost count. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}

		if (!IsMaterialItemDefinition(Cost.ItemDefinition))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost item is not marked as material. Upgrade=%s ItemDef=%s"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}

		const int32 AvailableCount = GetAvailableUpgradeCostCount(PlayerInventory, BaseStorage, Cost.ItemDefinition, ConsumeOrder);
		if (AvailableCount < Cost.Count)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: not enough resources. Upgrade=%s ItemDef=%s Available=%d Required=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				AvailableCount,
				Cost.Count);
			return;
		}
	}

	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!ConsumeUpgradeCost(PlayerInventory, BaseStorage, Cost.ItemDefinition, Cost.Count, ConsumeOrder))
		{
			UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Install base storage upgrade failed: cost consume failed after validation. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}
	}

	const bool bInstalled = Station->InstallUpgrade(UpgradeDefinition);
	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Install base storage upgrade result: Owner=%s Station=%s Upgrade=%s Installed=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		bInstalled ? TEXT("true") : TEXT("false"));
}

void URpgInventoryUiActionComponent::RequestApplyBaseResourceSort_Implementation(URpgBaseStorageStationComponent* Station, ERpgInventorySortMode SortMode)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage)
	{
		return;
	}

	BaseStorage->ApplyResourceSort(SortMode);
}

void URpgInventoryUiActionComponent::RequestMoveBaseResourceEntry_Implementation(URpgBaseStorageStationComponent* Station, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	URpgBaseStorageComponent* BaseStorage = Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage || !Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	BaseStorage->MoveResourceEntry(ItemDefinition, TargetIndex);
}

void URpgInventoryUiActionComponent::RequestPlaceBaseBuildable_Implementation(ARpgBaseCampActor* BaseCamp, URpgBaseBuildableDefinition* BuildableDefinition, FTransform BuildTransform, bool bAutoContributeFromBase)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!BaseCamp || !BuildableDefinition || !RequestingActor)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: missing input. Owner=%s BaseCamp=%s Buildable=%s RequestingActor=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(RequestingActor));
		return;
	}

	if (!BaseCamp->CanPlaceBuildableAtTransform(BuildableDefinition, BuildTransform, RequestingActor))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: placement validation denied. Owner=%s BaseCamp=%s Buildable=%s BuildActorClass=%s BaseDist=%.0f BuilderDist=%.0f BuildLocation=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(BuildableDefinition->BuildActorClass),
			FVector::Dist(BaseCamp->GetActorLocation(), BuildTransform.GetLocation()),
			FVector::Dist(RequestingActor->GetActorLocation(), BuildTransform.GetLocation()),
			*BuildTransform.GetLocation().ToCompactString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: world missing. Owner=%s BaseCamp=%s Buildable=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition));
		return;
	}

	TSubclassOf<ARpgBaseConstructionSiteActor> ConstructionSiteClass = BuildableDefinition->ConstructionSiteActorClass;
	if (!ConstructionSiteClass)
	{
		ConstructionSiteClass = ARpgBaseConstructionSiteActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = BaseCamp;
	SpawnParams.Instigator = Cast<APawn>(RequestingActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgBaseConstructionSiteActor* ConstructionSite = World->SpawnActor<ARpgBaseConstructionSiteActor>(ConstructionSiteClass, BuildTransform, SpawnParams);
	if (!ConstructionSite)
	{
		UE_LOG(LogRpgInventoryUiActions, Warning, TEXT("Place buildable failed: construction site spawn failed. Owner=%s BaseCamp=%s Buildable=%s SiteClass=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(ConstructionSiteClass));
		return;
	}

	UE_LOG(LogRpgInventoryUiActions, Log, TEXT("Place buildable succeeded: Site=%s BaseCamp=%s Buildable=%s BuildActorClass=%s AutoContributeFromBase=%s Location=%s"),
		*GetNameSafe(ConstructionSite),
		*GetNameSafe(BaseCamp),
		*GetNameSafe(BuildableDefinition),
		*GetNameSafe(BuildableDefinition->BuildActorClass),
		bAutoContributeFromBase ? TEXT("true") : TEXT("false"),
		*BuildTransform.GetLocation().ToCompactString());

	ConstructionSite->InitializeConstructionSite(BaseCamp, BuildableDefinition);
	if (bAutoContributeFromBase && IsValid(ConstructionSite) && !ConstructionSite->IsConstructionComplete())
	{
		ConstructionSite->ContributeAllResources(RequestingActor, true);
	}
}

void URpgInventoryUiActionComponent::RequestContributeAllToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor)
	{
		ConstructionSite->ContributeAllResources(RequestingActor, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestContributeMaterialToBaseConstructionSite_Implementation(ARpgBaseConstructionSiteActor* ConstructionSite, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, bool bAllowBaseStorage)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (ConstructionSite && RequestingActor && ItemDefinition && StackCount > 0)
	{
		ConstructionSite->ContributeMaterial(RequestingActor, ItemDefinition, StackCount, bAllowBaseStorage);
	}
}

void URpgInventoryUiActionComponent::RequestCraftRecipe_Implementation(URpgCraftingStationComponent* CraftingStation, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (!CraftingStation || !RecipeDefinition || !RequestingActor || !CraftingStation->CanCraftRecipeQuantity(RequestingActor, RecipeDefinition, Quantity))
	{
		return;
	}

	CraftingStation->QueueCraftRecipe(RequestingActor, RecipeDefinition, Quantity);
}

void URpgInventoryUiActionComponent::RequestCancelCraftJob_Implementation(URpgCraftingStationComponent* CraftingStation, FGuid JobId)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->CancelCraftJob(RequestingActor, JobId);
	}
}

void URpgInventoryUiActionComponent::RequestPauseCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->PauseCraftingStation(RequestingActor);
	}
}

void URpgInventoryUiActionComponent::RequestResumeCraftingStation_Implementation(URpgCraftingStationComponent* CraftingStation)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->ResumeCraftingStation(RequestingActor);
	}
}

void URpgInventoryUiActionComponent::RequestSetCraftingOutputAutoDepositEnabled_Implementation(URpgCraftingStationComponent* CraftingStation, bool bEnabled)
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->SetCraftingOutputAutoDepositEnabled(RequestingActor, bEnabled);
	}
}

bool URpgInventoryUiActionComponent::CanAccessInventory(URpgInventoryManagerComponent* Inventory) const
{
	if (Inventory == nullptr)
	{
		return false;
	}

	if (Inventory == FindPlayerInventory())
	{
		return true;
	}

	const AController* OwnerController = Cast<AController>(GetOwner());
	const AActor* RequestingActor = OwnerController ? OwnerController->GetPawn() : GetOwner();

	const URpgInventoryManagerComponent* BaseArmoryInventory = Inventory;
	const AActor* InventoryOwner = Inventory->GetOwner();
	const URpgBaseStorageStationComponent* Station = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgBaseStorageStationComponent>() : nullptr;
	if (Station && Station->GetArmoryInventory() == BaseArmoryInventory)
	{
		return Station->CanActorAccess(RequestingActor);
	}

	const URpgCraftingStationComponent* CraftingStation = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgCraftingStationComponent>() : nullptr;
	if (CraftingStation && CraftingStation->GetOutputInventory() == Inventory)
	{
		return CraftingStation->CanActorAccess(RequestingActor);
	}

	const URpgInventoryContainerComponent* Container = InventoryOwner ? InventoryOwner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
	if (!Container)
	{
		return false;
	}

	return Container->CanActorAccess(RequestingActor);
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

bool URpgInventoryUiActionComponent::CanTransferItemStack(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item)
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	return CanTargetAcceptTransferredStack(TargetInventory, Item, RequestedCount, RequestedCount >= AvailableCount);
}

bool URpgInventoryUiActionComponent::CanTransferItemStackToInventorySlot(URpgInventoryManagerComponent* SourceInventory, URpgInventoryManagerComponent* TargetInventory, URpgInventoryItemInstance* Item, int32 StackCount, int32 TargetSlotIndex) const
{
	if (!SourceInventory || !TargetInventory || SourceInventory == TargetInventory || !Item || TargetSlotIndex < 0)
	{
		return false;
	}

	if (!CanAccessInventory(SourceInventory) || !CanAccessInventory(TargetInventory))
	{
		return false;
	}

	const int32 AvailableCount = SourceInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return false;
	}

	const int32 RequestedCount = StackCount <= 0 ? AvailableCount : StackCount;
	if (RequestedCount <= 0 || RequestedCount > AvailableCount)
	{
		return false;
	}

	URpgInventoryItemInstance* TargetItem = TargetInventory->GetItemInSlot(TargetSlotIndex);
	if (!TargetItem)
	{
		if (RequestedCount >= AvailableCount)
		{
			return TargetInventory->CanAddItemInstanceToSlot(Item, AvailableCount, TargetSlotIndex);
		}

		return TargetInventory->CanAddItemDefinitionToSlot(Item->GetItemDef(), RequestedCount, TargetSlotIndex);
	}

	if (TargetItem->GetItemDef() == Item->GetItemDef() && TargetInventory->GetFreeStackCapacity(TargetItem) > 0)
	{
		return true;
	}

	return RequestedCount >= AvailableCount && SourceInventory->GetItemSlotIndex(Item) != INDEX_NONE;
}

bool URpgInventoryUiActionComponent::CanSplitItemStack(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 SplitCount, int32 TargetSlotIndex, int32& OutSplitCount, int32& OutTargetSlotIndex) const
{
	OutSplitCount = 0;
	OutTargetSlotIndex = INDEX_NONE;

	if (!Inventory || !Item || !CanAccessInventory(Inventory) || !IsStackableItem(Item))
	{
		return false;
	}

	const int32 AvailableCount = Inventory->GetItemStackCount(Item);
	if (AvailableCount <= 1)
	{
		return false;
	}

	const int32 RequestedSplitCount = SplitCount <= 0 ? AvailableCount / 2 : SplitCount;
	if (RequestedSplitCount <= 0 || RequestedSplitCount >= AvailableCount)
	{
		return false;
	}

	int32 ResolvedTargetSlotIndex = TargetSlotIndex;
	if (ResolvedTargetSlotIndex == INDEX_NONE && !FindFirstEmptyInventorySlot(Inventory, ResolvedTargetSlotIndex))
	{
		return false;
	}

	if (ResolvedTargetSlotIndex < 0 || Inventory->GetItemInSlot(ResolvedTargetSlotIndex) != nullptr)
	{
		return false;
	}

	if (!Inventory->CanAddItemDefinitionToSlot(Item->GetItemDef(), RequestedSplitCount, ResolvedTargetSlotIndex))
	{
		return false;
	}

	OutSplitCount = RequestedSplitCount;
	OutTargetSlotIndex = ResolvedTargetSlotIndex;
	return true;
}

bool URpgInventoryUiActionComponent::FindFirstEmptyInventorySlot(URpgInventoryManagerComponent* Inventory, int32& OutSlotIndex) const
{
	OutSlotIndex = INDEX_NONE;
	if (!Inventory)
	{
		return false;
	}

	const int32 ScanLimit = Inventory->IsCapacityUnlimited()
		? Inventory->GetUsedEntryCount() + 1
		: Inventory->GetMaxEntries();
	for (int32 SlotIndex = 0; SlotIndex < ScanLimit; ++SlotIndex)
	{
		if (!Inventory->GetItemInSlot(SlotIndex))
		{
			OutSlotIndex = SlotIndex;
			return true;
		}
	}

	return false;
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

void URpgInventoryUiActionComponent::ClearPlayerAssignmentsForItem(URpgInventoryItemInstance* Item) const
{
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
	{
		EquipmentLoadout->ClearItemFromAllEquipmentSlots(Item);
	}
}

bool URpgInventoryUiActionComponent::TryAssignItemToDefaultEquipmentDestination(URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return false;
	}

	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	if (!EquipmentCDO)
	{
		return false;
	}

	const ERpgEquipmentSlot DefaultSlot = EquipmentCDO->GetDefaultEquipSlot();
	if (IsUiActionManagedEquipmentSlot(DefaultSlot))
	{
		if (URpgEquipmentLoadoutComponent* EquipmentLoadout = FindEquipmentLoadout())
		{
			return EquipmentLoadout->AssignItemToEquipmentSlot(DefaultSlot, Item);
		}
		return false;
	}

	return false;
}

bool URpgInventoryUiActionComponent::TrySpawnManualDrop(URpgInventoryItemInstance* Item, int32 StackCount, bool bDropAsInstance)
{
	if (!Item || StackCount <= 0 || !GetWorld())
	{
		return false;
	}

	const FTransform DropTransform = GetManualDropTransform();
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Item->GetItemDef();
	if (!bDropAsInstance && TryMergeManualDrop(ItemDefinition, StackCount, DropTransform.GetLocation()))
	{
		return true;
	}

	TSubclassOf<ARpgDroppedInventoryActor> DropClass = ManualDropActorClass;
	if (!DropClass)
	{
		DropClass = ARpgDroppedInventoryActor::StaticClass();
	}

	AController* OwnerController = Cast<AController>(GetOwner());
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Instigator = OwnerController ? OwnerController->GetPawn() : nullptr;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgDroppedInventoryActor* DropActor = GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(DropClass, DropTransform, SpawnParameters);
	if (!DropActor)
	{
		return false;
	}

	FInventoryPickup Pickup;
	if (bDropAsInstance)
	{
		FPickupInstance& Instance = Pickup.Instances.AddDefaulted_GetRef();
		Instance.Item = Item;
	}
	else
	{
		FPickupTemplate& Template = Pickup.Templates.AddDefaulted_GetRef();
		Template.ItemDef = ItemDefinition;
		Template.StackCount = StackCount;
	}

	DropActor->SetPickupInventory(Pickup);
	return true;
}

bool URpgInventoryUiActionComponent::TryMergeManualDrop(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount, const FVector& SpawnLocation) const
{
	if (!ItemDefinition || StackCount <= 0 || ManualDropMergeRadius <= 0.0f || !GetWorld())
	{
		return false;
	}

	const float MergeRadiusSq = FMath::Square(ManualDropMergeRadius);
	for (TActorIterator<ARpgDroppedInventoryActor> It(GetWorld()); It; ++It)
	{
		ARpgDroppedInventoryActor* DropActor = *It;
		if (!DropActor || DropActor->IsPendingKillPending())
		{
			continue;
		}

		if (FVector::DistSquared(DropActor->GetActorLocation(), SpawnLocation) > MergeRadiusSq)
		{
			continue;
		}

		if (DropActor->CanMergePickupTemplate(ItemDefinition) && DropActor->MergePickupTemplate(ItemDefinition, StackCount))
		{
			return true;
		}
	}

	return false;
}

FTransform URpgInventoryUiActionComponent::GetManualDropTransform() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	const APawn* Pawn = OwnerController ? OwnerController->GetPawn() : nullptr;
	const AActor* SourceActor = Pawn ? Cast<AActor>(Pawn) : GetOwner();
	if (!SourceActor)
	{
		return FTransform::Identity;
	}

	FVector SpawnLocation = SourceActor->GetActorLocation();
	SpawnLocation += SourceActor->GetActorForwardVector() * ManualDropForwardDistance;
	SpawnLocation += FVector::UpVector * ManualDropUpOffset;
	return FTransform(SourceActor->GetActorRotation(), SpawnLocation);
}

void URpgInventoryUiActionComponent::SendActionFeedback(FGameplayTag ActionTag, ERpgInventoryActionFeedbackResult Result, URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 StackCount) const
{
	FRpgInventoryActionFeedbackMessage Message;
	Message.ActionTag = ActionTag;
	Message.Result = Result;
	Message.InventoryOwner = Inventory;
	Message.Item = Item;
	Message.StackCount = StackCount;

	const_cast<URpgInventoryUiActionComponent*>(this)->ClientBroadcastInventoryActionFeedback(Message);
}

void URpgInventoryUiActionComponent::ClientBroadcastInventoryActionFeedback_Implementation(const FRpgInventoryActionFeedbackMessage& Message)
{
	if (!GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(GetWorld());
	MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback, Message);
}
