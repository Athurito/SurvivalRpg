#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Curves/CurveFloat.h"
#include "Engine/NetSerialization.h"

#include "RpgProjectileShot.generated.h"

class APawn;
class ARpgCombatProjectile;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpec;

/** Where a projectile shot takes its ammunition from. Chosen by the shooting ability; never derived from a missing inventory. */
UENUM(BlueprintType)
enum class ERpgProjectileAmmunitionPolicy : uint8
{
	/**
	 * Each shot consumes one item of the shot definition's ammunition from the shooter's own inventory: the inventory of
	 * the PlayerState that owns its ability system, carried bags included. A shooter without one cannot shoot.
	 */
	ShooterInventory,
	/** Content policy for NPC archers: no item is consumed. Never a fallback for a shooter without an inventory. */
	Unlimited,
};

/** How a projectile shot ended. Only Released launched a projectile and paid for it. */
UENUM(BlueprintType)
enum class ERpgProjectileShotResult : uint8
{
	/** A projectile launched; one ammunition item and the release cost were paid. */
	Released,
	/** Released before the minimum nocking time: no projectile, nothing paid. */
	EarlyRelease,
	/** No ammunition item at the release, or none to draw with: nothing paid. */
	NoAmmunition,
	/** Not enough stamina for the release cost: nothing paid. */
	InsufficientStamina,
	/** The hold drain emptied the stamina; the drawn shot is lowered. Hold drain already paid stays paid. */
	Exhausted,
	/** Malformed shot definition, shooter or aim data: nothing paid. */
	InvalidShot,
	/** The projectile could not spawn: nothing paid, or everything refunded. */
	SpawnFailed,
	/** The server waited longer than MaxHoldSeconds for a remote shooter's release: nothing paid. */
	TimedOut,
	/** The activation ended before the release, for example through a dodge or the bow's removal: nothing paid. */
	Interrupted,
};

/**
 * Static data of one weapon's or ability's projectile shot: ammunition, draw timing, damage and launch speed over the
 * draw, release cost and launch point. Designer-tuned on the weapon (URpgRangedWeaponInstance) or passed by an ability.
 * The shot resolves the tunable values once per activation through GetTunedValue, so passives change the next shot.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgProjectileShotDefinition
{
	GENERATED_BODY()

	/** Ammunition item a shot consumes; its Ammunition fragment names the projectile. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Ammunition")
	TSubclassOf<URpgInventoryItemDefinition> AmmunitionItemDefinition;

	/** Seconds from the start of the draw until the shot is nocked. An earlier release cancels without cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Draw", meta = (ClampMin = "0.0", Units = "Seconds"))
	float MinimumNockSeconds = 0.25f;

	/** Seconds from the start of the draw to full draw; tuned by Ability.Tuning.Ranged.DrawTime. At least the nock time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Draw", meta = (ClampMin = "0.01", Units = "Seconds"))
	float FullDrawSeconds = 1.0f;

	/**
	 * SetByCaller.Damage of the projectile over the draw fraction from 0 to 1 (draw time over full draw time). The
	 * damage execution still adds the shooter's BaseDamage and applies armor, crit and block. Required.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Draw")
	FRuntimeFloatCurve DamageByDrawFraction;

	/** Launch speed in cm/s over the draw fraction from 0 to 1. Must stay positive. Required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Draw")
	FRuntimeFloatCurve LaunchSpeedByDrawFraction;

	/** SetByCaller.StaggerDamage at full draw in posture points; a partial draw scales it by the draw fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Draw", meta = (ClampMin = "0.0"))
	float StaggerDamage = 0.0f;

	/** Stamina paid once at the release; tuned by Ability.Tuning.Ranged.ReleaseCost. Zero for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Cost", meta = (ClampMin = "0.0"))
	float ReleaseStaminaCost = 0.0f;

	/** Stamina per second drained from nocking until the release; tuned by Ability.Tuning.Ranged.HoldDrain. Zero for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Cost", meta = (ClampMin = "0.0"))
	float HoldStaminaPerSecond = 0.0f;

	/**
	 * Instant stamina change the costs use: exactly one additive Stamina modifier whose magnitude is SetByCaller
	 * Data.StaminaDelta, such as the sprint's GE_RpgGasp_StaminaDelta. Required when a cost or drain is set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Cost")
	TSubclassOf<UGameplayEffect> StaminaChangeEffect;

	/**
	 * Launch point relative to the shooter's location in cm, in the frame of the aim's yaw (X forward, Y right, Z up). The
	 * server checks the segment from the shooter's body at the same height to it, so a wall in between stops the shot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Aim")
	FVector MuzzleOffset = FVector(60.0f, 0.0f, 50.0f);

	/** Length in cm of the aim trace from the camera; the projectile flies toward its first hit or its end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Aim", meta = (ClampMin = "100.0", Units = "Centimeters"))
	float MaxAimDistance = 10000.0f;

	/** Seconds the server waits for a remote shooter's release before it cancels without cost; zero waits indefinitely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot|Network", meta = (ClampMin = "0.0", Units = "Seconds"))
	float MaxHoldSeconds = 60.0f;
};

/** The tunable values of one activation, resolved once at its start. Read-only for presentation. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgProjectileShotTuning
{
	GENERATED_BODY()

	/** Seconds until the shot is nocked. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float MinimumNockSeconds = 0.0f;

	/** Seconds to full draw, at least MinimumNockSeconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float FullDrawSeconds = 1.0f;

	/** Stamina the release pays. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float ReleaseStaminaCost = 0.0f;

	/** Stamina per second the nocked shot drains. */
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float HoldStaminaPerSecond = 0.0f;
};

