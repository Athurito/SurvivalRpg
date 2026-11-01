#include "RpgFocusSet.h"

#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgFocusSet)

URpgFocusSet::URpgFocusSet()
	: Focus(0.0f)
	, MaxFocus(100.0f)
{
}

void URpgFocusSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampFocusAttribute(Attribute, NewValue);
}

void URpgFocusSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampFocusAttribute(Attribute, NewValue);
}

void URpgFocusSet::PostAttributeChange(const FGameplayAttribute& Attribute, const float OldValue, const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	if (Attribute == GetMaxFocusAttribute() && GetFocus() > NewValue)
	{
		if (URpgAbilitySystemComponent* AbilitySystemComponent = GetRpgAbilitySystemComponent())
		{
			AbilitySystemComponent->ApplyModToAttribute(GetFocusAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void URpgFocusSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Partners and late joiners never learn another player's focus.
	DOREPLIFETIME_CONDITION_NOTIFY(URpgFocusSet, Focus, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgFocusSet, MaxFocus, COND_OwnerOnly, REPNOTIFY_Always);
}

void URpgFocusSet::OnRep_Focus(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgFocusSet, Focus, OldValue);
}

void URpgFocusSet::OnRep_MaxFocus(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgFocusSet, MaxFocus, OldValue);
}

void URpgFocusSet::ClampFocusAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetFocusAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.0f, GetMaxFocus());
	}
	else if (Attribute == GetMaxFocusAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
