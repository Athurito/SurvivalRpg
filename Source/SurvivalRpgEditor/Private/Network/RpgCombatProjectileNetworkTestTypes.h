#pragma once

#include "GameFramework/GameModeBase.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Combat/RpgProjectileShot.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include "RpgCombatProjectileNetworkTestTypes.generated.h"

class UBoxComponent;
class URpgEmbeddedProjectileHostComponent;
class URpgInventoryManagerComponent;
class URpgLootSourceComponent;
class USkeletalMeshComponent;
class URpgAbilityTask_ProjectileShot;

/**
 * Plain game mode for the projectile network tests: real RPG player states in faction 1 and the RPG game state, whose
 * ability system sends the projectile impact cues. No Experience loads.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationProjectileGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationProjectileGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Real RPG player state and ability system in faction 1, without Experience-bound PawnData. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationProjectileShooterState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	ARpgNetworkAutomationProjectileShooterState();

	virtual void PostInitializeComponents() override;
};

/** Instant damage through the production damage execution, like GE_Damage_Instant, without loading content. */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationProjectileDamageEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationProjectileDamageEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * A straight, slow projectile with the fireball impact cue, so a test can watch it fly on every machine. It lingers two
 * seconds after its stop, long enough for every client to show the stop before the actor goes.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationTestProjectile : public ARpgCombatProjectile
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationTestProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static constexpr float FlightSpeed = 1500.0f;
	static constexpr float ShotDamage = 12.0f;
};

/** The test projectile with full gravity, like an arrow, so a stopped copy that keeps simulating would visibly fall. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationFallingTestProjectile final : public ARpgNetworkAutomationTestProjectile
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationFallingTestProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** The slow test projectile, recoverable: an intact impact leaves a plain ARpgEmbeddedProjectilePickup. No impact cue. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationRecoverableProjectile final : public ARpgNetworkAutomationTestProjectile
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationRecoverableProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** A replicated stand-in for an AI character's body: the Manny skeletal mesh, corpse loot and the arrow host. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationArrowBody final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationArrowBody(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	USkeletalMeshComponent* GetBodyMesh() const { return Mesh; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<URpgInventoryManagerComponent> Loot;

	UPROPERTY()
	TObjectPtr<URpgLootSourceComponent> LootSource;

	UPROPERTY()
	TObjectPtr<URpgEmbeddedProjectileHostComponent> Host;
};

/** A replicated, movable wall without an ability system that projectiles hit and recovered arrows embed in. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationRecoveryWall final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationRecoveryWall(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Half the wall's thickness along X in cm; its front face lies this far before its location. */
	static constexpr float HalfThickness = 20.0f;

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Wall;
};

/**
 * Locally predicted ability with no equipment whose server activation launches ARpgNetworkAutomationTestProjectile at
 * the first ARpgCombatNetworkTargetFixture through ARpgCombatProjectile::LaunchFromAbility, inside the client's
 * prediction window.
 */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationProjectileAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationProjectileAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static constexpr int32 ShotId = 4242;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

/** Stackable test ammunition that launches ARpgNetworkAutomationTestProjectile, for the bow shot network test. */
UCLASS(NotBlueprintable, Transient)
class URpgNetworkAutomationArrowDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationArrowDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Instant stamina change with SetByCaller Data.StaminaDelta, like the sprint's GE_RpgGasp_StaminaDelta. */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationShotStaminaEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationShotStaminaEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * Locally predicted shot ability without equipment, as GA_BowShot is without its bow: it draws with
 * URpgAbilityTask_ProjectileShot and the test's arrows from the player's own inventory, and ends with the shot.
 */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationShotAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationShotAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static constexpr float ReleaseCost = 10.0f;

	/** The shot this ability draws: the test arrow, a quick draw, a release cost and a 2 s server hold limit. */
	static FRpgProjectileShotDefinition MakeShotDefinition();

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilityTask_ProjectileShot> ShotTask;

	int32 ReleasedCount = 0;

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
