#include "RpgPlayerGameplayInputRouterComponent.h"

#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerGameplayInputRouterComponent)

URpgPlayerGameplayInputRouterComponent::URpgPlayerGameplayInputRouterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerGameplayInputRouterComponent::HandleGameplayInputPressed(FGameplayTag InputTag)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !InputTag.IsValid())
	{
		return;
	}

	const int32 ActionBarSlotIndex = GetActionBarSlotIndexFromInputTag(InputTag);
	if (ActionBarSlotIndex != INDEX_NONE)
	{
		if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
		{
			ActionBar->ActivateSlot(ActionBarSlotIndex);
		}
		return;
	}

	const int32 WeaponAbilitySlotIndex = GetWeaponAbilitySlotIndexFromInputTag(InputTag);
	if (WeaponAbilitySlotIndex != INDEX_NONE)
	{
		if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout = RpgPC->GetWeaponAbilityLoadoutComponent())
		{
			WeaponAbilityLoadout->HandleInputPressed(WeaponAbilitySlotIndex);
		}
	}
}

void URpgPlayerGameplayInputRouterComponent::HandleGameplayInputReleased(FGameplayTag InputTag)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !InputTag.IsValid())
	{
		return;
	}

	const int32 ActionBarSlotIndex = GetActionBarSlotIndexFromInputTag(InputTag);
	if (ActionBarSlotIndex != INDEX_NONE)
	{
		if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
		{
			ActionBar->ReleaseSlot(ActionBarSlotIndex);
		}
		return;
	}

	const int32 WeaponAbilitySlotIndex = GetWeaponAbilitySlotIndexFromInputTag(InputTag);
	if (WeaponAbilitySlotIndex != INDEX_NONE)
	{
		if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout = RpgPC->GetWeaponAbilityLoadoutComponent())
		{
			WeaponAbilityLoadout->HandleInputReleased(WeaponAbilitySlotIndex);
		}
	}
}

int32 URpgPlayerGameplayInputRouterComponent::GetActionBarSlotIndexFromInputTag(FGameplayTag InputTag)
{
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_1)
	{
		return 0;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_2)
	{
		return 1;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_3)
	{
		return 2;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_4)
	{
		return 3;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_5)
	{
		return 4;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_6)
	{
		return 5;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_7)
	{
		return 6;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_8)
	{
		return 7;
	}

	return INDEX_NONE;
}

int32 URpgPlayerGameplayInputRouterComponent::GetWeaponAbilitySlotIndexFromInputTag(FGameplayTag InputTag)
{
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_1)
	{
		return 0;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_2)
	{
		return 1;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_3)
	{
		return 2;
	}

	return INDEX_NONE;
}
