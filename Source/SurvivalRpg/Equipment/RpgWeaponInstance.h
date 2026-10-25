#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "RpgEquipmentInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "RpgWeaponInstance.generated.h"

class UAnimMontage;
class UAnimInstance;
class USkeleton;

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
struct SURVIVALRPG_API FRpgWeaponAttackDefinition
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

	/**
	 * Attack on the same weapon that follows this one when attack input is pressed before or during this montage's
	 * Combo Window; a press before the window chains at its start. None ends the combo with this attack. The montage then
	 * needs exactly one Combo Window that starts after its attack window and ends before auto blend-out. Designer-authored
	 * static data; the owner predicts each step and the server accepts it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (Categories = "Weapon.Attack"))
	FGameplayTag ComboNextAttack;

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

	/**
	 * Designer-selected impact cue when this attack hits a body, for example GameplayCue.Combat.Hit.Heavy on a finisher.
	 * Blocks and perfect blocks use the shared GameplayCue.Combat.Block and GameplayCue.Combat.PerfectBlock instead.
	 * The server's damage execution plays it for everyone; the attacking client predicts it. Empty plays no impact cue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (Categories = "GameplayCue.Combat.Hit"))
	FGameplayTag HitCueTag;

	FRpgWeaponAttackDefinition();
	bool CanApplyDamage() const { return DamageEffect && (Damage > 0.0f || StaggerDamage > 0.0f); }
	bool HasValidTraceData() const;
};

/**
 * Block presentation for one gameplay-mesh skeleton. Linked layers only bind to their exact target skeleton, so a
 * weapon used by pawns on different skeletons lists one presentation per further skeleton. Designer-authored static
 * data; every machine resolves it from its local gameplay mesh, and gameplay tuning stays in the block definition.
 */
USTRUCT(BlueprintType)
struct FRpgWeaponBlockPresentation
{
	GENERATED_BODY()

	/** Gameplay-mesh skeleton this presentation serves, matched by exact identity like a linked anim layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<USkeleton> Skeleton = nullptr;

	/** Linked block layer for this skeleton; its target skeleton must be Skeleton. Null retains the legacy loop montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> BlockLocomotionLayer;

	/** Optional montage played when block starts on this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockStartMontage = nullptr;

	/** Optional held-block montage on this skeleton, used only when BlockLocomotionLayer is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockLoopMontage = nullptr;

	/** Optional montage played when block ends normally on this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockEndMontage = nullptr;

	/** Optional reaction montage played when a normal block absorbs a hit on this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockHitMontage = nullptr;

	/** Optional feedback montage played when a perfect block succeeds on this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> PerfectBlockMontage = nullptr;

	/** Optional montage used when this item's block is guard-broken on this skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> GuardBreakMontage = nullptr;
};

/** Designer-authored block gameplay and presentation. The ability snapshots gameplay tuning for each activation. */
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

	/** Optional full-body linked animation layer on the gameplay mesh. Equipment owns binding; the layer owns cosmetic selection. Null retains legacy loop-montage presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> BlockLocomotionLayer;

	/** Maximum block locomotion speed in cm/s, sampled once per predicted/server activation. Zero preserves the ordinary speed limit; never increases it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MovementSpeedLimit = 0.0f;

	// Optional montage played when block starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockStartMontage = nullptr;

	// Optional looping or held-block montage, used only when BlockLocomotionLayer is unset.
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

	/**
	 * Optional presentations for further gameplay skeletons. An entry whose Skeleton is the gameplay mesh's skeleton
	 * replaces the layer and every block montage above as a whole; without a match the fields above apply.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<FRpgWeaponBlockPresentation> SkeletonPresentations;

	/** Returns a copy with the presentation for Skeleton applied; gameplay tuning is unchanged. The first match wins. */
	SURVIVALRPG_API FRpgWeaponBlockDefinition ResolveForSkeleton(const USkeleton* Skeleton) const;
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

#if WITH_EDITOR
	/** Validates generic block tuning and the compiled linked-layer contract; pack selection coverage is designer-owned. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

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
	/** Configures only a live test equipment instance; never mutates a definition CDO or replicates test tuning. The layer goes to the presentation the pawn's gameplay skeleton resolves. */
	bool ConfigureBlockLocomotionForTests(TSubclassOf<UAnimInstance> LayerClass, float SpeedLimit);
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
