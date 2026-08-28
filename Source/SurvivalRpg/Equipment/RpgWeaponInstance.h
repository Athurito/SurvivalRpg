#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "RpgEquipmentInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "RpgWeaponInstance.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class ERpgWeaponAttackTraceMode : uint8
{
	// Thin continuous line ribbon. Best default for blades because it avoids large forgiving volumes.
	LineTrace,

	// Sphere swept along each trace point. Useful for blunt weapons, claws, fists, or forgiving prototype hits.
	SphereSweep,

	// Capsule swept along each trace point. Useful for long, thick hit volumes such as monster limbs.
	CapsuleSweep,

	// Box swept along each trace point. Useful when the hit area should feel broad and flat.
	BoxSweep
};

USTRUCT(BlueprintType)
struct FRpgConditionalAttackModifier
{
	GENERATED_BODY()

	// Target must own all of these tags for this modifier to apply.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FGameplayTagContainer RequiredTargetTags;

	// If the target owns any of these tags, this modifier is skipped.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FGameplayTagContainer BlockedTargetTags;

	// Multiplies health damage after this modifier matches the target tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	// Multiplies posture/stagger damage after this modifier matches the target tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (ClampMin = "0.0"))
	float StaggerDamageMultiplier = 1.0f;

	bool MatchesTargetTags(const FGameplayTagContainer& TargetTags) const;
};

USTRUCT(BlueprintType)
struct FRpgWeaponAttackDefinition
{
	GENERATED_BODY()

