#include "RpgPlayerInventoryViewModel.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryViewModel)

namespace
{
	namespace PlayerInventoryRefreshDomains
	{
		constexpr uint8 Gear = 1 << 0;
		constexpr uint8 SlotGroups = 1 << 1;
		constexpr uint8 ActionBar = 1 << 2;
		constexpr uint8 All = Gear | SlotGroups | ActionBar;
	}

	URpgInventoryAddressSlotViewModel* FindReusableAddressSlot(
		const TMap<FRpgInventorySlotAddress, TObjectPtr<URpgInventoryAddressSlotViewModel>>& ReusableSlots,
		const FRpgInventorySlotAddress& Address)
	{
		if (const TObjectPtr<URpgInventoryAddressSlotViewModel>* ExistingSlot = ReusableSlots.Find(Address))
		{
			return ExistingSlot->Get();
		}

		return nullptr;
	}

	URpgInventorySlotGroupViewModel* FindReusableGroup(
		const TMap<FRpgInventoryContainerHandle, TObjectPtr<URpgInventorySlotGroupViewModel>>& ReusableGroups,
		const FRpgInventoryContainerHandle& ContainerHandle)
	{
		if (const TObjectPtr<URpgInventorySlotGroupViewModel>* ExistingGroup = ReusableGroups.Find(ContainerHandle))
		{
			return ExistingGroup->Get();
		}

		return nullptr;
	}

	template <typename ViewModelType>
	bool ArePlayerInventoryRootViewModelArraysEqual(
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

void URpgPlayerInventoryViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	const ARpgPlayerState* RpgPlayerState = RpgPlayerController ? RpgPlayerController->GetRpgPlayerState() : nullptr;

	UnregisterMessageListeners();

	ObservedPlayerInventory = RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr;
	ObservedInventoryLayout = RpgPlayerController ? RpgPlayerController->GetPlayerInventoryLayoutComponent() : nullptr;
	ObservedEquipmentLoadout = RpgPlayerController ? RpgPlayerController->GetEquipmentLoadoutComponent() : nullptr;
	ObservedActionBar = RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr;

	RegisterMessageListeners();
	RefreshAll();
}

void URpgPlayerInventoryViewModel::UnbindPlayerInventory()
{
	UnregisterMessageListeners();
	ObservedPlayerInventory.Reset();
	ObservedInventoryLayout.Reset();
	ObservedEquipmentLoadout.Reset();
	ObservedActionBar.Reset();
	RefreshAll();
}

void URpgPlayerInventoryViewModel::RefreshAll()
{
	CancelQueuedRefresh();
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBarSlots();
}

TArray<URpgEquipmentSlotViewModel*> URpgPlayerInventoryViewModel::GetArmorSlots() const
{
	TArray<URpgEquipmentSlotViewModel*> Result;
	Result.Reserve(ArmorSlots.Num());
	for (URpgEquipmentSlotViewModel* Slot : ArmorSlots)
	{
		Result.Add(Slot);
	}
	return Result;
}

TArray<URpgEquipmentSlotViewModel*> URpgPlayerInventoryViewModel::GetBagSlots() const
{
	TArray<URpgEquipmentSlotViewModel*> Result;
	Result.Reserve(BagSlots.Num());
	for (URpgEquipmentSlotViewModel* Slot : BagSlots)
	{
		Result.Add(Slot);
	}
	return Result;
}

TArray<URpgInventorySlotGroupViewModel*> URpgPlayerInventoryViewModel::GetCarryGroups() const
{
	TArray<URpgInventorySlotGroupViewModel*> Result;
	Result.Reserve(CarryGroups.Num());
	for (URpgInventorySlotGroupViewModel* Group : CarryGroups)
	{
		Result.Add(Group);
	}
	return Result;
}

TArray<URpgInventorySlotGroupViewModel*> URpgPlayerInventoryViewModel::GetInventoryGroups() const
{
	TArray<URpgInventorySlotGroupViewModel*> Result;
	Result.Reserve(InventoryGroups.Num());
	for (URpgInventorySlotGroupViewModel* Group : InventoryGroups)
	{
		Result.Add(Group);
	}
	return Result;
}

TArray<URpgActionBarSlotViewModel*> URpgPlayerInventoryViewModel::GetActionBarSlots() const
{
	TArray<URpgActionBarSlotViewModel*> Result;
	Result.Reserve(ActionBarSlots.Num());
	for (URpgActionBarSlotViewModel* Slot : ActionBarSlots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgEquipmentSlotViewModel* URpgPlayerInventoryViewModel::GetArmorSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	for (URpgEquipmentSlotViewModel* Slot : ArmorSlots)
	{
		if (Slot && Slot->GetEquipmentSlot() == EquipmentSlot)
		{
			return Slot;
		}
	}
	return nullptr;
}

URpgEquipmentSlotViewModel* URpgPlayerInventoryViewModel::GetBagSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	for (URpgEquipmentSlotViewModel* Slot : BagSlots)
	{
		if (Slot && Slot->GetEquipmentSlot() == EquipmentSlot)
		{
			return Slot;
		}
	}
	return nullptr;
}

URpgInventorySlotGroupViewModel* URpgPlayerInventoryViewModel::GetSlotGroupBySemanticRole(
	FGameplayTag SemanticRole) const
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		ObservedInventoryLayout.Get();
	FRpgInventorySlotGroupView ResolvedGroup;
	if (!InventoryLayout ||
		!InventoryLayout->TryGetSlotGroupBySemanticRole(
			SemanticRole,
			ResolvedGroup))
	{
		return nullptr;
	}

