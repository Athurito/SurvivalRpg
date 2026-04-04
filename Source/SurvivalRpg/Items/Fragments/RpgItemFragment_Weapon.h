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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Type", ToolTip = "Fine-grained weapon identifier for combat logic and content authoring. Use tags such as Weapon.Type.Axe, Weapon.Type.Staff, or other specific subtypes."))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Weapon.Family", ToolTip = "Broad family tag used by compatibility rules. Typical values are Weapon.Family.Sword, Weapon.Family.Dagger, Weapon.Family.Wand, Weapon.Family.Shield, or Weapon.Family.Bow."))
	FGameplayTag WeaponFamilyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", Categories = "Equipment.HandUsage", ToolTip = "Defines how the item consumes hand slots. MainHand for one-hand weapons, OffHand for shields, EitherHand for dual-wield candidates, and TwoHanded for bows or two-handed weapons."))
	FGameplayTag HandUsageTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", ToolTip = "Extra weapon traits used by rules and future systems. Example tags include Equipment.Trait.Shield or Equipment.Trait.DualWield."))
	FGameplayTagContainer EquipmentTraitTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", ToolTip = "Additional tags included during pairing checks. Use this when family and hand-usage tags are not enough to express a compatibility exception."))
	FGameplayTagContainer CompatibilityTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", ToolTip = "Ability sets granted only while the owning weapon set is active. This is the usual place for attack or weapon-specific combat abilities."))
	TArray<TObjectPtr<const URpgAbilitySet>> ActiveAbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", ToolTip = "GameplayEffects applied only while this weapon belongs to the active weapon set. Use this for active-stance bonuses or combat-mode modifiers."))
	TArray<FRpgItemGameplayEffectGrant> ActiveGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true", ToolTip = "Loose gameplay tags applied only while this weapon set is active. Useful for active combat state queries such as current weapon style or stance."))
	FGameplayTagContainer ActiveLooseTags;
};