	/**
	 * Designer-authored attack montage. It must contain exactly one section starting at zero with no section link or
	 * jump, plus one ordered
	 * project AttackWindowStart/AttackWindowEnd notify pair. The pair must finish before normal auto blend-out;
	 * authority derives its fail-safe trace schedule from those same authored times. Time-stretch curves are unsupported.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** Positive constant playback-rate multiplier used by both montage playback and the authority attack-window schedule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.001", UIMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// GameplayEffect applied by the attack. It should consume SetByCaller.Damage and SetByCaller.StaggerDamage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// Base health damage sent to the damage GameplayEffect as SetByCaller.Damage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	// Damage type tags appended to the outgoing spec, e.g. Damage.Type.Melee.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Damage.Type"))
	FGameplayTagContainer DamageTypeTags;

	// Base posture/stagger damage sent as SetByCaller.StaggerDamage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float StaggerDamage = 20.0f;

	// Optional target-tag based damage multipliers, such as bonus damage against State.Staggered.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TArray<FRpgConditionalAttackModifier> ConditionalModifiers;

	// Collision scan style used while the montage attack window is open.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	ERpgWeaponAttackTraceMode TraceMode = ERpgWeaponAttackTraceMode::LineTrace;

	// When enabled, the weapon builds a continuous ribbon between neighboring trace sockets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	bool bTraceBetweenSockets = true;

	// Maximum spacing between generated ribbon points. Lower values are denser but more expensive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", EditCondition = "bTraceBetweenSockets"))
	float TraceInterpolationDistance = 12.0f;

	// Radius used by SphereSweep and CapsuleSweep. Also kept for quick tuning/debug even when LineTrace is selected.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 12.0f;

	// Half height used by CapsuleSweep. The final capsule half height is never smaller than TraceRadius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceCapsuleHalfHeight = 24.0f;

	// Half extents used by BoxSweep.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FVector TraceBoxExtent = FVector(12.0f, 12.0f, 12.0f);

	// Ordered sockets or component names that define the weapon blade/impact path from base to tip.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	TArray<FName> TracePointSockets;

	// Server sample interval while the attack window is open. Lower values are more accurate but more expensive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float TraceSampleInterval = 0.016f;

	// Optional camera mode applied for the duration of this attack.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<URpgCameraMode> CameraMode;

	// Gameplay event sent to the hit target when health damage was actually applied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (Categories = "GameplayEvent"))
	FGameplayTag HitReactionEventTag;

	FRpgWeaponAttackDefinition();
	bool CanApplyDamage() const { return DamageEffect && (Damage > 0.0f || StaggerDamage > 0.0f); }
	bool HasValidTraceData() const;
};

USTRUCT(BlueprintType)
struct FRpgWeaponBlockDefinition
{
	GENERATED_BODY()

	// Enables this weapon or shield as a valid source for the block ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block")
	bool bCanBlock = true;

	// Damage type tags this block can stop. Basic sword/shield usually blocks Damage.Type.Melee only.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (Categories = "Damage.Type"))
	FGameplayTagContainer BlockableDamageTypeTags;

	// Front cone angle in degrees. Hits outside this cone bypass the block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float BlockAngleDegrees = 120.0f;

	// Time after block start where incoming melee hits count as perfect blocks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float PerfectBlockWindow = 0.25f;

	// Enables perfect block behavior for this item. Disable for heavy shields or simple weapons.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block")
	bool bAllowPerfectBlock = true;

	// Stamina cost paid when a normal block succeeds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float StaminaCost = 20.0f;

	// Fraction of health damage prevented by a valid normal block. 1.0 means full damage prevention.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction = 1.0f;

	// Fraction of incoming stagger/posture damage that still applies to the blocker on a normal block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float BlockStaggerDamageMultiplier = 0.3f;

	// Stamina restored to the defender when a perfect block succeeds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaminaRestore = 15.0f;

	// Base posture/stagger damage sent back to the attacker on a perfect block.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaggerDamage = 35.0f;

	// Multiplier applied to PerfectBlockStaggerDamage, useful for shields with stronger parries.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaggerDamageMultiplier = 1.0f;

	// Optional montage played when block starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockStartMontage = nullptr;

	// Optional looping or held-block montage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockLoopMontage = nullptr;

	// Optional montage played when block ends normally.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockEndMontage = nullptr;

	// Optional reaction montage played when a normal block absorbs a hit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockHitMontage = nullptr;

	// Optional feedback montage played when a perfect block succeeds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> PerfectBlockMontage = nullptr;

	// Optional montage used when this item's block is guard-broken.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> GuardBreakMontage = nullptr;
};

/**
 * Data-driven weapon instance used as the source object for weapon abilities.
 *
 * The instance owns weapon attack data and camera seams while equipment
 * definitions remain responsible for granting the actual abilities.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgWeaponInstance : public URpgEquipmentInstance
{
	GENERATED_BODY()

public:
	URpgWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FRpgWeaponAttackDefinition* FindAttackDefinition(FGameplayTag AttackDefinitionTag) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	bool GetAttackDefinitionByTag(FGameplayTag AttackDefinitionTag, FRpgWeaponAttackDefinition& OutAttackDefinition) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	TArray<FGameplayTag> GetAttackDefinitionTags() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	TArray<FName> GetAttackDefinitionTagNames() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	bool HasAttackDefinitionByTagName(FName AttackDefinitionTagName) const;

#if WITH_DEV_AUTOMATION_TESTS
	/** Overrides one transient weapon instance for a runtime timing test without mutating its asset CDO. */
	bool SetAttackMontagePlayRateForTests(
		FGameplayTag AttackDefinitionTag,
		float NewPlayRate,
		float& OutPreviousPlayRate);
#endif

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	FGameplayTag GetWeaponTypeTag() const { return WeaponTypeTag; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	FGameplayTag GetWeaponFamilyTag() const { return WeaponFamilyTag; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	const FGameplayTagContainer& GetEquipmentTraitTags() const { return EquipmentTraitTags; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Block")
	const FRpgWeaponBlockDefinition& GetBlockDefinition() const { return BlockDefinition; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Block")
	bool CanBlock() const { return BlockDefinition.bCanBlock; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponTagsByName(FName WeaponTypeTagName, FName WeaponFamilyTagName);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ClearAttackDefinitions();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ConfigureAttackByTagName(
		FName AttackDefinitionTagName,
		UAnimMontage* Montage,
		TSubclassOf<UGameplayEffect> DamageEffect,
		float Damage,
		float TraceRadius,
		const TArray<FName>& TracePointSockets,
		float TraceSampleInterval,
		TSubclassOf<URpgCameraMode> CameraMode,
		FName HitReactionEventTagName);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ConfigureMeleeAttackByTagName(
		FName AttackDefinitionTagName,
		UAnimMontage* Montage,
		TSubclassOf<UGameplayEffect> DamageEffect,
		float Damage,
		float StaggerDamage,
		float TraceRadius,
		const TArray<FName>& TracePointSockets,
		float TraceSampleInterval,
		TSubclassOf<URpgCameraMode> CameraMode,
		FName HitReactionEventTagName);

	UFUNCTION(BlueprintCallable, Category = "Weapon|Block")
	void ConfigureMeleeBlock(
		bool bCanBlock,
		bool bAllowPerfectBlock,
		float BlockAngleDegrees,
		float PerfectBlockWindow,
		float StaminaCost,
		float DamageReduction,
		float BlockStaggerDamageMultiplier,
		float PerfectBlockStaminaRestore,
		float PerfectBlockStaggerDamage,
		UAnimMontage* BlockLoopMontage);

protected:
	// Broad weapon type used by gameplay logic and future filtering, e.g. Weapon.Type.Melee.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponTypeTag;

	// Weapon family used by mastery/skill trees, e.g. Weapon.Family.Sword.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Family"))
	FGameplayTag WeaponFamilyTag;

	// Passive identity tags contributed by this equipment instance.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FGameplayTagContainer EquipmentTraitTags;

	// Attack data keyed by Weapon.Attack tags. Input routing chooses which entry to use.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attack", meta = (Categories = "Weapon.Attack"))
	TMap<FGameplayTag, FRpgWeaponAttackDefinition> AttackDefinitions;

	// Block/perfect-block tuning for this weapon or shield.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Block")
	FRpgWeaponBlockDefinition BlockDefinition;
};