	return GetSlotGroupByHandle(ResolvedGroup.ContainerHandle);
}

URpgInventorySlotGroupViewModel* URpgPlayerInventoryViewModel::GetSlotGroupByHandle(FRpgInventoryContainerHandle ContainerHandle) const
{
	for (URpgInventorySlotGroupViewModel* Group : CarryGroups)
	{
		if (Group && Group->GetContainerHandle() == ContainerHandle)
		{
			return Group;
		}
	}

	for (URpgInventorySlotGroupViewModel* Group : InventoryGroups)
	{
		if (Group && Group->GetContainerHandle() == ContainerHandle)
		{
			return Group;
		}
	}

	return nullptr;
}

void URpgPlayerInventoryViewModel::BeginDestroy()
{
	UnregisterMessageListeners();
	CancelQueuedRefresh();
	Super::BeginDestroy();
}

void URpgPlayerInventoryViewModel::RegisterMessageListeners()
{
	UnregisterMessageListeners();

	UWorld* World = nullptr;
	if (URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get())
	{
		World = PlayerInventory->GetWorld();
	}
	else if (URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get())
	{
		World = EquipmentLoadout->GetWorld();
	}
	else if (URpgActionBarComponent* ActionBar = ObservedActionBar.Get())
	{
		World = ActionBar->GetWorld();
	}

	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);

	LayoutChangedHandle = MessageSubsystem.RegisterListener<FRpgPlayerInventoryLayoutChangedMessage>(
		RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
		this,
		&ThisClass::HandleLayoutChanged);

	EquipmentChangedHandle = MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
		RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleEquipmentSlotsChanged);

	ActionBarChangedHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarSlotsChanged);
}

void URpgPlayerInventoryViewModel::UnregisterMessageListeners()
{
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
	if (LayoutChangedHandle.IsValid())
	{
		LayoutChangedHandle.Unregister();
	}
	if (EquipmentChangedHandle.IsValid())
	{
		EquipmentChangedHandle.Unregister();
	}
	if (ActionBarChangedHandle.IsValid())
	{
		ActionBarChangedHandle.Unregister();
	}
}

void URpgPlayerInventoryViewModel::RequestRefresh(uint8 RefreshDomains)
{
	PendingRefreshDomains |= RefreshDomains;

	UWorld* World = nullptr;
	if (URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get())
	{
		World = PlayerInventory->GetWorld();
	}
	else if (URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get())
	{
		World = InventoryLayout->GetWorld();
	}
	else if (URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get())
	{
		World = EquipmentLoadout->GetWorld();
	}
	else if (URpgActionBarComponent* ActionBar = ObservedActionBar.Get())
	{
		World = ActionBar->GetWorld();
	}

	if (!World)
	{
		FlushPendingRefreshes();
		return;
	}

	RefreshQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefresh);
}

void URpgPlayerInventoryViewModel::ExecuteQueuedRefresh()
{
	if (!RefreshQueue.Consume())
	{
		return;
	}

	FlushPendingRefreshes();
}

void URpgPlayerInventoryViewModel::FlushPendingRefreshes()
{
	const uint8 RefreshDomains = PendingRefreshDomains;
	PendingRefreshDomains = 0;

	if ((RefreshDomains & PlayerInventoryRefreshDomains::Gear) != 0)
	{
		RefreshGearSlots();
	}
	if ((RefreshDomains & PlayerInventoryRefreshDomains::SlotGroups) != 0)
	{
		RefreshSlotGroups();
	}
	if ((RefreshDomains & PlayerInventoryRefreshDomains::ActionBar) != 0)
	{
		RefreshActionBarSlots();
	}
}

void URpgPlayerInventoryViewModel::CancelQueuedRefresh()
{
	RefreshQueue.Cancel();
	PendingRefreshDomains = 0;
}

