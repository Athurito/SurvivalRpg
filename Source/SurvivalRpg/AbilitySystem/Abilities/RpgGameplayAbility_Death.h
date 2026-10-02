// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_Death.generated.h"

/** Abstract server death lifecycle: cancels incompatible abilities and guarantees terminal health cleanup. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_Death : public URpgGameplayAbility
{
	GENERATED_BODY()
	
public:

	URpgGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Starts the authoritative health-component death lifecycle once; callable by designer presentation flow. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability")
	void StartDeath();

	/** Finishes an already-started authoritative death lifecycle; repeated calls are harmless. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Ability")
	void FinishDeath();

protected:

	/** Designer opt-in to start death on activation; native end cleanup always finishes a started death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Death")
	bool bAutoStartDeath = false;
};
