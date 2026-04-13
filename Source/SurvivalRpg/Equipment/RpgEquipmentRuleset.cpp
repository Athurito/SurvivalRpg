#include "RpgEquipmentRuleset.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Items/RpgItemInstance.h"
#include "SurvivalRpg/Items/Fragments/RpgItemFragment_Weapon.h"

bool URpgEquipmentRuleset::IsWeaponSetSlot(const FGameplayTag& SlotTag) const
{
	return IsMainHandSlot(SlotTag) || IsOffHandSlot(SlotTag);
}

bool URpgEquipmentRuleset::IsMainHandSlot(const FGameplayTag& SlotTag) const
{
	return SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_1_MainHand
		|| SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_2_MainHand;
}

bool URpgEquipmentRuleset::IsOffHandSlot(const FGameplayTag& SlotTag) const
{
	return SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_1_OffHand
		|| SlotTag == RpgGameplayTags::Equipment_Slot_WeaponSet_2_OffHand;
}

bool URpgEquipmentRuleset::DoesItemFitSlot(const URpgItemInstance* ItemInstance, const FGameplayTag& SlotTag) const
{
	if (ItemInstance == nullptr || !IsWeaponSetSlot(SlotTag))
	{
		return false;
	}

	const URpgItemFragment_Weapon* WeaponFragment = ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>();
	if (WeaponFragment == nullptr)
	{
		return false;
	}

	const FGameplayTag& HandUsageTag = WeaponFragment->GetHandUsageTag();
	if (IsMainHandSlot(SlotTag))
	{
		return HandUsageTag == RpgGameplayTags::Equipment_HandUsage_MainHand
			|| HandUsageTag == RpgGameplayTags::Equipment_HandUsage_EitherHand
			|| HandUsageTag == RpgGameplayTags::Equipment_HandUsage_TwoHanded;
	}

	return HandUsageTag == RpgGameplayTags::Equipment_HandUsage_OffHand
		|| HandUsageTag == RpgGameplayTags::Equipment_HandUsage_EitherHand;
}

bool URpgEquipmentRuleset::AreItemsCompatible(const URpgItemInstance* MainHandItem, const URpgItemInstance* OffHandItem) const
{
	if (OffHandItem == nullptr)
	{
		return true;
	}

	if (MainHandItem == nullptr)
	{
		return bAllowOffHandWithoutMainHand;
	}

	if (MainHandItem == OffHandItem || IsTwoHanded(MainHandItem) || IsTwoHanded(OffHandItem))
	{
		return false;
	}

	const FGameplayTagContainer MainTags = BuildCompatibilityTags(MainHandItem);
	const FGameplayTagContainer OffTags = BuildCompatibilityTags(OffHandItem);

	if (MatchesCompatibilityRule(BlockedPairings, MainTags, OffTags))
	{
		return false;
	}

	return MatchesCompatibilityRule(AllowedPairings, MainTags, OffTags);
}

bool URpgEquipmentRuleset::IsTwoHanded(const URpgItemInstance* ItemInstance) const
{
	if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>() : nullptr)
	{
		return WeaponFragment->GetHandUsageTag() == RpgGameplayTags::Equipment_HandUsage_TwoHanded;
	}

	return false;
}

bool URpgEquipmentRuleset::IsOffHandOnly(const URpgItemInstance* ItemInstance) const
{
	if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>() : nullptr)
	{
		return WeaponFragment->GetHandUsageTag() == RpgGameplayTags::Equipment_HandUsage_OffHand;
	}

	return false;
}

bool URpgEquipmentRuleset::IsMainHandOnly(const URpgItemInstance* ItemInstance) const
{
	if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>() : nullptr)
	{
		return WeaponFragment->GetHandUsageTag() == RpgGameplayTags::Equipment_HandUsage_MainHand;
	}

	return false;
}

bool URpgEquipmentRuleset::MatchesCompatibilityRule(const TArray<FRpgEquipmentCompatibilityRule>& Rules, const FGameplayTagContainer& LeftTags, const FGameplayTagContainer& RightTags) const
{
	for (const FRpgEquipmentCompatibilityRule& Rule : Rules)
	{
		if (!Rule.LeftTag.IsValid() || !Rule.RightTag.IsValid())
		{
			continue;
		}

		const bool bDirectMatch = LeftTags.HasTagExact(Rule.LeftTag) && RightTags.HasTagExact(Rule.RightTag);
		const bool bReverseMatch = Rule.bBidirectional && LeftTags.HasTagExact(Rule.RightTag) && RightTags.HasTagExact(Rule.LeftTag);
		if (bDirectMatch || bReverseMatch)
		{
			return true;
		}
	}

	return false;
}

FGameplayTagContainer URpgEquipmentRuleset::BuildCompatibilityTags(const URpgItemInstance* ItemInstance) const
{
	if (const URpgItemFragment_Weapon* WeaponFragment = ItemInstance ? ItemInstance->FindFragmentByClass<URpgItemFragment_Weapon>() : nullptr)
	{
		return WeaponFragment->BuildCompatibilityTagContainer();
	}

	return FGameplayTagContainer();
}
