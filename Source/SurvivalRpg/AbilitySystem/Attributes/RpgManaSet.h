#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgManaSet.generated.h"

/**
 * Attribute set for mana, the resource of future spells.
 *
 * No pawn grants it yet. An AbilitySet that lists it under granted attribute sets gives a character mana; the HUD
 * shows its mana bar only while the character's ability system owns this set. Values are server-authoritative and
 * replicate to every client.
 */
UCLASS()
class SURVIVALRPG_API URpgManaSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgManaSet();

	/** Current mana, clamped to [0, MaxMana]. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Mana", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(URpgManaSet, Mana);

	/** Maximum mana, at least 1. Lowering it below the current mana lowers the mana as well. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Mana", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(URpgManaSet, MaxMana);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue) const;

private:
	void ClampManaAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