/** A shooter's release: how long it drew, where its camera aimed, and the shot's identity. */
USTRUCT()
struct SURVIVALRPG_API FRpgProjectileShotRelease
{
	GENERATED_BODY()

	/** Seconds the shooter drew; the server clamps it to the time it observed. */
	UPROPERTY()
	float DrawSeconds = 0.0f;

	/** Camera location in cm, the start of the aim trace. Must lie near the shooter. */
	UPROPERTY()
	FVector_NetQuantize10 AimOrigin = FVector::ZeroVector;

	/** Camera direction, normalized. */
	UPROPERTY()
	FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;

	/** Shooter-chosen identity the projectile replicates, for presentation only. */
	UPROPERTY()
	int32 ShotId = 0;
};

/** The owner's release, sent once to the server with the shot's activation. */
USTRUCT()
struct SURVIVALRPG_API FRpgProjectileShotTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FRpgProjectileShotRelease Release;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FRpgProjectileShotTargetData> : TStructOpsTypeTraitsBase2<FRpgProjectileShotTargetData>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

/** Where an authoritative shot launches and in which direction. */
struct FRpgProjectileShotGeometry
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	/** Whether something on the projectile channel lies between the shooter's body and the muzzle. */
	bool bObstructed = false;
};

/** The reusable projectile shot mechanism: validation, tuning, launch geometry and the exactly-once commit. */
namespace RpgProjectileShot
{
	/** Largest distance in cm between the shooter and its reported camera location. */
	inline constexpr float MaxAimOriginDistance = 1000.0f;

	/** Seconds the shooter's reported draw may exceed the draw the server observed, for network jitter. */
	inline constexpr float DrawTimeTolerance = 0.25f;

	/** Appends why Definition cannot shoot; empty when it can. */
	SURVIVALRPG_API void ValidateDefinition(const FRpgProjectileShotDefinition& Definition, TArray<FText>& OutErrors);

	/** Resolves Definition's tunable values for one activation through every tuning source of Spec. */
	SURVIVALRPG_API FRpgProjectileShotTuning ResolveTuning(
		const FRpgProjectileShotDefinition& Definition,
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo);

	/** The draw fraction from 0 to 1 of a shot drawn for DrawSeconds. */
	SURVIVALRPG_API float GetDrawFraction(float DrawSeconds, const FRpgProjectileShotTuning& Tuning);

	/** The inventory of the PlayerState that owns AbilitySystem, or null for any other owner such as an NPC pawn. */
	SURVIVALRPG_API URpgInventoryManagerComponent* FindShooterInventory(const UAbilitySystemComponent* AbilitySystem);

	/**
	 * Whether AbilitySystem can pay a release now: Released when it can, otherwise NoAmmunition or InsufficientStamina.
	 * Owners read their replicated state for presentation; only the server's answer counts.
	 */
	SURVIVALRPG_API ERpgProjectileShotResult CheckResources(
		const FRpgProjectileShotDefinition& Definition,
		ERpgProjectileAmmunitionPolicy AmmunitionPolicy,
		const FRpgProjectileShotTuning& Tuning,
		const UAbilitySystemComponent* AbilitySystem);

	/**
	 * Server: the launch point and direction for a release. The muzzle follows Definition.MuzzleOffset in the aim's yaw;
	 * a hit on the projectile channel between the shooter's body and the muzzle moves the launch point back in front of
	 * it, so a shot never starts on the far side of a wall. The projectile flies toward the camera trace's first hit.
	 * Returns false for malformed aim data or a camera location far from the shooter.
	 */
	SURVIVALRPG_API bool ResolveGeometry(
		const FRpgProjectileShotDefinition& Definition,
		const APawn* Shooter,
		const FVector& AimOrigin,
		const FVector& AimDirection,
		FRpgProjectileShotGeometry& OutGeometry);

	/**
	 * Server: the exactly-once release transaction of Ability's current activation. Spawns the projectile deferred, pays
	 * the release stamina, consumes one ammunition item under ShooterInventory and finishes the spawn. Any failure undoes
	 * what it already paid and destroys the unfinished projectile, so a failed release launches nothing and costs nothing.
	 */
	SURVIVALRPG_API ERpgProjectileShotResult CommitShot(
		UGameplayAbility* Ability,
		const FRpgProjectileShotDefinition& Definition,
		ERpgProjectileAmmunitionPolicy AmmunitionPolicy,
		const FRpgProjectileShotTuning& Tuning,
		float DrawFraction,
		const FRpgProjectileShotGeometry& Geometry,
		int32 ShotId,
		ARpgCombatProjectile*& OutProjectile);
}
