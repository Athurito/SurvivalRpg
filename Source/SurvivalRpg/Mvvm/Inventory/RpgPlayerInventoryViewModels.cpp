#include "RpgPlayerInventoryViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryViewModels)

namespace
{
	struct FRpgPlayerInventoryItemPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText ShortDisplayName;
	};

	FRpgPlayerInventoryItemPresentation BuildPlayerInventoryItemPresentation(const URpgInventoryItemInstance* ItemInstance)
	{
		FRpgPlayerInventoryItemPresentation Presentation;
		if (!ItemInstance)
		{
			return Presentation;
		}

		FText DisplayName = FText::GetEmpty();
		if (const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = ItemInstance->GetItemDef())
		{
			if (const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDefinition))
			{
				DisplayName = ItemCDO->DisplayName;
			}
		}

		if (const URpgInventoryFragment_UIData* UIData = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>())
		{
			Presentation.Icon = UIData->Icon;
			Presentation.ShortDisplayName = UIData->ShortDisplayName.IsEmpty() ? DisplayName : UIData->ShortDisplayName;
		}
		else
		{
			Presentation.ShortDisplayName = DisplayName;
		}

		return Presentation;
	}

	FText BuildAddressSlotLabel(const FRpgInventorySlotGroupView& GroupView, int32 LocalSlotIndex)
	{
		if (GroupView.SlotCount <= 1)
		{
			return GroupView.DisplayName;
		}

		return FText::Format(
			NSLOCTEXT("RpgPlayerInventory", "SlotGroupIndexLabel", "{0} {1}"),
			GroupView.DisplayName,
			FText::AsNumber(LocalSlotIndex + 1));
	}

	FGuid FindEntryIdForItem(const URpgInventoryManagerComponent* Inventory, const URpgInventoryItemInstance* Item)
	{
		if (!Inventory || !Item)
		{
			return FGuid();
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance == Item)
			{
				return Entry.EntryId;
			}
		}

		return FGuid();
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
		const TMap<FName, TObjectPtr<URpgInventorySlotGroupViewModel>>& ReusableGroups,
		FName GroupId)
	{
		if (const TObjectPtr<URpgInventorySlotGroupViewModel>* ExistingGroup = ReusableGroups.Find(GroupId))
		{
			return ExistingGroup->Get();
		}

		return nullptr;
	}
}

