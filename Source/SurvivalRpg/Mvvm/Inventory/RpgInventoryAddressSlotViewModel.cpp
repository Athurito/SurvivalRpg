#include "RpgInventoryAddressSlotViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryAddressSlotViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags InventoryAddressSlotTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

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
		!SlotLabel.IdenticalTo(NewSlotLabel, InventoryAddressSlotTextIdentityFlags);
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			InventoryAddressSlotTextIdentityFlags);
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