void URpgPlayerInventoryViewModel::RefreshGearSlots()
{
	URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();

	auto ResolveGearSlotItem = [PlayerInventory, InventoryLayout](
		ERpgEquipmentSlot EquipmentSlot)
	{
		FRpgInventorySlotAddress GearAddress;
		FRpgInventoryGridPlacement GearPlacement;
		if (PlayerInventory &&
			InventoryLayout &&
			InventoryLayout->TryMakeGearSlotAddress(EquipmentSlot, GearAddress) &&
			InventoryLayout->ResolveSlotAddress(GearAddress, GearPlacement))
		{
			return PlayerInventory->GetItemAtContainerCell(GearPlacement.GetContainerHandle(), GearPlacement.X, GearPlacement.Y);
		}

		return static_cast<URpgInventoryItemInstance*>(nullptr);
	};

	auto RefreshSlotsForOrder =
		[this, &ResolveGearSlotItem](
			TArray<TObjectPtr<URpgEquipmentSlotViewModel>>& InOutSlots,
			TConstArrayView<ERpgEquipmentSlot> SlotOrder)
	{
		TArray<TObjectPtr<URpgEquipmentSlotViewModel>> PreviousSlots = MoveTemp(InOutSlots);
		InOutSlots.Reset();
		InOutSlots.Reserve(SlotOrder.Num());

		for (int32 Index = 0; Index < SlotOrder.Num(); ++Index)
		{
			URpgEquipmentSlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(Index) ? PreviousSlots[Index].Get() : nullptr;
			if (!SlotViewModel)
			{
				SlotViewModel = NewObject<URpgEquipmentSlotViewModel>(this);
			}

			const ERpgEquipmentSlot EquipmentSlot = SlotOrder[Index];
			URpgInventoryItemInstance* Item = ResolveGearSlotItem(EquipmentSlot);
			SlotViewModel->InitializeSlot(EquipmentSlot, Item);
			InOutSlots.Add(SlotViewModel);
		}

		return !ArePlayerInventoryRootViewModelArraysEqual(
			PreviousSlots,
			InOutSlots);
	};

	const bool bArmorSlotsChanged =
		RefreshSlotsForOrder(ArmorSlots, GetArmorSlotOrder());
	const bool bBagSlotsChanged =
		RefreshSlotsForOrder(BagSlots, GetBagSlotOrder());

	if (bArmorSlotsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ArmorSlots);
	}
	if (bBagSlotsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BagSlots);
	}
	OnGearSlotsChanged.Broadcast();
}

void URpgPlayerInventoryViewModel::RefreshSlotGroups()
{
	URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();

	TMap<FRpgInventorySlotAddress, TObjectPtr<URpgInventoryAddressSlotViewModel>> ReusableSlots;
	auto CacheSlots = [&ReusableSlots](const TArray<TObjectPtr<URpgInventorySlotGroupViewModel>>& Groups)
	{
		for (URpgInventorySlotGroupViewModel* Group : Groups)
		{
			if (!Group)
			{
				continue;
			}

			for (URpgInventoryAddressSlotViewModel* Slot : Group->GetSlots())
			{
				if (Slot && Slot->GetSlotAddress().IsValid())
				{
					ReusableSlots.Add(Slot->GetSlotAddress(), Slot);
				}
			}
		}
	};

	TMap<FRpgInventoryContainerHandle, TObjectPtr<URpgInventorySlotGroupViewModel>> ReusableGroups;
	auto CacheGroups = [&ReusableGroups](const TArray<TObjectPtr<URpgInventorySlotGroupViewModel>>& Groups)
	{
		for (URpgInventorySlotGroupViewModel* Group : Groups)
		{
			if (Group && Group->GetContainerHandle().IsValid())
			{
				ReusableGroups.Add(Group->GetContainerHandle(), Group);
			}
		}
	};

	CacheSlots(CarryGroups);
	CacheSlots(InventoryGroups);
	CacheGroups(CarryGroups);
	CacheGroups(InventoryGroups);

	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> NewCarryGroups;
	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> NewInventoryGroups;

	if (InventoryLayout)
	{
		for (const FRpgInventorySlotGroupView& GroupView : InventoryLayout->GetSlotGroups())
		{
			if (GroupView.GroupKind != ERpgInventorySlotGroupKind::Carry &&
				GroupView.GroupKind != ERpgInventorySlotGroupKind::Content)
			{
				continue;
			}

			const FRpgInventoryContainerHandle GroupHandle = GroupView.ContainerHandle;
			if (!GroupHandle.IsValid() || !GroupView.GridSize.IsValid())
			{
				continue;
			}

			TArray<URpgInventoryAddressSlotViewModel*> GroupSlots;
			GroupSlots.Reserve(GroupView.GridSize.Width * GroupView.GridSize.Height);

			for (int32 Y = 0; Y < GroupView.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < GroupView.GridSize.Width; ++X)
				{
					const FRpgInventorySlotAddress Address = GroupView.MakeAddress(X, Y);
					URpgInventoryAddressSlotViewModel* SlotViewModel = FindReusableAddressSlot(ReusableSlots, Address);
					if (!SlotViewModel)
					{
						SlotViewModel = NewObject<URpgInventoryAddressSlotViewModel>(this);
					}

					SlotViewModel->InitializeSlot(PlayerInventory, InventoryLayout, GroupView, X, Y);
					GroupSlots.Add(SlotViewModel);
				}
			}

			URpgInventorySlotGroupViewModel* GroupViewModel = FindReusableGroup(ReusableGroups, GroupHandle);
			if (!GroupViewModel)
			{
				GroupViewModel = NewObject<URpgInventorySlotGroupViewModel>(this);
			}

			GroupViewModel->InitializeGroup(GroupView, GroupSlots);
			if (GroupView.GroupKind == ERpgInventorySlotGroupKind::Carry)
			{
				NewCarryGroups.Add(GroupViewModel);
			}
			else if (GroupView.GroupKind == ERpgInventorySlotGroupKind::Content)
			{
				NewInventoryGroups.Add(GroupViewModel);
			}
		}
	}

	const bool bCarryGroupsChanged =
		!ArePlayerInventoryRootViewModelArraysEqual(
			CarryGroups,
			NewCarryGroups);
	const bool bInventoryGroupsChanged =
		!ArePlayerInventoryRootViewModelArraysEqual(
			InventoryGroups,
			NewInventoryGroups);

	CarryGroups = MoveTemp(NewCarryGroups);
	InventoryGroups = MoveTemp(NewInventoryGroups);

	if (bCarryGroupsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CarryGroups);
	}
	if (bInventoryGroupsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryGroups);
	}
	OnSlotGroupsChanged.Broadcast();
}

