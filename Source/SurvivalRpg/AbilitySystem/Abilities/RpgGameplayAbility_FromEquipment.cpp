#include "RpgGameplayAbility_FromEquipment.h"

#include "AbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

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
