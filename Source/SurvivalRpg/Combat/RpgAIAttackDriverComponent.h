#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "RpgAIAttackDriverComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class URpgAbilitySystemComponent;
class URpgHealthComponent;

/** How an AI chooses its combat target among hostile pawns while a StateTree controls the driver. */
UENUM(BlueprintType)
enum class ERpgAICombatTargetPolicy : uint8
{
	/** The nearest hostile pawn; switches to a closer one at most once per switch cooldown. */
	Nearest,
	/** The last hostile that damaged this pawn, at most once per switch cooldown; the nearest until one does. */
	LastAttacker,
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent, DisplayName = "AI Melee Attack Driver"))
class SURVIVALRPG_API URpgAIAttackDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgAIAttackDriverComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Server: makes NewTarget the combat target and schedules an attack after the retaliation delay. Confirming the
	 * current target again keeps an attack that is already scheduled, so callers may confirm it every tick. Under external
	 * control it only sets the target.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void SetCombatTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void ClearCombatTarget();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|AI Combat")
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	/**
	 * Server: hands target choice and attacks to an external controller, normally the StateTree's combat control task.
	 * The driver then stops its own attack and acquisition timers, attacks only through StartAttackSequence and chooses
	 * targets by InTargetPolicy, switching at most once per InTargetSwitchCooldown seconds. Runtime state, not replicated.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void BeginExternalControl(ERpgAICombatTargetPolicy InTargetPolicy, float InTargetSwitchCooldown);

	/** Server: ends external control, stops a running attack sequence and returns to the timer-driven attacks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void EndExternalControl();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|AI Combat")
	bool IsExternallyControlled() const { return bExternalControl; }

	/**
	 * Server, external control: keeps the current target while it is valid and within LoseRadius (cm), otherwise picks
	 * the nearest hostile within SearchRadius (cm). The Nearest policy also moves to a closer hostile after the switch
	 * cooldown. Returns the combat target, or null.
	 */
	AActor* RefreshCombatTarget(float SearchRadius, float LoseRadius);

	/**
	 * Server, external control: faces the target and presses the attack input, then presses once more per running
	 * combo step until StepCount steps (1 or more) have started; the weapon's chain may end earlier. False when the
	 * attack cannot start now: no valid target, a blocking state tag, no primary attack, or an activation failure.
	 */
	bool StartAttackSequence(int32 StepCount);

	/** Server: stops pressing further steps; the running step plays out. */
	void CancelAttackSequence();

	/** True from StartAttackSequence until the attack ability ends. */
	bool IsAttackSequenceActive() const { return bAttackSequenceActive; }

	/** Combo steps the running sequence has started so far, the entry attack included. */
	int32 GetAttackSequenceStepsStarted() const { return AttackSequenceStepsStarted; }

	/** Designer attack reach in cm from the owner's to the target's location, used by the timers and the StateTree. */
	float GetAttackRange() const { return AttackRange; }

	/**
	 * Server: counts the combat target's activations of abilities tagged Ability.Type.Attack, across target changes. The
	 * StateTree's combat control task turns each increase into one StateTree event. Runtime state, not replicated.
	 */
	uint32 GetTargetAttackCount() const { return TargetAttackCount; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Server attack attempts so far, successful or not. */
	uint32 GetAttackAttemptCountForTests() const { return AttackAttemptCountForTests; }
#endif

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

private:
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
	bool IsOwnerAlive() const;
	bool IsValidCombatTarget(const AActor* Target) const;
	bool CanAttemptAttack(bool bRequireInterval) const;
	bool IsInAttackRangeAndCone(const AActor* Target) const;
	bool HasActiveMainHandPrimaryAttack() const;
	AActor* FindNearestHostileTarget(float SearchRadius) const;
	float GetNextAttackInterval() const;
	void FaceTarget(AActor* Target) const;
	void StartTargetAcquisitionIfNeeded();
	void StopDriverActivity();
	void ScheduleNextAttack(float Delay);
	void ScheduleTargetAcquisition(float Delay);
	void TryAttack();
	void TryAcquireNearbyTarget();
	void ReleaseAttackInput();
	bool ShouldDebugLog() const;
	FGameplayTag GetResolvedAttackInputTag() const;
	/** Sets the target and moves the attack observation to the new target's ability system. */
	void SetCombatTargetInternal(AActor* NewTarget);
	void HandleTargetAbilityActivated(UGameplayAbility* ActivatedAbility);
	void UnbindTargetAbilitySystem();
	/** Presses and releases the attack input once, as a player tap would. */
	void TapAttackInput(URpgAbilitySystemComponent& ASC) const;
	void TickAttackSequence();
	void FinishAttackSequence();

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (Categories = "InputTag"))
	FGameplayTag AttackInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AttackConeDegrees = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float MinAttackInterval = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackIntervalRandomJitter = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float RetaliationDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackRetryInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bAcquireNearbyHostileTargets = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float TargetAcquisitionRadius = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.05"))
	float TargetAcquisitionInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bUseLastHostileDamageInstigator = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bFaceTargetBeforeAttack = true;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> CachedHealthComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CombatTarget;

	/** Ability system of the combat target whose attack activations are counted. */
	TWeakObjectPtr<UAbilitySystemComponent> ObservedTargetAbilitySystem;
	FDelegateHandle TargetAbilityActivatedHandle;
	uint32 TargetAttackCount = 0;

	bool bExternalControl = false;
	ERpgAICombatTargetPolicy TargetPolicy = ERpgAICombatTargetPolicy::LastAttacker;
	float TargetSwitchCooldown = 0.0f;
	float LastTargetSwitchTime = -1000.0f;

	bool bAttackSequenceActive = false;
	int32 AttackSequenceStepCount = 0;
	int32 AttackSequenceStepsStarted = 0;
	/** Combo step during which the press for the following step was sent; -1 before the first. */
	int32 AttackSequencePressedDuringStep = -1;
	FGameplayAbilitySpecHandle AttackSequenceSpecHandle;
	FTimerHandle AttackSequenceTimerHandle;

	FTimerHandle AttackTimerHandle;
	FTimerHandle TargetAcquisitionTimerHandle;
	FTimerHandle ReleaseAttackInputTimerHandle;

	float LastAttackTime = -1000.0f;
	bool bAttackInputHeld = false;

#if WITH_DEV_AUTOMATION_TESTS
	uint32 AttackAttemptCountForTests = 0;
#endif
};
