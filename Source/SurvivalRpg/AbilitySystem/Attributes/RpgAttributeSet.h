// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "RpgAttributeSet.generated.h"

class URpgAbilitySystemComponent;
/**
 * 
 */
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
