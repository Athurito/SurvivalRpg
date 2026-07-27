#include "RpgActionBarViewModel.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarViewModel)

namespace
{
	template <typename ViewModelType>
	bool AreActionBarSlotViewModelArraysEqual(
		const TArray<TObjectPtr<ViewModelType>>& A,
		const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}

		return true;
	}
}

void URpgActionBarViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	const ARpgPlayerState* RpgPlayerState = RpgPlayerController ? RpgPlayerController->GetRpgPlayerState() : nullptr;
	BindActionBarWithLayout(
		RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr,
		RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr,
		RpgPlayerController ? RpgPlayerController->GetPlayerInventoryLayoutComponent() : nullptr);
}

void URpgActionBarViewModel::BindActionBar(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory)
{
	BindActionBarWithLayout(InActionBar, InPlayerInventory, nullptr);
}

void URpgActionBarViewModel::BindActionBarWithLayout(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory, URpgPlayerInventoryLayoutComponent* InInventoryLayout)
{
	if (ObservedActionBar.Get() == InActionBar && ObservedPlayerInventory.Get() == InPlayerInventory && ObservedInventoryLayout.Get() == InInventoryLayout)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListener();
	ObservedActionBar = InActionBar;
	ObservedPlayerInventory = InPlayerInventory;
	ObservedInventoryLayout = InInventoryLayout;
	RegisterMessageListener();
	RefreshSlots();
}

void URpgActionBarViewModel::UnbindActionBar()
{
	UnregisterMessageListener();
	ObservedActionBar.Reset();
	ObservedPlayerInventory.Reset();
	ObservedInventoryLayout.Reset();
	RefreshSlots();
}

void URpgActionBarViewModel::RefreshSlots()
{
	CancelQueuedRefreshSlots();

	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	const ARpgPlayerController* RpgPlayerController = ActionBar ? Cast<ARpgPlayerController>(ActionBar->GetOwner()) : nullptr;
	const URpgAbilitySystemComponent* AbilitySystem = RpgPlayerController ? RpgPlayerController->GetRpgAbilitySystemComponent() : nullptr;
	const TArray<FRpgActionBarSlot> SourceSlots = ActionBar ? ActionBar->GetSlots() : TArray<FRpgActionBarSlot>();
	const int32 SlotCount = ActionBar ? FMath::Max(ActionBar->GetNumSlots(), SourceSlots.Num()) : FMath::Max(1, DefaultSlotCount);

	TArray<TObjectPtr<URpgActionBarSlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		URpgActionBarSlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotIndex) ? PreviousSlots[SlotIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgActionBarSlotViewModel>(this);
		}

		const FRpgActionBarSlot EmptySlot;
		const FRpgActionBarSlot& SourceSlot = SourceSlots.IsValidIndex(SlotIndex) ? SourceSlots[SlotIndex] : EmptySlot;
		URpgInventoryItemInstance* ResolvedItem = InventoryLayout ? InventoryLayout->GetItemInSlotAddress(SourceSlot.SlotAddress) : nullptr;
		const int32 StackCount = (PlayerInventory && ResolvedItem)
			? PlayerInventory->GetItemStackCount(ResolvedItem)
			: 0;
		FText CarryDisplayName;
		if (InventoryLayout && SourceSlot.CarrySemanticRole.IsValid())
		{
			FRpgInventorySlotGroupView CarryGroup;
			if (InventoryLayout->TryGetSlotGroupBySemanticRole(
					SourceSlot.CarrySemanticRole,
					CarryGroup) &&
				CarryGroup.GroupKind == ERpgInventorySlotGroupKind::Carry)
			{
				CarryDisplayName = CarryGroup.DisplayName;
			}
		}
		SlotViewModel->InitializeSlotWithAbilitySystem(
			SlotIndex,
			SourceSlot,
			ResolvedItem,
			StackCount,
			AbilitySystem,
			CarryDisplayName);
		Slots.Add(SlotViewModel);
	}

	if (!AreActionBarSlotViewModelArraysEqual(PreviousSlots, Slots))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	}
	OnSlotsChanged.Broadcast();
}

TArray<URpgActionBarSlotViewModel*> URpgActionBarViewModel::GetSlots() const
{
	TArray<URpgActionBarSlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgActionBarSlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgActionBarSlotViewModel* URpgActionBarViewModel::GetSlotAtIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void URpgActionBarViewModel::BeginDestroy()
{
	UnregisterMessageListener();
	CancelQueuedRefreshSlots();
	Super::BeginDestroy();
}

void URpgActionBarViewModel::RegisterMessageListener()
{
	UnregisterMessageListener();

	URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	UWorld* World = ActionBar ? ActionBar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	SlotsChangedHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarSlotsChanged);

	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandlePlayerInventoryChanged);

	LayoutChangedHandle = MessageSubsystem.RegisterListener<FRpgPlayerInventoryLayoutChangedMessage>(
		RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
		this,
		&ThisClass::HandlePlayerInventoryLayoutChanged);
}

void URpgActionBarViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}

	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}

	if (LayoutChangedHandle.IsValid())
	{
		LayoutChangedHandle.Unregister();
	}
}

void URpgActionBarViewModel::RequestRefreshSlots()
{
	UWorld* World = nullptr;
	if (URpgActionBarComponent* ActionBar = ObservedActionBar.Get())
	{
		World = ActionBar->GetWorld();
	}
	else if (URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get())
	{
		World = PlayerInventory->GetWorld();
	}
	else if (URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get())
	{
		World = InventoryLayout->GetWorld();
	}

	if (!World)
	{
		RefreshSlots();
		return;
	}

	RefreshSlotsQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefreshSlots);
}

void URpgActionBarViewModel::ExecuteQueuedRefreshSlots()
{
	if (!RefreshSlotsQueue.Consume())
	{
		return;
	}

	RefreshSlots();
}

void URpgActionBarViewModel::CancelQueuedRefreshSlots()
{
	RefreshSlotsQueue.Cancel();
}

void URpgActionBarViewModel::HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	if (ActionBar && Message.ActionBarComponent == ActionBar)
	{
		RequestRefreshSlots();
	}
}

void URpgActionBarViewModel::HandlePlayerInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RequestRefreshSlots();
	}
}

void URpgActionBarViewModel::HandlePlayerInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RequestRefreshSlots();
	}
}
