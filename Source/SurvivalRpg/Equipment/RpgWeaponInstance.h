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
	LineTrace,
	SphereSweep,
	CapsuleSweep,
	BoxSweep
};

USTRUCT(BlueprintType)
struct FRpgConditionalAttackModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FGameplayTagContainer RequiredTargetTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	FGameplayTagContainer BlockedTargetTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (ClampMin = "0.0"))
	float StaggerDamageMultiplier = 1.0f;

	bool MatchesTargetTags(const FGameplayTagContainer& TargetTags) const;
};

USTRUCT(BlueprintType)
struct FRpgWeaponAttackDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (Categories = "Damage.Type"))
	FGameplayTagContainer DamageTypeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float StaggerDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TArray<FRpgConditionalAttackModifier> ConditionalModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	ERpgWeaponAttackTraceMode TraceMode = ERpgWeaponAttackTraceMode::LineTrace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	bool bTraceBetweenSockets = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", EditCondition = "bTraceBetweenSockets"))
	float TraceInterpolationDistance = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceCapsuleHalfHeight = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FVector TraceBoxExtent = FVector(12.0f, 12.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	TArray<FName> TracePointSockets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float TraceSampleInterval = 0.016f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<URpgCameraMode> CameraMode;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block")
	bool bCanBlock = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (Categories = "Damage.Type"))
	FGameplayTagContainer BlockableDamageTypeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float BlockAngleDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float PerfectBlockWindow = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block")
	bool bAllowPerfectBlock = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float StaminaCost = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageReduction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Block", meta = (ClampMin = "0.0"))
	float BlockStaggerDamageMultiplier = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaminaRestore = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaggerDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perfect Block", meta = (ClampMin = "0.0"))
	float PerfectBlockStaggerDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockStartMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockLoopMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockEndMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> BlockHitMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> PerfectBlockMontage = nullptr;

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
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Family"))
	FGameplayTag WeaponFamilyTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FGameplayTagContainer EquipmentTraitTags;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attack", meta = (Categories = "Weapon.Attack"))
	TMap<FGameplayTag, FRpgWeaponAttackDefinition> AttackDefinitions;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Block")
	FRpgWeaponBlockDefinition BlockDefinition;
};
