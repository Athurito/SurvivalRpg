#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Items/RpgItemGrantTypes.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Weapon.generated.h"

class URpgAbilitySet;

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemFragment_Weapon : public URpgItemFragment
{
	GENERATED_BODY()

public:
	FGameplayTagContainer BuildCompatibilityTagContainer() const;

	const FGameplayTag& GetWeaponTypeTag() const { return WeaponTypeTag; }
	const FGameplayTag& GetWeaponFamilyTag() const { return WeaponFamilyTag; }
	const FGameplayTag& GetHandUsageTag() const { return HandUsageTag; }
	const FGameplayTagContainer& GetEquipmentTraitTags() const { return EquipmentTraitTags; }
	const FGameplayTagContainer& GetCompatibilityTags() const { return CompatibilityTags; }
	const TArray<TObjectPtr<const URpgAbilitySet>>& GetActiveAbilitySets() const { return ActiveAbilitySets; }
	const TArray<FRpgItemGameplayEffectGrant>& GetActiveGameplayEffects() const { return ActiveGameplayEffects; }
	const FGameplayTagContainer& GetActiveLooseTags() const { return ActiveLooseTags; }
	void SetWeaponTypeTag(FGameplayTag InWeaponTypeTag) { WeaponTypeTag = InWeaponTypeTag; }
	void SetWeaponFamilyTag(FGameplayTag InWeaponFamilyTag) { WeaponFamilyTag = InWeaponFamilyTag; }
	void SetHandUsageTag(FGameplayTag InHandUsageTag) { HandUsageTag = InHandUsageTag; }
	void SetEquipmentTraitTags(const FGameplayTagContainer& InEquipmentTraitTags) { EquipmentTraitTags = InEquipmentTraitTags; }
	void SetCompatibilityTags(const FGameplayTagContainer& InCompatibilityTags) { CompatibilityTags = InCompatibilityTags; }
	void AddActiveAbilitySet(const URpgAbilitySet* AbilitySet) { ActiveAbilitySets.Add(AbilitySet); }
	void SetActiveLooseTags(const FGameplayTagContainer& InActiveLooseTags) { ActiveLooseTags = InActiveLooseTags; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Type"))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Family"))
	FGameplayTag WeaponFamilyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Equipment.HandUsage"))
	FGameplayTag HandUsageTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer EquipmentTraitTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer CompatibilityTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<const URpgAbilitySet>> ActiveAbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgItemGameplayEffectGrant> ActiveGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ActiveLooseTags;
};
