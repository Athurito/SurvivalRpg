#include "RpgProjectileShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgProjectileShot)

#define LOCTEXT_NAMESPACE "RpgProjectileShot"

bool FRpgProjectileShotTargetData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Release.DrawSeconds;
	Release.AimOrigin.NetSerialize(Ar, Map, bOutSuccess);
	Release.AimDirection.NetSerialize(Ar, Map, bOutSuccess);
	Ar << Release.ShotId;
	bOutSuccess = !Ar.IsError();
	return true;
}

namespace RpgProjectileShot
{
	/** How far in cm the launch point stays in front of an obstruction between the shooter's body and the muzzle. */
	constexpr float ObstructionPullBack = 2.0f;

	/** An aim point closer than this to the launch point keeps the camera's direction instead. */
	constexpr float MinAimPointDistance = 50.0f;

	bool IsStaminaChangeEffect(const UGameplayEffect* Effect)
	{
		if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Instant
			|| Effect->Modifiers.Num() != 1 || !Effect->Executions.IsEmpty())
		{
			return false;
		}
		const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
		return Modifier.Attribute == URpgStaminaSet::GetStaminaAttribute()
			&& Modifier.ModifierOp == EGameplayModOp::Additive
			&& Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
			&& Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag == RpgGameplayTags::Data_StaminaDelta;
	}

	bool HasKeys(const FRuntimeFloatCurve& Curve)
	{
		const FRichCurve* RichCurve = Curve.GetRichCurveConst();
		return RichCurve && RichCurve->GetNumKeys() > 0;
	}

	float EvaluateCurve(const FRuntimeFloatCurve& Curve, const float Fraction)
	{
		const FRichCurve* RichCurve = Curve.GetRichCurveConst();
		return RichCurve ? RichCurve->Eval(Fraction, 0.0f) : 0.0f;
	}

	bool ApplyStaminaDelta(UAbilitySystemComponent& AbilitySystem, TSubclassOf<UGameplayEffect> Effect, const float Level, const float Delta)
	{
		const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(Effect, Level, AbilitySystem.MakeEffectContext());
		if (!Spec.IsValid())
		{
			return false;
		}
		Spec.Data->SetSetByCallerMagnitude(RpgGameplayTags::Data_StaminaDelta, Delta);
		// Instant effects return a non-active sentinel handle; IsValid() would reject a success.
		return AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	}

	void ValidateDefinition(const FRpgProjectileShotDefinition& Definition, TArray<FText>& OutErrors)
	{
		if (!URpgInventoryFragment_Ammunition::FindProjectileClass(Definition.AmmunitionItemDefinition))
		{
			OutErrors.Add(LOCTEXT("MissingAmmunition",
				"AmmunitionItemDefinition must name an item with an Ammunition fragment that names a projectile class."));
		}
		if (!FMath::IsFinite(Definition.MinimumNockSeconds) || Definition.MinimumNockSeconds < 0.0f
			|| !FMath::IsFinite(Definition.FullDrawSeconds) || Definition.FullDrawSeconds <= 0.0f)
		{
			OutErrors.Add(LOCTEXT("InvalidDraw", "MinimumNockSeconds must be zero or more and FullDrawSeconds positive."));
		}
		if (!HasKeys(Definition.DamageByDrawFraction) || !HasKeys(Definition.LaunchSpeedByDrawFraction))
		{
			OutErrors.Add(LOCTEXT("MissingCurves", "DamageByDrawFraction and LaunchSpeedByDrawFraction need keys."));
		}
		else
		{
			for (const float Fraction : { 0.0f, 0.5f, 1.0f })
			{
				if (!(EvaluateCurve(Definition.LaunchSpeedByDrawFraction, Fraction) > 0.0f)
					|| !(EvaluateCurve(Definition.DamageByDrawFraction, Fraction) >= 0.0f))
				{
					OutErrors.Add(LOCTEXT("InvalidCurveValues", "Launch speed must stay positive and damage non-negative over the whole draw."));
					break;
				}
			}
		}
		const bool bHasCost = Definition.ReleaseStaminaCost > 0.0f || Definition.HoldStaminaPerSecond > 0.0f;
		if (!FMath::IsFinite(Definition.ReleaseStaminaCost) || Definition.ReleaseStaminaCost < 0.0f
			|| !FMath::IsFinite(Definition.HoldStaminaPerSecond) || Definition.HoldStaminaPerSecond < 0.0f)
		{
			OutErrors.Add(LOCTEXT("InvalidCost", "ReleaseStaminaCost and HoldStaminaPerSecond must be zero or more."));
		}
		if (bHasCost && !IsStaminaChangeEffect(Definition.StaminaChangeEffect.GetDefaultObject()))
		{
			OutErrors.Add(LOCTEXT("InvalidStaminaEffect",
				"A stamina cost needs a StaminaChangeEffect: Instant, one additive Stamina modifier with SetByCaller Data.StaminaDelta."));
		}
		if (Definition.MuzzleOffset.ContainsNaN() || !FMath::IsFinite(Definition.MaxAimDistance) || Definition.MaxAimDistance <= 0.0f)
		{
			OutErrors.Add(LOCTEXT("InvalidAim", "MuzzleOffset must be finite and MaxAimDistance positive."));
		}
	}

	FRpgProjectileShotTuning ResolveTuning(
		const FRpgProjectileShotDefinition& Definition,
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo)
	{
		const auto Tune = [&Spec, &ActorInfo](const FGameplayTag Tag, const float BaseValue)
		{
			const float Value = URpgGameplayAbility::GetTunedValueForSpec(Spec, ActorInfo, Tag, BaseValue);
			return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : FMath::Max(0.0f, BaseValue);
		};

		FRpgProjectileShotTuning Tuning;
		Tuning.MinimumNockSeconds = FMath::Max(0.0f, Definition.MinimumNockSeconds);
		Tuning.FullDrawSeconds = FMath::Max3(0.01f, Tuning.MinimumNockSeconds,
			Tune(RpgGameplayTags::Ability_Tuning_Ranged_DrawTime, Definition.FullDrawSeconds));
		Tuning.ReleaseStaminaCost = Tune(RpgGameplayTags::Ability_Tuning_Ranged_ReleaseCost, Definition.ReleaseStaminaCost);
		Tuning.HoldStaminaPerSecond = Tune(RpgGameplayTags::Ability_Tuning_Ranged_HoldDrain, Definition.HoldStaminaPerSecond);
		return Tuning;
	}

	float GetDrawFraction(const float DrawSeconds, const FRpgProjectileShotTuning& Tuning)
	{
		return FMath::IsFinite(DrawSeconds) ? FMath::Clamp(DrawSeconds / FMath::Max(0.01f, Tuning.FullDrawSeconds), 0.0f, 1.0f) : 0.0f;
	}

	URpgInventoryManagerComponent* FindShooterInventory(const UAbilitySystemComponent* AbilitySystem)
	{
		// Only a player state owns a combat inventory; an NPC pawn's inventory is its corpse loot store.
		const APlayerState* PlayerState = AbilitySystem ? Cast<APlayerState>(AbilitySystem->GetOwnerActor()) : nullptr;
		return PlayerState ? PlayerState->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
	}

	ERpgProjectileShotResult CheckResources(
		const FRpgProjectileShotDefinition& Definition,
		const ERpgProjectileAmmunitionPolicy AmmunitionPolicy,
		const FRpgProjectileShotTuning& Tuning,
		const UAbilitySystemComponent* AbilitySystem)
	{
		if (!AbilitySystem)
		{
			return ERpgProjectileShotResult::InvalidShot;
		}
		if (AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory)
		{
			const URpgInventoryManagerComponent* Inventory = FindShooterInventory(AbilitySystem);
			if (!Inventory || !Definition.AmmunitionItemDefinition
				|| Inventory->GetTotalItemCountByDefinition(Definition.AmmunitionItemDefinition) < 1)
			{
				return ERpgProjectileShotResult::NoAmmunition;
			}
		}
		if (Tuning.ReleaseStaminaCost > 0.0f)
		{
			bool bFound = false;
			const float Stamina = AbilitySystem->GetGameplayAttributeValue(URpgStaminaSet::GetStaminaAttribute(), bFound);
			if (!bFound || !FMath::IsFinite(Stamina) || Stamina < Tuning.ReleaseStaminaCost)
			{
				return ERpgProjectileShotResult::InsufficientStamina;
			}
		}
		return ERpgProjectileShotResult::Released;
	}

	bool ResolveGeometry(
		const FRpgProjectileShotDefinition& Definition,
		const APawn* Shooter,
		const FVector& AimOrigin,
		const FVector& AimDirection,
		FRpgProjectileShotGeometry& OutGeometry)
	{
		UWorld* World = Shooter ? Shooter->GetWorld() : nullptr;
		const FVector CameraDirection = AimDirection.GetSafeNormal();
		const FVector ShooterLocation = Shooter ? Shooter->GetActorLocation() : FVector::ZeroVector;
		if (!World || AimOrigin.ContainsNaN() || CameraDirection.IsNearlyZero()
			|| FVector::DistSquared(AimOrigin, ShooterLocation) > FMath::Square(MaxAimOriginDistance))
		{
			return false;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgProjectileShotAim), false, Shooter);
		TArray<AActor*> AttachedActors;
		Shooter->GetAttachedActors(AttachedActors, true, true);
		QueryParams.AddIgnoredActors(AttachedActors);

		// The camera only gives intent: the shot leaves from the weapon, and the body-to-muzzle segment must be clear.
		const FRotator AimYaw(0.0f, CameraDirection.Rotation().Yaw, 0.0f);
		const FVector Muzzle = ShooterLocation + AimYaw.RotateVector(Definition.MuzzleOffset);
		const FVector Body = ShooterLocation + FVector(0.0f, 0.0f, Definition.MuzzleOffset.Z);
		OutGeometry.Origin = Muzzle;
		OutGeometry.bObstructed = false;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Body, Muzzle, Rpg_TraceChannel_Projectile, QueryParams))
		{
			OutGeometry.bObstructed = true;
			OutGeometry.Origin = Hit.Location - (Muzzle - Body).GetSafeNormal() * ObstructionPullBack;
		}

		const FVector AimEnd = AimOrigin + CameraDirection * FMath::Max(100.0f, Definition.MaxAimDistance);
		const FVector AimPoint = World->LineTraceSingleByChannel(Hit, AimOrigin, AimEnd, Rpg_TraceChannel_Projectile, QueryParams)
			? Hit.ImpactPoint
			: AimEnd;
		const FVector ToAimPoint = AimPoint - OutGeometry.Origin;
		const FVector LaunchDirection = ToAimPoint.GetSafeNormal();
		// An aim point at or behind the launch point, such as a wall right at the weapon, keeps the camera's direction.
		OutGeometry.Direction = ToAimPoint.SizeSquared() < FMath::Square(MinAimPointDistance) || (LaunchDirection | CameraDirection) <= 0.0f
			? CameraDirection
			: LaunchDirection;
		return true;
	}

	ERpgProjectileShotResult CommitShot(
		UGameplayAbility* Ability,
		const FRpgProjectileShotDefinition& Definition,
		const ERpgProjectileAmmunitionPolicy AmmunitionPolicy,
		const FRpgProjectileShotTuning& Tuning,
		const float DrawFraction,
		const FRpgProjectileShotGeometry& Geometry,
		const int32 ShotId,
		ARpgCombatProjectile*& OutProjectile)
	{
		OutProjectile = nullptr;
		const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
		UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const TSubclassOf<ARpgCombatProjectile> ProjectileClass =
			URpgInventoryFragment_Ammunition::FindProjectileClass(Definition.AmmunitionItemDefinition);
		if (!AbilitySystem || !ActorInfo->IsNetAuthority() || !ProjectileClass || !FMath::IsFinite(DrawFraction))
		{
			return ERpgProjectileShotResult::InvalidShot;
		}

		const ERpgProjectileShotResult Resources = CheckResources(Definition, AmmunitionPolicy, Tuning, AbilitySystem);
		if (Resources != ERpgProjectileShotResult::Released)
		{
			return Resources;
		}
		const float Cost = Tuning.ReleaseStaminaCost;
		if (Cost > 0.0f && !IsStaminaChangeEffect(Definition.StaminaChangeEffect.GetDefaultObject()))
		{
			return ERpgProjectileShotResult::InvalidShot;
		}

		const float Fraction = FMath::Clamp(DrawFraction, 0.0f, 1.0f);
		FRpgProjectileLaunchParams Params;
		Params.Origin = Geometry.Origin;
		Params.Direction = Geometry.Direction;
		Params.Speed = EvaluateCurve(Definition.LaunchSpeedByDrawFraction, Fraction);
		Params.Damage = EvaluateCurve(Definition.DamageByDrawFraction, Fraction);
		Params.StaggerDamage = Definition.StaggerDamage * Fraction;
		Params.ShotId = ShotId;
		if (!FMath::IsFinite(Params.Speed) || Params.Speed <= 0.0f || !FMath::IsFinite(Params.Damage) || Params.Damage < 0.0f)
		{
			return ERpgProjectileShotResult::InvalidShot;
		}

		// Spawned but not finished: nothing begins play or replicates until every payment has gone through.
		ARpgCombatProjectile* Projectile = ARpgCombatProjectile::BeginLaunchFromAbility(Ability, ProjectileClass, Params);
		if (!Projectile)
		{
			return ERpgProjectileShotResult::SpawnFailed;
		}

		const float Level = Ability->GetAbilityLevel();
		if (Cost > 0.0f && !ApplyStaminaDelta(*AbilitySystem, Definition.StaminaChangeEffect, Level, -Cost))
		{
			Projectile->AbortDeferredLaunch();
			return ERpgProjectileShotResult::InsufficientStamina;
		}

		const auto RefundStamina = [&]()
		{
			// Same frame and effect, so adding the cost back restores the exact value it had.
			if (Cost > 0.0f)
			{
				ApplyStaminaDelta(*AbilitySystem, Definition.StaminaChangeEffect, Level, Cost);
			}
		};

		URpgInventoryManagerComponent* Inventory = AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory
			? FindShooterInventory(AbilitySystem)
			: nullptr;
		if (AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory
			&& (!Inventory || !Inventory->ConsumeItemsByDefinition(Definition.AmmunitionItemDefinition, 1)))
		{
			RefundStamina();
			Projectile->AbortDeferredLaunch();
			return ERpgProjectileShotResult::NoAmmunition;
		}

		if (!Projectile->FinishDeferredLaunch())
		{
			RefundStamina();
			if (Inventory)
			{
				Inventory->GrantItemDefinition(Definition.AmmunitionItemDefinition, 1);
			}
			return ERpgProjectileShotResult::SpawnFailed;
		}

		OutProjectile = Projectile;
		return ERpgProjectileShotResult::Released;
	}
}

#undef LOCTEXT_NAMESPACE
