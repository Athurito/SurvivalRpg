// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_Death.generated.h"

class UAnimMontage;

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

	/**
	 * Death montage for this activation: the pawn's combat reaction profile (PawnData) picks one by the killer's side
	 * from the event, otherwise DeathMontage. Server activation only; null plays none.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Ability")
	UAnimMontage* GetDeathMontage(const FGameplayEventData& EventData) const;

protected:

	/** Designer opt-in to start death on activation; native end cleanup always finishes a started death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Death")
	bool bAutoStartDeath = false;

	/**
	 * Designer cue the server executes on the dying character when death starts, for every machine. Location is the
	 * character, Instigator the killer from the death event. Empty plays none.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Death", meta = (Categories = "GameplayCue"))
	FGameplayTag DeathCueTag;

	/** Designer fallback death montage for pawns whose reaction profile names none. It should hold its last pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Death")
	TObjectPtr<UAnimMontage> DeathMontage;
};
