#pragma once

#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"

#include "RpgInventoryCapacitySet.generated.h"

/**
 * Attribute set for player inventory capacity.
 *
 * BackpackSlots is read by URpgInventoryManagerComponent when that inventory is configured to use
 * AbilitySystemAttribute capacity. GameplayEffects from backpacks, runes, skills, or buffs may
 * modify it while the inventory remains the server-authoritative item owner.
 */
UCLASS()
class SURVIVALRPG_API URpgInventoryCapacitySet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgInventoryCapacitySet();

	/** Number of backpack entries available to the player inventory. Runtime mutable through GAS. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Inventory", ReplicatedUsing = OnRep_BackpackSlots)
	FGameplayAttributeData BackpackSlots;
	ATTRIBUTE_ACCESSORS_BASIC(URpgInventoryCapacitySet, BackpackSlots);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_BackpackSlots(const FGameplayAttributeData& OldValue) const;
};