void URpgInventoryAddressSlotViewModel::InitializeSlot(
	URpgInventoryManagerComponent* InInventory,
	URpgPlayerInventoryLayoutComponent* InInventoryLayout,
	const FRpgInventorySlotGroupView& InGroupView,
	int32 InLocalSlotIndex)
{
	const FRpgInventorySlotAddress NewAddress = InGroupView.MakeAddress(InLocalSlotIndex);
	const int32 NewGlobalSlotIndex = InGroupView.FirstGlobalSlotIndex + InLocalSlotIndex;
	URpgInventoryItemInstance* NewItem = InInventory ? InInventory->GetItemInSlot(NewGlobalSlotIndex) : nullptr;
	const int32 NewStackCount = (InInventory && NewItem) ? InInventory->GetItemStackCount(NewItem) : 0;
	const FGuid NewEntryId = FindEntryIdForItem(InInventory, NewItem);
	const FRpgPlayerInventoryItemPresentation Presentation = BuildPlayerInventoryItemPresentation(NewItem);
	const bool bNewActionbarBindable = InInventoryLayout
		? InInventoryLayout->CanBindSlotAddressToActionbar(NewAddress, NewItem)
		: false;

	const bool bWasChanged =
		Inventory != InInventory ||
		InventoryLayout != InInventoryLayout ||
		GroupId != InGroupView.GroupId ||
		SlotAddress != NewAddress ||
		LocalSlotIndex != InLocalSlotIndex ||
		GlobalSlotIndex != NewGlobalSlotIndex ||
		EntryId != NewEntryId ||
		ItemInstance != NewItem ||
		StackCount != NewStackCount ||
		bActionbarBindable != bNewActionbarBindable ||
		bCarrySlot != (InGroupView.GroupKind == ERpgInventorySlotGroupKind::Carry && InGroupView.Rule.bCarrySlot) ||
		bGearSlot != (InGroupView.GroupKind == ERpgInventorySlotGroupKind::Gear);

	Inventory = InInventory;
	InventoryLayout = InInventoryLayout;
	GroupId = InGroupView.GroupId;
	SlotAddress = NewAddress;
	LocalSlotIndex = InLocalSlotIndex;
	GlobalSlotIndex = NewGlobalSlotIndex;
	EntryId = NewEntryId;
	ItemInstance = NewItem;
	StackCount = NewStackCount;
	SlotLabel = BuildAddressSlotLabel(InGroupView, InLocalSlotIndex);
	ShortDisplayName = Presentation.ShortDisplayName;
	Icon = Presentation.Icon;
	bIsEmptySlot = ItemInstance == nullptr;
	bCanDrag = ItemInstance != nullptr && StackCount > 0;
	bActionbarBindable = bNewActionbarBindable;
	bCarrySlot = InGroupView.GroupKind == ERpgInventorySlotGroupKind::Carry && InGroupView.Rule.bCarrySlot;
	bGearSlot = InGroupView.GroupKind == ERpgInventorySlotGroupKind::Gear;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Inventory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryLayout);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GroupId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotAddress);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(LocalSlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GlobalSlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotLabel);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmptySlot);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDrag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bActionbarBindable);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCarrySlot);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bGearSlot);

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgInventorySlotGroupViewModel::InitializeGroup(const FRpgInventorySlotGroupView& InGroupView, const TArray<URpgInventoryAddressSlotViewModel*>& InSlots)
{
	GroupId = InGroupView.GroupId;
	DisplayName = InGroupView.DisplayName;
	Icon = InGroupView.Icon;
	FirstGlobalSlotIndex = InGroupView.FirstGlobalSlotIndex;
	SlotCount = InGroupView.SlotCount;
	bActionbarBindable = InGroupView.Rule.bActionbarBindable;
	bCarryGroup = InGroupView.GroupKind == ERpgInventorySlotGroupKind::Carry && InGroupView.Rule.bCarrySlot;
	bGearGroup = InGroupView.GroupKind == ERpgInventorySlotGroupKind::Gear;
	bContentGroup = InGroupView.GroupKind == ERpgInventorySlotGroupKind::Content;
	bProvidedByEquipment = InGroupView.bProvidedByEquipment;
	SourceEquipmentSlotName = InGroupView.SourceEquipmentSlotName;

	Slots.Reset();
	Slots.Reserve(InSlots.Num());
	for (URpgInventoryAddressSlotViewModel* Slot : InSlots)
	{
		Slots.Add(Slot);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GroupId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FirstGlobalSlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bActionbarBindable);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCarryGroup);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bGearGroup);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bContentGroup);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bProvidedByEquipment);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SourceEquipmentSlotName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
}

TArray<URpgInventoryAddressSlotViewModel*> URpgInventorySlotGroupViewModel::GetSlots() const
{
	TArray<URpgInventoryAddressSlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgInventoryAddressSlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

void URpgPlayerInventoryViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	const ARpgPlayerState* RpgPlayerState = RpgPlayerController ? RpgPlayerController->GetRpgPlayerState() : nullptr;

	UnregisterMessageListeners();

	OwningPlayerController = InPlayerController;
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
	OwningPlayerController.Reset();
	ObservedPlayerInventory.Reset();
	ObservedInventoryLayout.Reset();
	ObservedEquipmentLoadout.Reset();
	ObservedActionBar.Reset();
	RefreshAll();
}

