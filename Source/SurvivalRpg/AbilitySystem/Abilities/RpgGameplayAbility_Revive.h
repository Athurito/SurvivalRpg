// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "RpgGameplayAbility_Revive.generated.h"

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_Revive : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Revive();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnReviveCastFinished();

	/** Revalidates range, line of sight, target state, and interaction revision while the revive is channeling. */
	UFUNCTION()
	void ValidateReviveInteraction();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.5"))
	float ReviveDuration = 5.0f;

private:
	bool IsReviveInteractionStillValid() const;

	UPROPERTY()
	TWeakObjectPtr<AActor> ReviveTarget;

	UPROPERTY()
	FGameplayEventData ReviveInteractionEventData;

	UPROPERTY()
	FInteractionOption ActiveReviveOption;

	FTimerHandle ReviveValidationTimerHandle;
	bool bInteractionStarted = false;
};
