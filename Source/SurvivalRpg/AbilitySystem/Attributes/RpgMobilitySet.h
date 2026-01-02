// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "UObject/Object.h"
#include "RpgMobilitySet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgMobilitySet : public URpgAttributeSet
{
	GENERATED_BODY()
public:	
	//Movement Attributes
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MovementSpeed, Category = "Movement Attributes")
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS_BASIC(URpgMobilitySet, MovementSpeed)
	
	
	UFUNCTION() void OnRep_MovementSpeed(const FGameplayAttributeData& OldValue) const;
	
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
};
