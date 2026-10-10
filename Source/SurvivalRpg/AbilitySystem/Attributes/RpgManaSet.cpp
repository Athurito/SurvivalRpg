#include "RpgManaSet.h"

#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

URpgManaSet::URpgManaSet()
	: Mana(100.0f)
	, MaxMana(100.0f)
{
}

void URpgManaSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampManaAttribute(Attribute, NewValue);
}

void URpgManaSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampManaAttribute(Attribute, NewValue);
}

void URpgManaSet::PostAttributeChange(const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxManaAttribute() && GetMana() > NewValue)
	{
		if (URpgAbilitySystemComponent* AbilitySystemComponent = GetRpgAbilitySystemComponent())
		{
			AbilitySystemComponent->ApplyModToAttribute(GetManaAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void URpgManaSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgManaSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgManaSet, MaxMana, COND_None, REPNOTIFY_Always);
}

void URpgManaSet::OnRep_Mana(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgManaSet, Mana, OldValue);
}

void URpgManaSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgManaSet, MaxMana, OldValue);
}

void URpgManaSet::ClampManaAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetManaAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.0f, GetMaxMana());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
