#include "RpgGameplayAbility_FromEquipment.h"

#include "AbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgGameplayAbility_FromEquipment::URpgGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

URpgEquipmentInstance* URpgGameplayAbility_FromEquipment::GetAssociatedEquipment() const
{
	if (const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec())
	{
		return Cast<URpgEquipmentInstance>(AbilitySpec->SourceObject.Get());
	}

	return nullptr;
}

URpgInventoryItemInstance* URpgGameplayAbility_FromEquipment::GetAssociatedItem() const
{
	if (URpgEquipmentInstance* EquipmentInstance = GetAssociatedEquipment())
	{
		return Cast<URpgInventoryItemInstance>(EquipmentInstance->GetInstigator());
	}

	return nullptr;
}

FGameplayTag URpgGameplayAbility_FromEquipment::GetInputTagFromSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	if (!AbilitySpec)
	{
		return FGameplayTag();
	}

	const FGameplayTagContainer& SourceTags = AbilitySpec->GetDynamicSpecSourceTags();
	if (SourceTags.HasTagExact(RpgGameplayTags::InputTag_Weapon_Primary))
	{
		return RpgGameplayTags::InputTag_Weapon_Primary;
	}
	if (SourceTags.HasTagExact(RpgGameplayTags::InputTag_Weapon_Secondary))
	{
		return RpgGameplayTags::InputTag_Weapon_Secondary;
	}
	if (SourceTags.HasTagExact(RpgGameplayTags::InputTag_Weapon_Block))
	{
		return RpgGameplayTags::InputTag_Weapon_Block;
	}

	return FGameplayTag();
}

bool URpgGameplayAbility_FromEquipment::IsEquipmentActiveForInput(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const
{
	if (!EquipmentInstance)
	{
		return false;
	}

	const APawn* Pawn = EquipmentInstance->GetPawn();
	const URpgEquipmentManagerComponent* EquipmentManager = Pawn ? Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
	return !EquipmentManager || EquipmentManager->IsEquipmentInstanceActiveForInputTag(EquipmentInstance, InputTag);
}
