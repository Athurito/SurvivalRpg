// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_SelfRevive.generated.h"

/** Abstract server revive mechanism coordinating lifecycle reset, downed reservations and native health mutation. */
UCLASS(Abstract, Blueprintable)
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

	/** Designer delay in seconds before the server restores this avatar; zero completes immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.0", Units = "s"))
	float SelfReviveDelay = 0.0f;

	/** Designer fraction of maximum health restored by the server outside the downed-component revive path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Revive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReviveHealthPercent = 0.0f;
};
