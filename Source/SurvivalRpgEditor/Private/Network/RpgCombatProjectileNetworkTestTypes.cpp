#include "Network/RpgCombatProjectileNetworkTestTypes.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/AbilitySystem/Executions/RpgDamageExecution.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_ProjectileShot.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

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

URpgNetworkAutomationArrowDefinition::URpgNetworkAutomationArrowDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Network Automation Arrow"));

	URpgInventoryFragment_SpatialItem* Spatial = CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
	Spatial->Footprint.Width = 1;
	Spatial->Footprint.Height = 1;
	Fragments.Add(Spatial);

	URpgInventoryFragment_ItemTraits* Traits = CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
	Traits->ItemCategory = ERpgInventoryItemCategory::Misc;
	Traits->bCanStack = true;
	Traits->MaxStackSize = 20;
	Fragments.Add(Traits);

	URpgInventoryFragment_Ammunition* Ammunition = CreateDefaultSubobject<URpgInventoryFragment_Ammunition>(TEXT("Ammunition"));
	Ammunition->ProjectileClass = ARpgNetworkAutomationTestProjectile::StaticClass();
	Fragments.Add(Ammunition);
}

URpgNetworkAutomationShotStaminaEffect::URpgNetworkAutomationShotStaminaEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = RpgGameplayTags::Data_StaminaDelta;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = URpgStaminaSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}

URpgNetworkAutomationShotAbility::URpgNetworkAutomationShotAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

FRpgProjectileShotDefinition URpgNetworkAutomationShotAbility::MakeShotDefinition()
{
	FRpgProjectileShotDefinition Definition;
	Definition.AmmunitionItemDefinition = URpgNetworkAutomationArrowDefinition::StaticClass();
	Definition.MinimumNockSeconds = 0.2f;
	Definition.FullDrawSeconds = 0.5f;
	const auto AddKeys = [](FRuntimeFloatCurve& Curve, const float AtZero, const float AtOne)
	{
		FRichCurve* RichCurve = Curve.GetRichCurve();
		RichCurve->SetKeyInterpMode(RichCurve->AddKey(0.0f, AtZero), RCIM_Linear);
		RichCurve->SetKeyInterpMode(RichCurve->AddKey(1.0f, AtOne), RCIM_Linear);
	};
	AddKeys(Definition.DamageByDrawFraction, 5.0f, 12.0f);
	AddKeys(Definition.LaunchSpeedByDrawFraction, 1000.0f, ARpgNetworkAutomationTestProjectile::FlightSpeed);
	Definition.ReleaseStaminaCost = ReleaseCost;
	Definition.StaminaChangeEffect = URpgNetworkAutomationShotStaminaEffect::StaticClass();
	return Definition;
}

void URpgNetworkAutomationShotAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ShotTask = URpgAbilityTask_ProjectileShot::StartProjectileShot(this, MakeShotDefinition());
	ShotTask->OnReleased.AddDynamic(this, &ThisClass::HandleReleased);
	ShotTask->OnCancelled.AddDynamic(this, &ThisClass::HandleCancelled);
	ShotTask->ReadyForActivation();
}

void URpgNetworkAutomationShotAbility::HandleReleased(float DrawFraction, ARpgCombatProjectile* Projectile)
{
	++ReleasedCount;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URpgNetworkAutomationShotAbility::HandleCancelled(ERpgProjectileShotResult Reason)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