void URpgPlayerInventoryViewModel::RefreshAll()
{
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

URpgInventorySlotGroupViewModel* URpgPlayerInventoryViewModel::GetSlotGroup(FName GroupId) const
{
	for (URpgInventorySlotGroupViewModel* Group : CarryGroups)
	{
		if (Group && Group->GetGroupId() == GroupId)
		{
			return Group;
		}
	}

	for (URpgInventorySlotGroupViewModel* Group : InventoryGroups)
	{
		if (Group && Group->GetGroupId() == GroupId)
		{
			return Group;
		}
	}

	return nullptr;
}

void URpgPlayerInventoryViewModel::BeginDestroy()
{
	UnregisterMessageListeners();
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

void URpgPlayerInventoryViewModel::RefreshGearSlots()
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();
	URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();

	auto ResolveGearSlotItem = [PlayerInventory, InventoryLayout, EquipmentLoadout](ERpgEquipmentSlot EquipmentSlot)
	{
		FRpgInventorySlotAddress GearAddress;
		int32 GlobalSlotIndex = INDEX_NONE;
		if (PlayerInventory &&
			InventoryLayout &&
			URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, GearAddress) &&
			InventoryLayout->ResolveSlotAddress(GearAddress, GlobalSlotIndex))
		{
			return PlayerInventory->GetItemInSlot(GlobalSlotIndex);
		}

		return EquipmentLoadout ? EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot) : nullptr;
	};

	auto RefreshSlotsForOrder = [this, &ResolveGearSlotItem](TArray<TObjectPtr<URpgEquipmentSlotViewModel>>& InOutSlots, TConstArrayView<ERpgEquipmentSlot> SlotOrder)
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
	};

	RefreshSlotsForOrder(ArmorSlots, GetArmorSlotOrder());
	RefreshSlotsForOrder(BagSlots, GetBagSlotOrder());

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ArmorSlots);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BagSlots);
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
				if (Slot)
				{
					ReusableSlots.Add(Slot->GetSlotAddress(), Slot);
				}
			}
		}
	};

	TMap<FName, TObjectPtr<URpgInventorySlotGroupViewModel>> ReusableGroups;
	auto CacheGroups = [&ReusableGroups](const TArray<TObjectPtr<URpgInventorySlotGroupViewModel>>& Groups)
	{
		for (URpgInventorySlotGroupViewModel* Group : Groups)
		{
			if (Group)
			{
				ReusableGroups.Add(Group->GetGroupId(), Group);
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
			TArray<URpgInventoryAddressSlotViewModel*> GroupSlots;
			GroupSlots.Reserve(GroupView.SlotCount);

			for (int32 LocalSlotIndex = 0; LocalSlotIndex < GroupView.SlotCount; ++LocalSlotIndex)
			{
				const FRpgInventorySlotAddress Address = GroupView.MakeAddress(LocalSlotIndex);
				URpgInventoryAddressSlotViewModel* SlotViewModel = FindReusableAddressSlot(ReusableSlots, Address);
				if (!SlotViewModel)
				{
					SlotViewModel = NewObject<URpgInventoryAddressSlotViewModel>(this);
				}

				SlotViewModel->InitializeSlot(PlayerInventory, InventoryLayout, GroupView, LocalSlotIndex);
				GroupSlots.Add(SlotViewModel);
			}

			URpgInventorySlotGroupViewModel* GroupViewModel = FindReusableGroup(ReusableGroups, GroupView.GroupId);
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

	CarryGroups = MoveTemp(NewCarryGroups);
	InventoryGroups = MoveTemp(NewInventoryGroups);

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CarryGroups);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryGroups);
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
		SlotViewModel->InitializeSlot(SlotIndex, SourceSlot, ResolvedItem, StackCount);
		ActionBarSlots.Add(SlotViewModel);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ActionBarSlots);
	OnActionBarSlotsChanged.Broadcast();
}

void URpgPlayerInventoryViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RefreshSlotGroups();
		RefreshActionBarSlots();
	}
}

void URpgPlayerInventoryViewModel::HandleLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RefreshSlotGroups();
		RefreshActionBarSlots();
	}
}

void URpgPlayerInventoryViewModel::HandleEquipmentSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	const URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();
	if (EquipmentLoadout && Message.Owner == EquipmentLoadout->GetOwner())
	{
		RefreshGearSlots();
		RefreshSlotGroups();
		RefreshActionBarSlots();
	}
}

void URpgPlayerInventoryViewModel::HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	if (ActionBar && Message.ActionBarComponent == ActionBar)
	{
		RefreshActionBarSlots();
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
