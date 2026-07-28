#pragma once

#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"

#include "RpgGatheringSet.generated.h"

/**
 * Player-owned gathering modifiers consumed by authoritative harvest reward rolls.
 *
 * Both attributes are additive bonuses: zero is the neutral value, while 0.25 means
 * twenty-five percent more yield or rare-find chance after the trade-skill multiplier.
 * GameplayEffects from equipment, runes, or temporary buffs may modify these values.
 */
UCLASS()
class SURVIVALRPG_API URpgGatheringSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgGatheringSet();

	/** Additive harvest quantity bonus; server-authored and replicated to the owning player. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_YieldBonus, Category = "Rpg|Gathering")
	FGameplayAttributeData YieldBonus;
	ATTRIBUTE_ACCESSORS_BASIC(URpgGatheringSet, YieldBonus);

	/** Additive rare-drop chance bonus; server-authored and replicated to the owning player. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RareFindBonus, Category = "Rpg|Gathering")
	FGameplayAttributeData RareFindBonus;
	ATTRIBUTE_ACCESSORS_BASIC(URpgGatheringSet, RareFindBonus);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_YieldBonus(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_RareFindBonus(const FGameplayAttributeData& OldValue) const;
};
