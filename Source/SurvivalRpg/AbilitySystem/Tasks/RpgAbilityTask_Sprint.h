#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayAbilitySpec.h"
#include "GameplayPrediction.h"
#include "RpgAbilityTask_Sprint.generated.h"

class ACharacter;
class UGameplayEffect;
class URpgAbilitySystemComponent;
class URpgCharacterMovementComponent;
class URpgStaminaSet;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgSprintStopped);

/** Owns one GAS sprint lease; only authoritative, effective CMC movement consumes stamina. */
UCLASS()
class SURVIVALRPG_API URpgAbilityTask_Sprint : public UAbilityTask
{
	GENERATED_BODY()

public:
	/** The lease has already been released. The owning Blueprint ability should end this activation. */
	UPROPERTY(BlueprintAssignable)
	FRpgSprintStopped OnStopped;

	/**
	 * Holds the CMC's authored SprintSpeed until task/ability end. Idle and block suspend effective sprint and cost.
	 * The designer effect must be Instant with one additive Stamina modifier using StaminaDeltaTag as SetByCaller.
	 * StaminaPerSecond is units per second of actual sprint movement; MinimumStaminaToStart is an activation threshold.
	 * Movement is locally predicted; resource changes are authority-only. Exhaustion or an ability end while input remains
	 * held consumes that press until physical release, including authority termination arriving before the resource update.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Start Rpg Sprint", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_Sprint* StartSprint(UGameplayAbility* OwningAbility,
		TSubclassOf<UGameplayEffect> StaminaChangeEffect, FGameplayTag StaminaDeltaTag,
		float StaminaPerSecond, float MinimumStaminaToStart);

	virtual void Activate() override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void HandleStaminaChanged(const FOnAttributeChangeData& Change);
	bool MatchesActivation() const;
	bool HasLiveBinding() const;
	void StopSprint(bool bExhausted);
	void ReleaseResources();

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ChangeEffect;
	FGameplayTag DeltaTag;
	float DrainRate = 0.f;
	float StartThreshold = 0.f;
	TWeakObjectPtr<URpgAbilitySystemComponent> CapturedASC;
	TWeakObjectPtr<ACharacter> CapturedCharacter;
	TWeakObjectPtr<URpgCharacterMovementComponent> CapturedMovement;
	TWeakObjectPtr<const URpgStaminaSet> CapturedStamina;
	FGameplayAbilitySpecHandle CapturedSpec;
	FPredictionKey CapturedActivationKey;
	FDelegateHandle StaminaChangedHandle;
	uint32 MovementLease = 0;
	bool bAuthority = false;
	bool bStopped = false;
};
