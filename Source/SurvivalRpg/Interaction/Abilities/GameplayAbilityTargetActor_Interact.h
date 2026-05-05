// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "GameplayAbilityTargetActor_Interact.generated.h"

UCLASS(BlueprintType)
class SURVIVALRPG_API AGameplayAbilityTargetActor_Interact : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()

public:
	explicit AGameplayAbilityTargetActor_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;
};
