// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "RpgAttributeSet.generated.h"

struct FGameplayEffectSpec;
class URpgAbilitySystemComponent;

/** 
 * Delegate used to broadcast attribute events, some of these parameters may be null on clients: 
 * @param EffectInstigator	The original instigating actor for this event
 * @param EffectCauser		The physical actor that caused the change
 * @param EffectSpec		The full effect spec for this change
 * @param EffectMagnitude	The raw magnitude, this is before clamping
 * @param OldValue			The value of the attribute before it was changed
 * @param NewValue			The value after it was changed
*/
DECLARE_MULTICAST_DELEGATE_SixParams(FRpgAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, const FGameplayEffectSpec* /*EffectSpec*/, float /*EffectMagnitude*/, float /*OldValue*/, float /*NewValue*/);



UCLASS()
class SURVIVALRPG_API URpgAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	URpgAttributeSet();
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
protected:
	
	static float ClampAttribute(const float Value, const float Min, const float Max);
};
