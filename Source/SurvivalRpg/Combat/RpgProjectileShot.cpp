#include "RpgProjectileShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Combat/RpgCombatProjectile.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/SurvivalRpg.h"

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
		// Costs need a stamina effect to be paid with; a shot without one stays free, also when a passive adds a cost.
		if (IsStaminaChangeEffect(Definition.StaminaChangeEffect.GetDefaultObject()))
		{
			Tuning.ReleaseStaminaCost = Tune(RpgGameplayTags::Ability_Tuning_Ranged_ReleaseCost, Definition.ReleaseStaminaCost);
			Tuning.HoldStaminaPerSecond = Tune(RpgGameplayTags::Ability_Tuning_Ranged_HoldDrain, Definition.HoldStaminaPerSecond);
		}
		const TSubclassOf<ARpgCombatProjectile> ProjectileClass =
			URpgInventoryFragment_Ammunition::FindProjectileClass(Definition.AmmunitionItemDefinition);
		const float BaseBreakChance = ProjectileClass
			? ProjectileClass->GetDefaultObject<ARpgCombatProjectile>()->GetRecoveryDefinition().BaseBreakChance
			: 0.0f;
		Tuning.BreakChance = FMath::Min(1.0f, Tune(RpgGameplayTags::Ability_Tuning_Ranged_BreakChance, BaseBreakChance));
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

	float GetLaunchSpeed(const FRpgProjectileShotDefinition& Definition, const float DrawFraction)
	{
		return EvaluateCurve(Definition.LaunchSpeedByDrawFraction, FMath::Clamp(DrawFraction, 0.0f, 1.0f));
	}

	FVector GetMuzzleLocation(const FRpgProjectileShotDefinition& Definition, const FVector& ShooterLocation, const float AimYawDegrees)
	{
		return ShooterLocation + FRotator(0.0f, AimYawDegrees, 0.0f).RotateVector(Definition.MuzzleOffset);
	}

	bool SolveLaunchDirection(const FVector& Origin, const FVector& Target, const float Speed, const float Gravity, FVector& OutDirection)
	{
		const FVector Offset = Target - Origin;
		OutDirection = Offset.GetSafeNormal();
		const double Horizontal = Offset.Size2D();
		if (OutDirection.IsNearlyZero() || !(Speed > 0.0f) || Horizontal < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		if (Gravity <= 0.0f)
		{
			return true;
		}

		// Low root of the ballistic equation: tan(a) = (v² - sqrt(v⁴ - g(g x² + 2 y v²))) / (g x).
		const double SpeedSquared = FMath::Square(static_cast<double>(Speed));
		const double Discriminant = FMath::Square(SpeedSquared) - Gravity * (Gravity * FMath::Square(Horizontal) + 2.0 * Offset.Z * SpeedSquared);
		if (Discriminant < 0.0)
		{
			return false;
		}
		const double Elevation = FMath::Atan((SpeedSquared - FMath::Sqrt(Discriminant)) / (Gravity * Horizontal));
		const FVector Flat = FVector(Offset.X, Offset.Y, 0.0).GetSafeNormal();
		OutDirection = Flat * FMath::Cos(Elevation) + FVector::UpVector * FMath::Sin(Elevation);
		return true;
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
		const FVector Muzzle = GetMuzzleLocation(Definition, ShooterLocation, CameraDirection.Rotation().Yaw);
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
		Params.BreakChance = Tuning.BreakChance;
		if (!FMath::IsFinite(Params.Speed) || Params.Speed <= 0.0f || !FMath::IsFinite(Params.Damage) || Params.Damage < 0.0f)
		{
			return ERpgProjectileShotResult::InvalidShot;
		}

		// Only an arrow the shooter paid for comes back. Unlimited fire returns ammunition only for the recovery tokens
		// of the shooter's life; a failed release gives its token back, and ammunition consumed on impact takes none.
		URpgAbilitySystemComponent* RpgAbilitySystem = Cast<URpgAbilitySystemComponent>(AbilitySystem);
		const bool bRecoveryToken = AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::Unlimited
			&& Definition.UnlimitedRecoveryBudget > 0 && RpgAbilitySystem
			&& ProjectileClass->GetDefaultObject<ARpgCombatProjectile>()->GetImpactRecovery() == ERpgProjectileImpactRecovery::Recoverable
			&& RpgAbilitySystem->TryReserveRangedRecoveryToken(Definition.UnlimitedRecoveryBudget);
		const auto ReturnToken = [bRecoveryToken, RpgAbilitySystem]()
		{
			if (bRecoveryToken)
			{
				RpgAbilitySystem->ReturnRangedRecoveryToken();
			}
		};
		Params.RecoveryItemDefinition = AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory || bRecoveryToken
			? Definition.AmmunitionItemDefinition
			: nullptr;

		// Spawned but not finished: nothing begins play or replicates until every payment has gone through.
		ARpgCombatProjectile* Projectile = ARpgCombatProjectile::BeginLaunchFromAbility(Ability, ProjectileClass, Params);
		if (!Projectile)
		{
			ReturnToken();
			return ERpgProjectileShotResult::SpawnFailed;
		}

		URpgInventoryManagerComponent* Inventory = AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory
			? FindShooterInventory(AbilitySystem)
			: nullptr;
		if (AmmunitionPolicy == ERpgProjectileAmmunitionPolicy::ShooterInventory
			&& (!Inventory || !Inventory->ConsumeItemsByDefinition(Definition.AmmunitionItemDefinition, 1)))
		{
			Projectile->AbortDeferredLaunch();
			ReturnToken();
			return ERpgProjectileShotResult::NoAmmunition;
		}

		const auto ReturnArrow = [&]()
		{
			// A new instance in the first free cell; the stack it left may sit elsewhere, such as in a bag.
			if (Inventory && !Inventory->GrantItemDefinition(Definition.AmmunitionItemDefinition, 1))
			{
				UE_LOG(LogRpgWeapons, Warning, TEXT("Projectile shot of %s could not return its %s to the inventory."),
					*GetNameSafe(Ability), *GetNameSafe(Definition.AmmunitionItemDefinition.Get()));
			}
		};

		const float Level = Ability->GetAbilityLevel();
		if (Cost > 0.0f && !ApplyStaminaDelta(*AbilitySystem, Definition.StaminaChangeEffect, Level, -Cost))
		{
			ReturnArrow();
			Projectile->AbortDeferredLaunch();
			ReturnToken();
			return ERpgProjectileShotResult::InsufficientStamina;
		}

		if (!Projectile->FinishDeferredLaunch())
		{
			// Same frame and effect, so adding the cost back restores the exact value it had.
			if (Cost > 0.0f)
			{
				ApplyStaminaDelta(*AbilitySystem, Definition.StaminaChangeEffect, Level, Cost);
			}
			ReturnArrow();
			ReturnToken();
			return ERpgProjectileShotResult::SpawnFailed;
		}

		OutProjectile = Projectile;
		return ERpgProjectileShotResult::Released;
	}
}

#undef LOCTEXT_NAMESPACE
