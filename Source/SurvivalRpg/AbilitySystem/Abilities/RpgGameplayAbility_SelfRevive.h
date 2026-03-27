// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_SelfRevive.generated.h"

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_SelfRevive : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_SelfRevive();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnSelfReviveFinished();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.0"))
	float SelfReviveDelay = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ReviveHealthPercent = 0.3f;
};
