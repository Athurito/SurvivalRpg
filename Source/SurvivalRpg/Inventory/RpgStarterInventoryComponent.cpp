#include "RpgStarterInventoryComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
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

	if (PlayerController == nullptr || PlayerState == nullptr || InventoryComponent == nullptr)
	{
		ScheduleRetry();
		return;
	}

	// PostLogin restores the complete graph before feature-authored starter items may be considered.
	if (ARpgGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
	{
		if (!GameMode->IsPlayerProfileRestoreComplete(PlayerController))
		{
			ScheduleRetry();
			return;
		}
		if (GameMode->HasRestoredPlayerProfile(PlayerController))
		{
			bHasTriedGrant = true;
			return;
		}
	}

	if (ShouldWaitForPawn(PlayerController))
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
			else if (TryMoveItemToEquipmentSlot(InventoryLayout, InventoryComponent, Entry.EquipmentSlot, ItemInstance))
			{
				EquipmentLoadout->AssignItemToEquipmentSlot(Entry.EquipmentSlot, ItemInstance);
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
	FRpgInventoryGridPlacement CurrentPlacement;
	if (Inventory->GetItemPlacement(ItemInstance, CurrentPlacement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(CurrentPlacement, CurrentAddress) &&
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

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				FRpgInventoryGridPlacement TargetPlacement;
				TargetPlacement.SetContainerHandle(Group.ContainerHandle.IsValid()
					? Group.ContainerHandle
					: FRpgInventoryContainerHandle::MakeRoot(Group.ContainerId));
				TargetPlacement.X = X;
				TargetPlacement.Y = Y;
				TargetPlacement.Width = 1;
				TargetPlacement.Height = 1;
				if (Inventory->CanMoveInventoryEntryToPlacement(EntryId, TargetPlacement))
				{
					return Inventory->MoveInventoryEntryToPlacement(EntryId, TargetPlacement);
				}
			}
		}
	}

	return false;
}

bool URpgStarterInventoryComponent::TryMoveItemToEquipmentSlot(
	URpgPlayerInventoryLayoutComponent* InventoryLayout,
	URpgInventoryManagerComponent* Inventory,
	ERpgEquipmentSlot EquipmentSlot,
	URpgInventoryItemInstance* ItemInstance)
{
	if (!InventoryLayout || !Inventory || !ItemInstance || Inventory->GetItemStackCount(ItemInstance) <= 0)
	{
		return false;
	}

	FRpgInventorySlotAddress GearAddress;
	FRpgInventoryGridPlacement GearPlacement;
	if (!URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, GearAddress) ||
		!InventoryLayout->ResolveSlotAddress(GearAddress, GearPlacement) ||
		!InventoryLayout->CanItemUseSlotAddress(ItemInstance, GearAddress))
	{
		return false;
	}

	FRpgInventoryGridPlacement CurrentPlacement;
	if (Inventory->GetItemPlacement(ItemInstance, CurrentPlacement) &&
		CurrentPlacement.GetContainerHandle() == GearPlacement.GetContainerHandle() &&
		CurrentPlacement.X == GearPlacement.X &&
		CurrentPlacement.Y == GearPlacement.Y)
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

	return EntryId.IsValid() &&
		Inventory->CanMoveInventoryEntryToPlacement(EntryId, GearPlacement) &&
		Inventory->MoveInventoryEntryToPlacement(EntryId, GearPlacement);
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
