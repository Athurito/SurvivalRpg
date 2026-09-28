#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "TimerManager.h"
#include "RpgAbilityTask_StaminaRegen.generated.h"

class UGameplayEffect;
class URpgAbilitySystemComponent;
class URpgStaminaSet;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgStaminaRegenStopped);

/** Authority-only delayed regeneration for a Blueprint ability; attributes remain passive GAS data. */
UCLASS()
class SURVIVALRPG_API URpgAbilityTask_StaminaRegen : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** The original actor/attribute/activation binding ended or its effect failed; Blueprint should end the ability. */
	UPROPERTY(BlueprintAssignable)
	FRpgStaminaRegenStopped OnStopped;

	/**
	 * Regenerates using the current StaminaRegen attribute in units/second, sampled on a 0.1-second authority timer.
	 * Every negative Stamina change restarts DelaySeconds; full/dead avatars never regenerate or accrue catch-up credit.
	 * Effect must be Instant with one additive Stamina modifier whose SetByCaller tag is StaminaDeltaTag.
	 * Optional RegenerationTuningEffect adds the designer's regeneration rate (Stamina units/second) only after the
	 * canonical StaminaSet exists. It must be Infinite, nonperiodic and nonstacking, with one additive StaminaRegen modifier.
	 * Intended for a server-only OnSpawn Blueprint ability; task end removes its own tuning effect, listeners and timers.
	 * Without a tuning effect, the task uses the existing externally configured rate and never restores attribute values.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Regenerate Rpg Stamina", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_StaminaRegen* RegenerateStamina(UGameplayAbility* OwningAbility,
		TSubclassOf<UGameplayEffect> StaminaChangeEffect, FGameplayTag StaminaDeltaTag, float DelaySeconds,
		TSubclassOf<UGameplayEffect> RegenerationTuningEffect = nullptr);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleStaminaChanged(const FOnAttributeChangeData& Change);
	void InitializeRegeneration();
	void Regenerate();
	bool MatchesActivation() const;
	bool HasLiveBinding() const;
	void StopRegeneration();
	void ReleaseResources();

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ChangeEffect;
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> TuningEffect;
	FActiveGameplayEffectHandle TuningEffectHandle;
	FGameplayTag DeltaTag;
	float RecoveryDelay = 0.f;
	TWeakObjectPtr<URpgAbilitySystemComponent> CapturedASC;
	TWeakObjectPtr<AActor> CapturedAvatar;
	TWeakObjectPtr<const URpgStaminaSet> CapturedStamina;
	TWeakObjectPtr<UWorld> CapturedWorld;
	FGameplayAbilitySpecHandle CapturedSpec;
	FPredictionKey CapturedActivationKey;
	FDelegateHandle StaminaChangedHandle;
	FTimerHandle RegenTimer;
	double LastUpdateTime = 0.;
	double RegenAllowedAt = 0.;
	bool bStopped = false;
};
