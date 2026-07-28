#include "RpgGatheringSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGatheringSet)

URpgGatheringSet::URpgGatheringSet()
	: YieldBonus(0.0f)
	, RareFindBonus(0.0f)
{
}

void URpgGatheringSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetYieldBonusAttribute() || Attribute == GetRareFindBonusAttribute())
	{
		NewValue = FMath::Max(-0.95f, NewValue);
	}
}

void URpgGatheringSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetYieldBonusAttribute() || Attribute == GetRareFindBonusAttribute())
	{
		NewValue = FMath::Max(-0.95f, NewValue);
	}
}

void URpgGatheringSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgGatheringSet, YieldBonus, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgGatheringSet, RareFindBonus, COND_OwnerOnly, REPNOTIFY_Always);
}

void URpgGatheringSet::OnRep_YieldBonus(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgGatheringSet, YieldBonus, OldValue);
}

void URpgGatheringSet::OnRep_RareFindBonus(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgGatheringSet, RareFindBonus, OldValue);
}
