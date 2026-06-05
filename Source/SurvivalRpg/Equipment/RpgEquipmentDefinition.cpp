#include "RpgEquipmentDefinition.h"

#include "RpgEquipmentInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentDefinition)

FName FRpgEquipmentActorToSpawn::GetAttachSocketForSlot(ERpgEquipmentSlot Slot) const
{
	if (Slot == ERpgEquipmentSlot::MainHand && !MainHandAttachSocket.IsNone())
	{
		return MainHandAttachSocket;
	}

	if (Slot == ERpgEquipmentSlot::OffHand && !OffHandAttachSocket.IsNone())
	{
		return OffHandAttachSocket;
	}

	return AttachSocket;
}

URpgEquipmentDefinition::URpgEquipmentDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = URpgEquipmentInstance::StaticClass();
	AllowedSlots.Add(ERpgEquipmentSlot::MainHand);
}

bool URpgEquipmentDefinition::CanEquipInSlot(ERpgEquipmentSlot Slot) const
{
	if (Slot == ERpgEquipmentSlot::None)
	{
		return false;
	}

	return AllowedSlots.IsEmpty() ? Slot == ERpgEquipmentSlot::MainHand : AllowedSlots.Contains(Slot);
}

bool URpgEquipmentDefinition::OccupiesSlot(ERpgEquipmentSlot EquippedSlot, ERpgEquipmentSlot QuerySlot) const
{
	if (EquippedSlot == ERpgEquipmentSlot::None || QuerySlot == ERpgEquipmentSlot::None)
	{
		return false;
	}

	if (HandOccupancy == ERpgEquipmentHandOccupancy::BothHands)
	{
		return QuerySlot == ERpgEquipmentSlot::MainHand || QuerySlot == ERpgEquipmentSlot::OffHand;
	}

	return EquippedSlot == QuerySlot;
}

ERpgEquipmentSlot URpgEquipmentDefinition::GetDefaultEquipSlot() const
{
	return AllowedSlots.IsEmpty() ? ERpgEquipmentSlot::MainHand : AllowedSlots[0];
}
