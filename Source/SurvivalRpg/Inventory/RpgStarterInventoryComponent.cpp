#include "RpgStarterInventoryComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStarterInventoryComponent)

URpgStarterInventoryComponent::URpgStarterInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URpgStarterInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	TryGrantStarterInventory();
}

void URpgStarterInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void URpgStarterInventoryComponent::TryGrantStarterInventory()
{
	if (bHasTriedGrant)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		ScheduleRetry();
		return;
	}

	if (!Owner->HasAuthority())
	{
		bHasTriedGrant = true;
		return;
	}

	ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(Owner);
	ARpgPlayerState* PlayerState = PlayerController ? PlayerController->GetRpgPlayerState() : nullptr;
	URpgInventoryManagerComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	URpgEquipmentLoadoutComponent* EquipmentLoadout = PlayerController ? PlayerController->GetEquipmentLoadoutComponent() : nullptr;
	URpgPlayerInventoryLayoutComponent* InventoryLayout = PlayerController ? PlayerController->GetPlayerInventoryLayoutComponent() : nullptr;

	if (PlayerController == nullptr || PlayerState == nullptr || InventoryComponent == nullptr || ShouldWaitForPawn(PlayerController))
	{
		ScheduleRetry();
		return;
	}

	if (InventoryLayout)
	{
		InventoryLayout->ApplyLayoutCapacityToInventory();
	}

	for (const FRpgStarterInventoryEntry& Entry : StarterInventory)
	{
		if (Entry.ItemDefinition.IsNull() || Entry.StackCount <= 0)
		{
			continue;
		}

		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = Entry.ItemDefinition.LoadSynchronous();
		if (ItemDefinition == nullptr)
		{
			continue;
		}

		URpgInventoryItemInstance* ItemInstance = bGrantOnlyIfMissing
			? InventoryComponent->FindFirstItemStackByDefinition(ItemDefinition)
			: nullptr;

		if (ItemInstance == nullptr)
		{
			ItemInstance = InventoryComponent->AddItemDefinition(ItemDefinition, Entry.StackCount);
		}

		if (Entry.bAssignToEquipment && EquipmentLoadout != nullptr && ItemInstance != nullptr && !EquipmentLoadoutContainsItem(EquipmentLoadout, ItemInstance))
		{
			if (Entry.EquipmentSlot == ERpgEquipmentSlot::MainHand || Entry.EquipmentSlot == ERpgEquipmentSlot::OffHand)
			{
				TryMoveItemToFirstCompatibleCarrySlot(InventoryLayout, InventoryComponent, ItemInstance);
			}
			else
			{
				EquipmentLoadout->AssignItemToEquipmentSlot(Entry.EquipmentSlot, ItemInstance);
			}
		}
	}

	bHasTriedGrant = true;
}

void URpgStarterInventoryComponent::ScheduleRetry()
{
	if (MaxGrantAttempts > 0 && ++GrantAttempts >= MaxGrantAttempts)
	{
		bHasTriedGrant = true;
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RetryTimerHandle,
			this,
			&ThisClass::TryGrantStarterInventory,
			RetryInterval,
			false);
	}
}

bool URpgStarterInventoryComponent::ShouldWaitForPawn(const ARpgPlayerController* PlayerController) const
{
	if (!bWaitForPawnBeforeAssigningEquipment || PlayerController == nullptr)
	{
		return false;
	}

	for (const FRpgStarterInventoryEntry& Entry : StarterInventory)
	{
		if (Entry.bAssignToEquipment)
		{
			const APawn* Pawn = PlayerController->GetPawn();
			return Pawn == nullptr || Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() == nullptr;
		}
	}

	return false;
}

bool URpgStarterInventoryComponent::TryMoveItemToFirstCompatibleCarrySlot(URpgPlayerInventoryLayoutComponent* InventoryLayout, URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* ItemInstance)
{
	if (!InventoryLayout || !Inventory || !ItemInstance || Inventory->GetItemStackCount(ItemInstance) <= 0)
	{
		return false;
	}

	FRpgInventorySlotAddress CurrentAddress;
	const int32 CurrentGlobalSlotIndex = Inventory->GetItemSlotIndex(ItemInstance);
	if (InventoryLayout->TryMakeSlotAddressFromGlobalSlotIndex(CurrentGlobalSlotIndex, CurrentAddress) &&
		InventoryLayout->IsCarrySlotAddress(CurrentAddress) &&
		InventoryLayout->CanItemUseSlotAddress(ItemInstance, CurrentAddress))
	{
		return true;
	}

	FGuid EntryId;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.Instance == ItemInstance)
		{
			EntryId = Entry.EntryId;
			break;
		}
	}

	if (!EntryId.IsValid())
	{
		return false;
	}

	for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
	{
		if (!Group.Rule.bCarrySlot || !Group.Rule.AllowsItem(ItemInstance))
		{
			continue;
		}

		for (int32 LocalSlotIndex = 0; LocalSlotIndex < Group.SlotCount; ++LocalSlotIndex)
		{
			const int32 GlobalSlotIndex = Group.FirstGlobalSlotIndex + LocalSlotIndex;
			if (!Inventory->GetItemInSlot(GlobalSlotIndex))
			{
				return Inventory->MoveInventoryEntryToSlot(EntryId, GlobalSlotIndex);
			}
		}
	}

	return false;
}

bool URpgStarterInventoryComponent::EquipmentLoadoutContainsItem(const URpgEquipmentLoadoutComponent* EquipmentLoadout, const URpgInventoryItemInstance* ItemInstance)
{
	if (EquipmentLoadout == nullptr || ItemInstance == nullptr)
	{
		return false;
	}

	for (const FRpgEquipmentLoadoutSlot& LoadoutSlot : EquipmentLoadout->GetLoadoutSlots())
	{
		if (LoadoutSlot.Item == ItemInstance)
		{
			return true;
		}
	}

	return false;
}
