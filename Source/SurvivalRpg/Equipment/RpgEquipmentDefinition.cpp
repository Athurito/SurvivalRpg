#include "RpgEquipmentDefinition.h"

#include "RpgEquipmentInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentDefinition)

#define LOCTEXT_NAMESPACE "RpgEquipmentDefinition"

namespace
{
#if WITH_EDITOR
	FText GetEquipmentDefinitionValidationPath(const UObject* Definition)
	{
		const FString ObjectPath = GetPathNameSafe(Definition);
		const FString PackagePath =
			Definition && Definition->GetOutermost()
				? Definition->GetOutermost()->GetName()
				: FString();
		return FText::FromString(
			PackagePath.IsEmpty() || PackagePath == TEXT("/Engine/Transient")
				? ObjectPath
				: PackagePath);
	}
#endif
}

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

bool URpgEquipmentDefinition::IsConcreteEquipmentSlot(
	ERpgEquipmentSlot Slot)
{
	switch (Slot)
	{
	case ERpgEquipmentSlot::MainHand:
	case ERpgEquipmentSlot::OffHand:
	case ERpgEquipmentSlot::Head:
	case ERpgEquipmentSlot::Chest:
	case ERpgEquipmentSlot::Hands:
	case ERpgEquipmentSlot::Legs:
	case ERpgEquipmentSlot::Feet:
	case ERpgEquipmentSlot::Backpack:
	case ERpgEquipmentSlot::Belt:
	case ERpgEquipmentSlot::Pouch:
	case ERpgEquipmentSlot::ResourceBag:
		return true;

	case ERpgEquipmentSlot::None:
	default:
		return false;
	}
}

bool URpgEquipmentDefinition::HasStructurallyValidSlotReferences() const
{
	TSet<ERpgEquipmentSlot> UniqueAllowedSlots;
	for (const ERpgEquipmentSlot AllowedSlot : AllowedSlots)
	{
		if (!IsConcreteEquipmentSlot(AllowedSlot) ||
			UniqueAllowedSlots.Contains(AllowedSlot))
		{
			return false;
		}
		UniqueAllowedSlots.Add(AllowedSlot);
	}

	for (const FRpgEquipmentSlotAbilitySet& SlotAbilitySet :
		SlotAbilitySetsToGrant)
	{
		if (!SlotAbilitySet.AbilitySet ||
			!IsConcreteEquipmentSlot(SlotAbilitySet.EquippedSlot) ||
			!UniqueAllowedSlots.Contains(SlotAbilitySet.EquippedSlot))
		{
			return false;
		}
	}

	return true;
}

bool URpgEquipmentDefinition::CanEquipInSlot(ERpgEquipmentSlot Slot) const
{
	if (!IsConcreteEquipmentSlot(Slot))
	{
		return false;
	}

	return AllowedSlots.Contains(Slot) &&
		!(Slot == ERpgEquipmentSlot::OffHand &&
			HandOccupancy == ERpgEquipmentHandOccupancy::BothHands);
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
	for (const ERpgEquipmentSlot AllowedSlot : AllowedSlots)
	{
		if (CanEquipInSlot(AllowedSlot))
		{
			return AllowedSlot;
		}
	}

	return ERpgEquipmentSlot::None;
}

#if WITH_EDITOR
EDataValidationResult URpgEquipmentDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (HasStructurallyValidSlotReferences())
	{
		return Result;
	}

	Result = EDataValidationResult::Invalid;
	const FText DefinitionPath =
		GetEquipmentDefinitionValidationPath(this);
	TSet<ERpgEquipmentSlot> UniqueAllowedSlots;
	for (int32 SlotIndex = 0; SlotIndex < AllowedSlots.Num(); ++SlotIndex)
	{
		const ERpgEquipmentSlot AllowedSlot = AllowedSlots[SlotIndex];
		if (!IsConcreteEquipmentSlot(AllowedSlot))
		{
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"InvalidAllowedSlot",
						"Equipment definition '{0}' has an invalid or None slot at AllowedSlots[{1}]. "
						"Choose a concrete equipment slot or remove the entry."),
					DefinitionPath,
					FText::AsNumber(SlotIndex)));
			continue;
		}

		if (UniqueAllowedSlots.Contains(AllowedSlot))
		{
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"DuplicateAllowedSlot",
						"Equipment definition '{0}' repeats slot '{1}' at AllowedSlots[{2}]. "
						"Keep each allowed slot exactly once."),
					DefinitionPath,
					FText::FromString(
						StaticEnum<ERpgEquipmentSlot>()->GetNameStringByValue(
							static_cast<int64>(AllowedSlot))),
					FText::AsNumber(SlotIndex)));
			continue;
		}

		UniqueAllowedSlots.Add(AllowedSlot);
	}

	for (int32 GrantIndex = 0;
		GrantIndex < SlotAbilitySetsToGrant.Num();
		++GrantIndex)
	{
		const FRpgEquipmentSlotAbilitySet& SlotAbilitySet =
			SlotAbilitySetsToGrant[GrantIndex];
		if (!SlotAbilitySet.AbilitySet)
		{
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"MissingSlotAbilitySet",
						"Equipment definition '{0}' has no AbilitySet at SlotAbilitySetsToGrant[{1}]. "
						"Assign an AbilitySet or remove the row."),
					DefinitionPath,
					FText::AsNumber(GrantIndex)));
		}

		if (!IsConcreteEquipmentSlot(SlotAbilitySet.EquippedSlot))
		{
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"InvalidSlotAbilitySetSlot",
						"Equipment definition '{0}' has an invalid or None EquippedSlot at "
						"SlotAbilitySetsToGrant[{1}]. Choose one concrete slot from AllowedSlots."),
					DefinitionPath,
					FText::AsNumber(GrantIndex)));
		}
		else if (!UniqueAllowedSlots.Contains(
			SlotAbilitySet.EquippedSlot))
		{
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"UnreachableSlotAbilitySet",
						"Equipment definition '{0}' targets slot '{1}' at SlotAbilitySetsToGrant[{2}], but that slot "
						"is not present exactly once in AllowedSlots. Add the slot or retarget/remove the row."),
					DefinitionPath,
					FText::FromString(
						StaticEnum<ERpgEquipmentSlot>()->GetNameStringByValue(
							static_cast<int64>(
								SlotAbilitySet.EquippedSlot))),
					FText::AsNumber(GrantIndex)));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
