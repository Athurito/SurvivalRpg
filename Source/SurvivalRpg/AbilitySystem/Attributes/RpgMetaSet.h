// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgMetaSet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgMetaSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgMetaSet();
	ATTRIBUTE_ACCESSORS_BASIC(URpgMetaSet, Healing);
	ATTRIBUTE_ACCESSORS_BASIC(URpgMetaSet, Damage);

private:
	
	
	// -------------------------------------------------------------------
	//	Meta Attribute (please keep attributes that aren't 'stateful' below 
	// -------------------------------------------------------------------

	// Incoming healing. This is mapped directly to +Health
	UPROPERTY(BlueprintReadOnly, Category="Rpg|Health", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Healing;

	// Incoming damage. This is mapped directly to -Health
	UPROPERTY(BlueprintReadOnly, Category="Rpg|Health", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Damage;
};