void URpgPlayerInventoryViewModel::RefreshActionBarSlots()
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	const TArray<FRpgActionBarSlot> SourceSlots = ActionBar ? ActionBar->GetSlots() : TArray<FRpgActionBarSlot>();
	const int32 SlotCount = ActionBar ? FMath::Max(ActionBar->GetNumSlots(), SourceSlots.Num()) : 8;

	TArray<TObjectPtr<URpgActionBarSlotViewModel>> PreviousSlots = MoveTemp(ActionBarSlots);
	ActionBarSlots.Reset();
	ActionBarSlots.Reserve(SlotCount);

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
		const int32 StackCount = (PlayerInventory && ResolvedItem) ? PlayerInventory->GetItemStackCount(ResolvedItem) : 0;
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
			nullptr,
			CarryDisplayName);
		ActionBarSlots.Add(SlotViewModel);
	}

	if (!ArePlayerInventoryRootViewModelArraysEqual(
		PreviousSlots,
		ActionBarSlots))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ActionBarSlots);
	}
	OnActionBarSlotsChanged.Broadcast();
}

void URpgPlayerInventoryViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RequestRefresh(
			PlayerInventoryRefreshDomains::SlotGroups |
			PlayerInventoryRefreshDomains::ActionBar);
	}
}

void URpgPlayerInventoryViewModel::HandleLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RequestRefresh(
			PlayerInventoryRefreshDomains::SlotGroups |
			PlayerInventoryRefreshDomains::ActionBar);
	}
}

void URpgPlayerInventoryViewModel::HandleEquipmentSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	const URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();
	if (EquipmentLoadout && Message.Owner == EquipmentLoadout->GetOwner())
	{
		RequestRefresh(PlayerInventoryRefreshDomains::All);
	}
}

void URpgPlayerInventoryViewModel::HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	if (ActionBar && Message.ActionBarComponent == ActionBar)
	{
		RequestRefresh(PlayerInventoryRefreshDomains::ActionBar);
	}
}

TConstArrayView<ERpgEquipmentSlot> URpgPlayerInventoryViewModel::GetArmorSlotOrder()
{
	static constexpr ERpgEquipmentSlot ArmorSlotsOrder[] =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet
	};

	return TConstArrayView<ERpgEquipmentSlot>(ArmorSlotsOrder, UE_ARRAY_COUNT(ArmorSlotsOrder));
}

TConstArrayView<ERpgEquipmentSlot> URpgPlayerInventoryViewModel::GetBagSlotOrder()
{
	static constexpr ERpgEquipmentSlot BagSlotsOrder[] =
	{
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
	};

	return TConstArrayView<ERpgEquipmentSlot>(BagSlotsOrder, UE_ARRAY_COUNT(BagSlotsOrder));
}
