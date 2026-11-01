#pragma once

#include "GameFramework/GameModeBase.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include "RpgCombatProjectileNetworkTestTypes.generated.h"

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

/** A straight, slow projectile with the fireball impact cue, so a test can watch it fly on every machine. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationTestProjectile final : public ARpgCombatProjectile
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationTestProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static constexpr float FlightSpeed = 1500.0f;
	static constexpr float ShotDamage = 12.0f;
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
