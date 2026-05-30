#include "RpgStarterInventoryComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

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
	URpgQuickBarComponent* QuickBarComponent = PlayerController ? PlayerController->GetQuickBarComponent() : nullptr;

	if (PlayerController == nullptr || PlayerState == nullptr || InventoryComponent == nullptr || ShouldWaitForPawn(PlayerController))
	{
		ScheduleRetry();
		return;
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

		if (Entry.bAddToQuickBar && QuickBarComponent != nullptr && ItemInstance != nullptr && !QuickBarContainsItem(QuickBarComponent, ItemInstance))
		{
			const int32 SlotIndex = Entry.QuickBarSlotIndex >= 0
				? Entry.QuickBarSlotIndex
				: QuickBarComponent->GetNextFreeItemSlot();

			if (SlotIndex != INDEX_NONE)
			{
				QuickBarComponent->AddItemToSlot(SlotIndex, ItemInstance);

				if (Entry.bActivateQuickBarSlot)
				{
					QuickBarComponent->SetActiveSlotIndex(SlotIndex);
				}
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
	if (!bWaitForPawnBeforeActivatingQuickBar || PlayerController == nullptr)
	{
		return false;
	}

	for (const FRpgStarterInventoryEntry& Entry : StarterInventory)
	{
		if (Entry.bAddToQuickBar && Entry.bActivateQuickBarSlot)
		{
			const APawn* Pawn = PlayerController->GetPawn();
			return Pawn == nullptr || Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() == nullptr;
		}
	}

	return false;
}

bool URpgStarterInventoryComponent::QuickBarContainsItem(const URpgQuickBarComponent* QuickBarComponent, const URpgInventoryItemInstance* ItemInstance)
{
	if (QuickBarComponent == nullptr || ItemInstance == nullptr)
	{
		return false;
	}

	for (const URpgInventoryItemInstance* SlotItem : QuickBarComponent->GetSlots())
	{
		if (SlotItem == ItemInstance)
		{
			return true;
		}
	}

	return false;
}
