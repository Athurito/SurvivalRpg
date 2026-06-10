#include "RpgInventoryCapacitySet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryCapacitySet)

URpgInventoryCapacitySet::URpgInventoryCapacitySet()
	: BackpackSlots(24.0f)
{
}

void URpgInventoryCapacitySet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetBackpackSlotsAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void URpgInventoryCapacitySet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetBackpackSlotsAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void URpgInventoryCapacitySet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgInventoryCapacitySet, BackpackSlots, COND_None, REPNOTIFY_Always);
}

void URpgInventoryCapacitySet::OnRep_BackpackSlots(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgInventoryCapacitySet, BackpackSlots, OldValue);
}
