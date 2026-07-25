#include "RpgInventoryDragDropTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragDropTypes)

bool FRpgInventorySpatialPreviewDescriptor::IsEquivalentTo(const FRpgInventorySpatialPreviewDescriptor& Other) const
{
	return bValid == Other.bValid &&
		EntryId == Other.EntryId &&
		Target.TargetType == Other.Target.TargetType &&
		Target.TargetInventory == Other.Target.TargetInventory &&
		Target.TargetPlacement.GetContainerHandle() == Other.Target.TargetPlacement.GetContainerHandle() &&
		Target.TargetPlacement.X == Other.Target.TargetPlacement.X &&
		Target.TargetPlacement.Y == Other.Target.TargetPlacement.Y &&
		Target.TargetPlacement.Width == Other.Target.TargetPlacement.Width &&
		Target.TargetPlacement.Height == Other.Target.TargetPlacement.Height &&
		Target.TargetPlacement.bRotated == Other.Target.TargetPlacement.bRotated &&
		Target.SlotAddress == Other.Target.SlotAddress &&
		Target.ActionBarSlotIndex == Other.Target.ActionBarSlotIndex &&
		Target.EquipmentSlot == Other.Target.EquipmentSlot &&
		TargetPlacement.GetContainerHandle() == Other.TargetPlacement.GetContainerHandle() &&
		TargetPlacement.X == Other.TargetPlacement.X &&
		TargetPlacement.Y == Other.TargetPlacement.Y &&
		TargetPlacement.Width == Other.TargetPlacement.Width &&
		TargetPlacement.Height == Other.TargetPlacement.Height &&
		TargetPlacement.bRotated == Other.TargetPlacement.bRotated &&
		PreviewState == Other.PreviewState &&
		SnappedLocalPosition.Equals(Other.SnappedLocalPosition) &&
		SnappedLocalSize.Equals(Other.SnappedLocalSize);
}
