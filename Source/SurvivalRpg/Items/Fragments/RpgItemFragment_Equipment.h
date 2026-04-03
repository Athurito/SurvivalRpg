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
	const FGameplayTagContainer& GetEquipmentTags() const { return EquipmentTags; }
	const TArray<TObjectPtr<const URpgAbilitySet>>& GetEquippedAbilitySets() const { return EquippedAbilitySets; }
	const TArray<FRpgItemGameplayEffectGrant>& GetEquippedGameplayEffects() const { return EquippedGameplayEffects; }
	const FGameplayTagContainer& GetEquippedLooseTags() const { return EquippedLooseTags; }
	void SetSupportedSlotTags(const FGameplayTagContainer& InSupportedSlotTags) { SupportedSlotTags = InSupportedSlotTags; }
	void SetEquipmentTags(const FGameplayTagContainer& InEquipmentTags) { EquipmentTags = InEquipmentTags; }
	void AddEquippedAbilitySet(const URpgAbilitySet* AbilitySet) { EquippedAbilitySets.Add(AbilitySet); }
	void SetEquippedLooseTags(const FGameplayTagContainer& InEquippedLooseTags) { EquippedLooseTags = InEquippedLooseTags; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", Categories = "Equipment.Slot"))
	FGameplayTagContainer SupportedSlotTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer EquipmentTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<const URpgAbilitySet>> EquippedAbilitySets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgItemGameplayEffectGrant> EquippedGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer EquippedLooseTags;
};
