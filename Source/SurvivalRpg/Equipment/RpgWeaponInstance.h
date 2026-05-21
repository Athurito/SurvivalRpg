#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "RpgEquipmentInstance.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "RpgWeaponInstance.generated.h"

class UAnimMontage;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.0"))
	float DamageTraceDelay = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.0"))
	float TraceDistance = 175.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FName TraceStartSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FName TraceEndSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<URpgCameraMode> CameraMode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (Categories = "GameplayEvent"))
	FGameplayTag HitReactionEventTag;

	bool CanApplyDamage() const { return DamageEffect && Damage > 0.0f; }
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
		float DamageTraceDelay,
		float TraceDistance,
		float TraceRadius,
		FName TraceStartSocket,
		FName TraceEndSocket,
		TSubclassOf<URpgCameraMode> CameraMode,
		FName HitReactionEventTagName);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (Categories = "Weapon.Family"))
	FGameplayTag WeaponFamilyTag;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FGameplayTagContainer EquipmentTraitTags;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Attack", meta = (Categories = "Weapon.Attack"))
	TMap<FGameplayTag, FRpgWeaponAttackDefinition> AttackDefinitions;
};
