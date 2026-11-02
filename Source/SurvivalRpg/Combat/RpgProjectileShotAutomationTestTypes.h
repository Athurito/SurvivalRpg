#pragma once

#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgProjectileShotAutomationTestTypes.generated.h"

class URpgAbilityTask_ProjectileShot;

/** Editor-only straight projectile without damage for projectile shot tests. */
UCLASS(NotBlueprintable, Transient)
class ARpgProjectileShotAutomationTestProjectile : public ARpgCombatProjectile
{
	GENERATED_BODY()

public:
	explicit ARpgProjectileShotAutomationTestProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only projectile that destroys itself in BeginPlay, so a shot's finished spawn fails. */
UCLASS(NotBlueprintable, Transient)
class ARpgProjectileShotAutomationFailingProjectile final : public ARpgProjectileShotAutomationTestProjectile
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};

/** Editor-only 1x1 stackable ammunition that launches ARpgProjectileShotAutomationTestProjectile. */
UCLASS(NotBlueprintable, Transient)
class URpgProjectileShotAutomationArrowDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgProjectileShotAutomationArrowDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 1x1 stackable ammunition whose projectile cannot finish spawning. */
UCLASS(NotBlueprintable, Transient)
class URpgProjectileShotAutomationFailingArrowDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgProjectileShotAutomationFailingArrowDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only instant stamina change with SetByCaller Data.StaminaDelta, like the sprint's GE_RpgGasp_StaminaDelta. */
UCLASS(NotBlueprintable)
class URpgProjectileShotAutomationStaminaEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit URpgProjectileShotAutomationStaminaEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * Editor-only shot ability: starts a projectile shot with its instance's ShotDefinition and AmmunitionPolicy, records
 * how the shot ended and ends itself, as GA_BowShot does.
 */
UCLASS(NotBlueprintable)
class URpgProjectileShotAutomationAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgProjectileShotAutomationAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FRpgProjectileShotDefinition ShotDefinition;
	ERpgProjectileAmmunitionPolicy AmmunitionPolicy = ERpgProjectileAmmunitionPolicy::ShooterInventory;
	/** False keeps the ability active after its release, as content with a release montage does. */
	bool bEndOnRelease = true;

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilityTask_ProjectileShot> ShotTask;

	int32 ReleasedCount = 0;
	int32 CancelledCount = 0;
	float LastDrawFraction = -1.0f;
	ERpgProjectileShotResult LastResult = ERpgProjectileShotResult::Interrupted;
	TWeakObjectPtr<ARpgCombatProjectile> LastProjectile;

	void ResetObservations();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void HandleReleased(float DrawFraction, ARpgCombatProjectile* Projectile);

	UFUNCTION()
	void HandleCancelled(ERpgProjectileShotResult Reason);
};
