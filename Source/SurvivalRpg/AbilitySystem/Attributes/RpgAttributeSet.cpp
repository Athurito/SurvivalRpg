// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAttributeSet.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

URpgAttributeSet::URpgAttributeSet()
{
}

URpgAbilitySystemComponent* URpgAttributeSet::GetRpgAbilitySystemComponent() const
{
	return Cast<URpgAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

float URpgAttributeSet::ClampAttribute(const float Value, const float Min, const float Max)
{
	return FMath::Clamp(Value, Min, Max);
}
