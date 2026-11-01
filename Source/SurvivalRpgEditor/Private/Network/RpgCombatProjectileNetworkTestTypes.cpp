#include "Network/RpgCombatProjectileNetworkTestTypes.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/AbilitySystem/Executions/RpgDamageExecution.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatProjectileNetworkTestTypes)

ARpgNetworkAutomationProjectileGameMode::ARpgNetworkAutomationProjectileGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = ARpgGameStateBase::StaticClass();
	PlayerStateClass = ARpgNetworkAutomationProjectileShooterState::StaticClass();
}

ARpgNetworkAutomationProjectileShooterState::ARpgNetworkAutomationProjectileShooterState()
{
	// Hostile to the target fixtures (faction 2), as PawnData would set it for a real player.
	TeamId = 1;
}

void ARpgNetworkAutomationProjectileShooterState::PostInitializeComponents()
{
	// Skip the Experience-bound PawnData grant of ARpgPlayerState, but keep the real ASC lifecycle.
	ARpgBasePlayerState::PostInitializeComponents();
}

URpgNetworkAutomationProjectileDamageEffect::URpgNetworkAutomationProjectileDamageEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = URpgDamageExecution::StaticClass();
	Executions.Add(Execution);
}

ARpgNetworkAutomationTestProjectile::ARpgNetworkAutomationTestProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DamageEffectClass = URpgNetworkAutomationProjectileDamageEffect::StaticClass();
	DefaultSpeed = FlightSpeed;
	GravityScale = 0.0f;
	CollisionRadius = 5.0f;
	MaxFlightSeconds = 5.0f;
	ImpactLingerSeconds = 2.0f;
	ImpactCueTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Spell.Fireball.Impact"), false);
	ImpactCueMagnitude = 300.0f;
}

ARpgNetworkAutomationFallingTestProjectile::ARpgNetworkAutomationFallingTestProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GravityScale = 1.0f;
}

URpgNetworkAutomationProjectileAbility::URpgNetworkAutomationProjectileAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URpgNetworkAutomationProjectileAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UWorld* World = GetWorld();
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (World && Avatar && ActorInfo->IsNetAuthority())
	{
		TActorIterator<ARpgCombatNetworkTargetFixture> Target(World);
		if (Target)
		{
			// Launch from just in front of the avatar, straight at the target, inside this predicted activation.
			const FVector ToTarget = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();
			FRpgProjectileLaunchParams Params;
			Params.Origin = Avatar->GetActorLocation() + ToTarget * 100.0f;
			Params.Direction = ToTarget;
			Params.Damage = ARpgNetworkAutomationTestProjectile::ShotDamage;
			Params.ShotId = ShotId;
			ARpgCombatProjectile::LaunchFromAbility(this, ARpgNetworkAutomationTestProjectile::StaticClass(), Params);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
