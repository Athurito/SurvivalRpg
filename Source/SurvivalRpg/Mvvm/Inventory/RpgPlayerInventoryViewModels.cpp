#include "RpgPlayerInventoryViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryEquipmentPlacementPolicy.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryViewModels)

namespace
{
	constexpr ETextIdenticalModeFlags PlayerInventoryTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	namespace PlayerInventoryRefreshDomains
	{
		constexpr uint8 Gear = 1 << 0;
		constexpr uint8 SlotGroups = 1 << 1;
		constexpr uint8 ActionBar = 1 << 2;
		constexpr uint8 All = Gear | SlotGroups | ActionBar;
	}

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

	FText BuildAddressSlotLabel(const FRpgInventorySlotGroupView& GroupView, int32 X, int32 Y)
	{
		if (GroupView.GridSize.Width * GroupView.GridSize.Height <= 1)
		{
			return GroupView.DisplayName;
		}

		return FText::Format(
			NSLOCTEXT("RpgPlayerInventory", "SlotGroupCellLabel", "{0} {1},{2}"),
			GroupView.DisplayName,
			FText::AsNumber(X + 1),
			FText::AsNumber(Y + 1));
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
		const TMap<FRpgInventoryContainerHandle, TObjectPtr<URpgInventorySlotGroupViewModel>>& ReusableGroups,
		const FRpgInventoryContainerHandle& ContainerHandle)
	{
		if (const TObjectPtr<URpgInventorySlotGroupViewModel>* ExistingGroup = ReusableGroups.Find(ContainerHandle))
		{
			return ExistingGroup->Get();
		}

		return nullptr;
	}

