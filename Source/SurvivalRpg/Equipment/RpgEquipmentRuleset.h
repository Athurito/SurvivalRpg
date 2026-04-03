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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment|Weapon"))
	FGameplayTag LeftTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment|Weapon"))
	FGameplayTag RightTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 NumWeaponSets = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bLimitTwoHandedWeaponsToOne = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	bool bAllowOffHandWithoutMainHand = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgEquipmentCompatibilityRule> AllowedPairings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgEquipmentCompatibilityRule> BlockedPairings;
};
