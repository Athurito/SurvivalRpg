#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgEquipmentRuleset.generated.h"

class URpgItemInstance;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentCompatibilityRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment|Weapon", ToolTip = "Primary tag expected on the main-hand or left-side item for this rule. Typical values are Weapon.Family.Sword, Weapon.Family.Dagger, or Equipment.Trait.Shield."))
	FGameplayTag LeftTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment|Weapon", ToolTip = "Primary tag expected on the off-hand or right-side item for this rule. Pair this with LeftTag to explicitly allow or block a combination."))
	FGameplayTag RightTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ToolTip = "If true, the rule matches both directions. For example Sword plus Shield also matches Shield plus Sword."))
	bool bBidirectional = true;
};

UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgEquipmentRuleset : public UDataAsset
{
	GENERATED_BODY()

public:
	bool IsWeaponSetSlot(const FGameplayTag& SlotTag) const;
	bool IsMainHandSlot(const FGameplayTag& SlotTag) const;
	bool IsOffHandSlot(const FGameplayTag& SlotTag) const;
	bool DoesItemFitSlot(const URpgItemInstance* ItemInstance, const FGameplayTag& SlotTag) const;
	bool AreItemsCompatible(const URpgItemInstance* MainHandItem, const URpgItemInstance* OffHandItem) const;
	bool IsTwoHanded(const URpgItemInstance* ItemInstance) const;
	bool IsOffHandOnly(const URpgItemInstance* ItemInstance) const;
	bool IsMainHandOnly(const URpgItemInstance* ItemInstance) const;
	int32 GetNumWeaponSets() const { return FMath::Max(NumWeaponSets, 1); }
	bool IsTwoHandedCarryLimitEnabled() const { return bLimitTwoHandedWeaponsToOne; }
	bool AllowsOffHandWithoutMainHand() const { return bAllowOffHandWithoutMainHand; }
	void SetNumWeaponSets(int32 InNumWeaponSets) { NumWeaponSets = FMath::Max(InNumWeaponSets, 1); }
	void SetLimitTwoHandedWeaponsToOne(bool bInLimitTwoHandedWeaponsToOne) { bLimitTwoHandedWeaponsToOne = bInLimitTwoHandedWeaponsToOne; }
	void SetAllowOffHandWithoutMainHand(bool bInAllowOffHandWithoutMainHand) { bAllowOffHandWithoutMainHand = bInAllowOffHandWithoutMainHand; }
	void AddAllowedPairing(FGameplayTag LeftTag, FGameplayTag RightTag, bool bBidirectional = true) { AllowedPairings.Add({LeftTag, RightTag, bBidirectional}); }
	void AddBlockedPairing(FGameplayTag LeftTag, FGameplayTag RightTag, bool bBidirectional = true) { BlockedPairings.Add({LeftTag, RightTag, bBidirectional}); }

private:
	bool MatchesCompatibilityRule(const TArray<FRpgEquipmentCompatibilityRule>& Rules, const FGameplayTagContainer& LeftTags, const FGameplayTagContainer& RightTags) const;
	FGameplayTagContainer BuildCompatibilityTags(const URpgItemInstance* ItemInstance) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "1", ToolTip = "How many weapon sets a character can carry at the same time. Phase 1 expects 2 sets by default."))
	int32 NumWeaponSets = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "If true, only one two-handed weapon may be carried across all weapon sets. Disable this later if a talent should unlock additional two-handed loadouts."))
	bool bLimitTwoHandedWeaponsToOne = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "If false, an off-hand item such as a shield requires a compatible main-hand item in the same weapon set."))
	bool bAllowOffHandWithoutMainHand = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "Explicitly allowed main-hand and off-hand tag pairings. Use this for combinations such as Dagger plus Dagger, Sword plus Shield, or Wand plus Shield."))
	TArray<FRpgEquipmentCompatibilityRule> AllowedPairings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ToolTip = "Explicitly blocked pairings that should fail even if a more general allow rule would match. Use this for exceptions and future design restrictions."))
	TArray<FRpgEquipmentCompatibilityRule> BlockedPairings;
};
