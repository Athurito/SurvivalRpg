#include "RpgInventorySlotGroupViewModel.h"

#include "SurvivalRpg/Inventory/RpgInventoryEquipmentPlacementPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventorySlotGroupViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags InventorySlotGroupTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	bool IsTypedCarryGroupView(
		const FRpgInventorySlotGroupView& GroupView)
	{
		return GroupView.GroupKind == ERpgInventorySlotGroupKind::Carry &&
			FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
				GroupView.EquipmentSlotRole);
	}

	template <typename ViewModelType>
	bool AreInventorySlotGroupViewModelArraysEqual(
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
		!DisplayName.IdenticalTo(NewDisplayName, InventorySlotGroupTextIdentityFlags);
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
		!AreInventorySlotGroupViewModelArraysEqual(Slots, NewSlots);

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
