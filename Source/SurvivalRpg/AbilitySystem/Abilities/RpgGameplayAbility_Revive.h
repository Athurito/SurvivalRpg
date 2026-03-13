// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_Revive.generated.h"

class URpgDownedComponent;

/**
 * Revive ability — activated on the reviver, targets a downed ally.
 * Uses WaitDelay to handle the cast time. On completion, calls CompleteRevive on the target's DownedComponent.
 * The target is passed via the GameplayEvent payload (TriggerEventData->Target).
 */
UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_Revive : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Revive();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Called when the cast time finishes. Completes the revive on the target. */
	UFUNCTION()
	void OnReviveCastFinished();

	/** How long the revive cast takes in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.5"))
	float ReviveDuration = 5.0f;

private:
	/** Cached reference to the downed target being revived. */
	UPROPERTY()
	TWeakObjectPtr<AActor> ReviveTarget;
};
