// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgPrimarySet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgPrimarySet : public URpgAttributeSet
{
	GENERATED_BODY()
	
public:
	URpgPrimarySet();
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "Primary Attributes")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS_BASIC(URpgPrimarySet, Strength)
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence, Category = "Primary Attributes")
	FGameplayAttributeData Intelligence;
	ATTRIBUTE_ACCESSORS_BASIC(URpgPrimarySet, Intelligence)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Resilience, Category = "Primary Attributes")
	FGameplayAttributeData Resilience;
	ATTRIBUTE_ACCESSORS_BASIC(URpgPrimarySet, Resilience)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Vitality, Category = "Primary Attributes")
	FGameplayAttributeData Vitality;
	ATTRIBUTE_ACCESSORS_BASIC(URpgPrimarySet, Vitality)
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	
protected:
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Resilience(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_Vitality(const FGameplayAttributeData& OldValue) const;
};
