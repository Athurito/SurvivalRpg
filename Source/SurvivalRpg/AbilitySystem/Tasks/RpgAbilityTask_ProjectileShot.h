#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayAbilitySpec.h"
#include "GameplayPrediction.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "RpgAbilityTask_ProjectileShot.generated.h"

class ARpgCombatProjectile;
class URpgAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgProjectileShotNocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgProjectileShotReleased, float, DrawFraction, ARpgCombatProjectile*, Projectile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgProjectileShotCancelled, ERpgProjectileShotResult, Reason);

/**
 * One held projectile shot of the owning ability's activation, such as a bow draw: nock, draw, release.
 *
 * The draw starts when the task activates. The locally controlled machine releases on the ability's input release (or
 * Release Shot) and sends the draw time and camera aim with the activation. The server clamps the draw to the time it
 * observed, checks the aim and launch obstruction, then commits exactly once: deferred spawn, release stamina, one
 * ammunition item, finished spawn, or nothing at all. Releasing before the minimum nocking time cancels without cost.
 * A duplicate release is ignored. Tunable values resolve once when the task starts, so a passive change applies to the
 * next activation. The owner predicts only the presentation; the authority alone launches and pays.
 */
UCLASS()
class SURVIVALRPG_API URpgAbilityTask_ProjectileShot : public UAbilityTask
{
	GENERATED_BODY()

public:
	URpgAbilityTask_ProjectileShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Every machine running the activation: the minimum nocking time passed, so a release now shoots. */
	UPROPERTY(BlueprintAssignable)
	FRpgProjectileShotNocked OnNocked;

	/**
	 * The shot released with DrawFraction from 0 to 1. On the server Projectile is the launched projectile; the owner
	 * predicts it with no projectile. The owning ability should end.
	 */
	UPROPERTY(BlueprintAssignable)
	FRpgProjectileShotReleased OnReleased;

	/** The shot ended without a projectile and without cost, for Reason. The owning ability should end. */
	UPROPERTY(BlueprintAssignable)
	FRpgProjectileShotCancelled OnCancelled;

	/**
	 * Starts a shot with ShotDefinition, for spells, scripted or NPC shots without a ranged weapon. AmmunitionPolicy
	 * chooses the shooter's inventory or the explicit unlimited NPC policy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Start Projectile Shot", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_ProjectileShot* StartProjectileShot(
		UGameplayAbility* OwningAbility,
		const FRpgProjectileShotDefinition& ShotDefinition,
		ERpgProjectileAmmunitionPolicy AmmunitionPolicy = ERpgProjectileAmmunitionPolicy::ShooterInventory);

	/**
	 * Starts a shot with the shot definition of the ability's source weapon (URpgRangedWeaponInstance), such as the
	 * equipped bow. Without a ranged source weapon the shot cancels as InvalidShot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Start Projectile Shot From Weapon", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_ProjectileShot* StartProjectileShotFromWeapon(
		UGameplayAbility* OwningAbility,
		ERpgProjectileAmmunitionPolicy AmmunitionPolicy = ERpgProjectileAmmunitionPolicy::ShooterInventory);

	/** Locally controlled machine: releases now, as an input release does, for example for an NPC's scripted shot. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
	void ReleaseShot();

	/** How far the shot is drawn on this machine, from 0 to 1; presentation only. */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks")
	float GetDrawFraction() const;

	/** Whether the minimum nocking time passed on this machine. */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks")
	bool IsNocked() const;

	/** The tunable values this activation resolved at its start. */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks")
	FRpgProjectileShotTuning GetShotTuning() const { return Tuning; }

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

#if WITH_DEV_AUTOMATION_TESTS
	/** Delivers Release to the server path as a remote owner's target data would, without network. */
	void ReceiveReleaseForTests(const FRpgProjectileShotRelease& Release) { ResolveRelease(Release); }
	ERpgProjectileShotResult GetResultForTests() const { return Result; }
	bool IsResolvedForTests() const { return bResolved; }
#endif

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void HandleInputReleased();
	void HandleReleaseTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void HandleNockTimer();
	void HandleServerTimeout();
	FRpgProjectileShotRelease MakeLocalRelease() const;
	void ResolveRelease(const FRpgProjectileShotRelease& Release);
	void PredictRelease(const FRpgProjectileShotRelease& Release);
	void FinishReleased(float DrawFraction, ARpgCombatProjectile* Projectile);
	void FinishCancelled(ERpgProjectileShotResult Reason);
	double GetElapsedSeconds() const;
	bool MatchesActivation() const;
	void ReleaseBindings();

	FRpgProjectileShotDefinition Definition;
	ERpgProjectileAmmunitionPolicy AmmunitionPolicy = ERpgProjectileAmmunitionPolicy::ShooterInventory;
	bool bDefinitionFromWeapon = false;
	FRpgProjectileShotTuning Tuning;

	TWeakObjectPtr<UAbilitySystemComponent> CapturedASC;
	FGameplayAbilitySpecHandle CapturedSpec;
	FPredictionKey CapturedActivationKey;
	FDelegateHandle InputReleasedHandle;
	FDelegateHandle TargetDataHandle;
	FTimerHandle NockTimerHandle;
	FTimerHandle ServerTimeoutHandle;
	double StartSeconds = 0.0;
	bool bAuthority = false;
	bool bLocallyControlled = false;
	bool bResolved = false;
	bool bNocked = false;
	ERpgProjectileShotResult Result = ERpgProjectileShotResult::Interrupted;
};
