#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Items/RpgItemGrantTypes.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Equipment.generated.h"

class URpgAbilitySet;

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemFragment_Equipment : public URpgItemFragment
{
	GENERATED_BODY()

public:
	const FGameplayTagContainer& GetSupportedSlotTags() const { return SupportedSlotTags; }
	const TArray<TObjectPtr<const URpgAbilitySet>>& GetEquippedAbilitySets() const { return EquippedAbilitySets; }
	const TArray<FRpgItemGameplayEffectGrant>& GetEquippedGameplayEffects() const { return EquippedGameplayEffects; }
	const FGameplayTagContainer& GetEquippedLooseTags() const { return EquippedLooseTags; }
	void SetSupportedSlotTags(const FGameplayTagContainer& InSupportedSlotTags) { SupportedSlotTags = InSupportedSlotTags; }
	void AddEquippedAbilitySet(const URpgAbilitySet* AbilitySet) { EquippedAbilitySets.Add(AbilitySet); }
	void SetEquippedLooseTags(const FGameplayTagContainer& InEquippedLooseTags) { EquippedLooseTags = InEquippedLooseTags; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", Categories = "Equipment.Slot", ToolTip = "All slots this item is allowed to occupy. Add MainHand tags for weapons, OffHand tags for shields, or both when the item can be equipped in multiple hand slots."))
	FGameplayTagContainer SupportedSlotTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "Ability sets granted while this item is equipped, even if its weapon set is currently inactive. Use this for persistent passive equipment abilities."))
	TArray<TObjectPtr<const URpgAbilitySet>> EquippedAbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "GameplayEffects applied while the item stays equipped. Use this for passive bonuses such as armor, resistances, or stat modifiers."))
	TArray<FRpgItemGameplayEffectGrant> EquippedGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "Loose gameplay tags applied while the item is equipped. Useful for state queries such as Equipment.State.HasShield or passive trait flags."))
	FGameplayTagContainer EquippedLooseTags;
};