	bool IsTypedCarryGroupView(
		const FRpgInventorySlotGroupView& GroupView)
	{
		return GroupView.GroupKind == ERpgInventorySlotGroupKind::Carry &&
			FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
				GroupView.EquipmentSlotRole);
	}

	template <typename ViewModelType>
	bool ArePlayerInventoryViewModelArraysEqual(
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

void URpgInventoryAddressSlotViewModel::InitializeSlot(
	URpgInventoryManagerComponent* InInventory,
	URpgPlayerInventoryLayoutComponent* InInventoryLayout,
	const FRpgInventorySlotGroupView& InGroupView,
	int32 InX,
	int32 InY)
{
	const FRpgInventorySlotAddress NewAddress = InGroupView.MakeAddress(InX, InY);
	FRpgInventoryGridPlacement NewPlacement;
	NewPlacement.SetContainerHandle(NewAddress.GetContainerHandle());
	NewPlacement.X = NewAddress.X;
	NewPlacement.Y = NewAddress.Y;
	NewPlacement.Width = 1;
	NewPlacement.Height = 1;
	URpgInventoryItemInstance* NewItem = InInventory
		? InInventory->GetItemAtContainerCell(NewPlacement.GetContainerHandle(), NewPlacement.X, NewPlacement.Y)
		: nullptr;
	FRpgInventoryGridPlacement NewItemPlacement;
	const bool bHasResolvedItemPlacement = NewItem && InInventory && InInventory->GetItemPlacement(NewItem, NewItemPlacement);
	const bool bNewItemOriginCell = bHasResolvedItemPlacement &&
		NewItemPlacement.GetContainerHandle() == NewPlacement.GetContainerHandle() &&
		NewItemPlacement.X == NewPlacement.X &&
		NewItemPlacement.Y == NewPlacement.Y;
	const bool bNewItemCoveredCell = bHasResolvedItemPlacement &&
		NewItemPlacement.ContainsCell(NewPlacement.X, NewPlacement.Y) &&
		!bNewItemOriginCell;
	const bool bCanRepresentItemFromThisCell = NewItem && (!bHasResolvedItemPlacement || bNewItemOriginCell);
	int32 NewItemOccupiedWidth = 0;
	int32 NewItemOccupiedHeight = 0;
	if (bHasResolvedItemPlacement)
	{
		const FRpgInventoryGridSize OccupiedSize = NewItemPlacement.GetOccupiedSize();
		NewItemOccupiedWidth = OccupiedSize.Width;
		NewItemOccupiedHeight = OccupiedSize.Height;
	}
	const int32 NewStackCount = (InInventory && NewItem && bCanRepresentItemFromThisCell) ? InInventory->GetItemStackCount(NewItem) : 0;
	const FGuid NewEntryId = FindEntryIdForItem(InInventory, NewItem);
	const FRpgPlayerInventoryItemPresentation Presentation = bCanRepresentItemFromThisCell
		? BuildPlayerInventoryItemPresentation(NewItem)
		: FRpgPlayerInventoryItemPresentation();
	const bool bNewActionbarBindable = InInventoryLayout
		? bCanRepresentItemFromThisCell && InInventoryLayout->CanBindSlotAddressToActionbar(NewAddress, NewItem)
		: false;
	const FText NewSlotLabel = BuildAddressSlotLabel(InGroupView, InX, InY);
	const FText NewShortDisplayName = Presentation.ShortDisplayName;
	const TSoftObjectPtr<UTexture2D> NewIcon = Presentation.Icon;
	const bool bNewIsEmptySlot = NewItem == nullptr;
	const bool bNewRenderItemVisual = bCanRepresentItemFromThisCell;
	const bool bNewCanDrag =
		bCanRepresentItemFromThisCell && NewStackCount > 0;
	const ERpgEquipmentSlot NewEquipmentSlotRole =
		InGroupView.EquipmentSlotRole;
	const bool bNewGearSlot =
		InGroupView.GroupKind == ERpgInventorySlotGroupKind::Gear;

	const bool bInventoryChanged = Inventory != InInventory;
	const bool bInventoryLayoutChanged =
		InventoryLayout != InInventoryLayout;
	const bool bContainerIdChanged = ContainerId != InGroupView.ContainerId;
	const bool bSlotAddressChanged = SlotAddress != NewAddress;
	const bool bXChanged = X != InX;
	const bool bYChanged = Y != InY;
	const bool bPlacementChanged = Placement != NewPlacement;
	const bool bItemPlacementChanged = ItemPlacement != NewItemPlacement;
	const bool bItemOccupiedWidthChanged =
		ItemOccupiedWidth != NewItemOccupiedWidth;
	const bool bItemOccupiedHeightChanged =
		ItemOccupiedHeight != NewItemOccupiedHeight;
	const bool bEntryIdChanged = EntryId != NewEntryId;
	const bool bItemInstanceChanged = ItemInstance != NewItem;
	const bool bStackCountChanged = StackCount != NewStackCount;
	const bool bSlotLabelChanged =
		!SlotLabel.IdenticalTo(NewSlotLabel, PlayerInventoryTextIdentityFlags);
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			PlayerInventoryTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bIsEmptySlotChanged = bIsEmptySlot != bNewIsEmptySlot;
	const bool bItemOriginCellChanged =
		bItemOriginCell != bNewItemOriginCell;
	const bool bItemCoveredCellChanged =
		bItemCoveredCell != bNewItemCoveredCell;
	const bool bRenderItemVisualChanged =
		bRenderItemVisual != bNewRenderItemVisual;
	const bool bCanDragChanged = bCanDrag != bNewCanDrag;
	const bool bActionbarBindableChanged =
		bActionbarBindable != bNewActionbarBindable;
	const bool bEquipmentSlotRoleChanged =
		EquipmentSlotRole != NewEquipmentSlotRole;
	const bool bGearSlotChanged = bGearSlot != bNewGearSlot;
	const bool bWasChanged =
		bInventoryChanged ||
		bInventoryLayoutChanged ||
		bContainerIdChanged ||
		bSlotAddressChanged ||
		bXChanged ||
		bYChanged ||
		bPlacementChanged ||
		bItemPlacementChanged ||
		bItemOccupiedWidthChanged ||
		bItemOccupiedHeightChanged ||
		bEntryIdChanged ||
		bItemInstanceChanged ||
		bStackCountChanged ||
		bSlotLabelChanged ||
		bShortDisplayNameChanged ||
		bIconChanged ||
		bIsEmptySlotChanged ||
		bItemOriginCellChanged ||
		bItemCoveredCellChanged ||
		bRenderItemVisualChanged ||
		bCanDragChanged ||
		bActionbarBindableChanged ||
		bEquipmentSlotRoleChanged ||
		bGearSlotChanged;

	Inventory = InInventory;
	InventoryLayout = InInventoryLayout;
	ContainerId = InGroupView.ContainerId;
	SlotAddress = NewAddress;
	X = InX;
	Y = InY;
	Placement = NewPlacement;
	ItemPlacement = NewItemPlacement;
	ItemOccupiedWidth = NewItemOccupiedWidth;
	ItemOccupiedHeight = NewItemOccupiedHeight;
	EntryId = NewEntryId;
	ItemInstance = NewItem;
	StackCount = NewStackCount;
	SlotLabel = NewSlotLabel;
	ShortDisplayName = NewShortDisplayName;
	Icon = NewIcon;
	bIsEmptySlot = bNewIsEmptySlot;
	bItemOriginCell = bNewItemOriginCell;
	bItemCoveredCell = bNewItemCoveredCell;
	bRenderItemVisual = bNewRenderItemVisual;
	bCanDrag = bNewCanDrag;
	bActionbarBindable = bNewActionbarBindable;
	EquipmentSlotRole = NewEquipmentSlotRole;
	bGearSlot = bNewGearSlot;

	if (bInventoryChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Inventory);
	}
	if (bInventoryLayoutChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryLayout);
	}
	if (bContainerIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ContainerId);
	}
	if (bSlotAddressChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotAddress);
	}
	if (bXChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(X);
	}
	if (bYChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Y);
	}
	if (bPlacementChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Placement);
	}
	if (bItemPlacementChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemPlacement);
	}
	if (bItemOccupiedWidthChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemOccupiedWidth);
	}
	if (bItemOccupiedHeightChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemOccupiedHeight);
	}
	if (bEntryIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	}
	if (bItemInstanceChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	}
	if (bStackCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	}
	if (bSlotLabelChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotLabel);
	}
	if (bShortDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bIsEmptySlotChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsEmptySlot);
	}
	if (bItemOriginCellChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bItemOriginCell);
	}
	if (bItemCoveredCellChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bItemCoveredCell);
	}
	if (bRenderItemVisualChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bRenderItemVisual);
	}
	if (bCanDragChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDrag);
	}
	if (bActionbarBindableChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bActionbarBindable);
	}
	if (bEquipmentSlotRoleChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EquipmentSlotRole);
	}
	if (bGearSlotChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bGearSlot);
	}

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgInventorySlotGroupViewModel::InitializeGroup(const FRpgInventorySlotGroupView& InGroupView, const TArray<URpgInventoryAddressSlotViewModel*>& InSlots)
{
	const FRpgInventoryContainerHandle NewContainerHandle =
		InGroupView.ContainerHandle;
	const FName NewContainerId = InGroupView.ContainerId;
	const FGameplayTag NewSemanticRole = InGroupView.SemanticRole;
	const FText NewDisplayName = InGroupView.DisplayName;
	const TSoftObjectPtr<UTexture2D> NewIcon = InGroupView.Icon;
	const FRpgInventoryGridSize NewGridSize = InGroupView.GridSize;
	const ERpgEquipmentSlot NewEquipmentSlotRole =
		InGroupView.EquipmentSlotRole;
	const bool bNewActionbarBindable = InGroupView.Rule.bActionbarBindable;
	const bool bNewCarryGroup = IsTypedCarryGroupView(InGroupView);
	const bool bNewGearGroup =
		InGroupView.GroupKind == ERpgInventorySlotGroupKind::Gear;
	const bool bNewContentGroup =
		InGroupView.GroupKind == ERpgInventorySlotGroupKind::Content;
	const bool bNewProvidedByEquipment = InGroupView.bProvidedByEquipment;
	const ERpgEquipmentSlot NewSourceEquipmentSlot =
		InGroupView.SourceEquipmentSlot;
	TArray<TObjectPtr<URpgInventoryAddressSlotViewModel>> NewSlots;
	NewSlots.Reserve(InSlots.Num());
	for (URpgInventoryAddressSlotViewModel* Slot : InSlots)
	{
		NewSlots.Add(Slot);
	}

	const bool bContainerHandleChanged =
		ContainerHandle != NewContainerHandle;
	const bool bContainerIdChanged = ContainerId != NewContainerId;
	const bool bSemanticRoleChanged = SemanticRole != NewSemanticRole;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, PlayerInventoryTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bGridSizeChanged = GridSize != NewGridSize;
	const bool bEquipmentSlotRoleChanged =
		EquipmentSlotRole != NewEquipmentSlotRole;
	const bool bActionbarBindableChanged =
		bActionbarBindable != bNewActionbarBindable;
	const bool bCarryGroupChanged = bCarryGroup != bNewCarryGroup;
	const bool bGearGroupChanged = bGearGroup != bNewGearGroup;
	const bool bContentGroupChanged = bContentGroup != bNewContentGroup;
	const bool bProvidedByEquipmentChanged =
		bProvidedByEquipment != bNewProvidedByEquipment;
	const bool bSourceEquipmentSlotChanged =
		SourceEquipmentSlot != NewSourceEquipmentSlot;
	const bool bSlotsChanged =
		!ArePlayerInventoryViewModelArraysEqual(Slots, NewSlots);

	ContainerHandle = NewContainerHandle;
	ContainerId = NewContainerId;
	SemanticRole = NewSemanticRole;
	DisplayName = NewDisplayName;
	Icon = NewIcon;
	GridSize = NewGridSize;
	EquipmentSlotRole = NewEquipmentSlotRole;
	bActionbarBindable = bNewActionbarBindable;
	bCarryGroup = bNewCarryGroup;
	bGearGroup = bNewGearGroup;
	bContentGroup = bNewContentGroup;
	bProvidedByEquipment = bNewProvidedByEquipment;
	SourceEquipmentSlot = NewSourceEquipmentSlot;
	Slots = MoveTemp(NewSlots);

	if (bContainerHandleChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ContainerHandle);
	}
	if (bContainerIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ContainerId);
	}
	if (bSemanticRoleChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SemanticRole);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bGridSizeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GridSize);
	}
	if (bEquipmentSlotRoleChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EquipmentSlotRole);
	}
	if (bActionbarBindableChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bActionbarBindable);
	}
	if (bCarryGroupChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCarryGroup);
	}
	if (bGearGroupChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bGearGroup);
	}
	if (bContentGroupChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bContentGroup);
	}
	if (bProvidedByEquipmentChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bProvidedByEquipment);
	}
	if (bSourceEquipmentSlotChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SourceEquipmentSlot);
	}
	if (bSlotsChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	}
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

		return !ArePlayerInventoryViewModelArraysEqual(
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
		!ArePlayerInventoryViewModelArraysEqual(
			CarryGroups,
			NewCarryGroups);
	const bool bInventoryGroupsChanged =
		!ArePlayerInventoryViewModelArraysEqual(
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

	if (!ArePlayerInventoryViewModelArraysEqual(
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
