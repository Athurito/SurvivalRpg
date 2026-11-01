#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"

#include "RpgFocusSet.generated.h"

/**
 * Attribute set for focus, the harvesting resource that powers harvesting spells.
 *
 * GF_Harvesting_Magic grants it to the player state's ability system. Landed tool swings fill Focus on the server
 * through an instant GameplayEffect; a spell's cost spends it. Values are server-authoritative runtime state, replicate
 * only to the owning player, and are never saved: focus survives tool switches and death, and starts empty in every
 * session. The HUD shows the meter only while the ability system owns this set and only reads it.
 */
UCLASS()
class SURVIVALRPG_API URpgFocusSet : public URpgAttributeSet
{
	GENERATED_BODY()

public:
	URpgFocusSet();

	/** Current focus, clamped to [0, MaxFocus]; starts at 0. Server-authored and replicated to the owning player. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Focus", ReplicatedUsing = OnRep_Focus)
	FGameplayAttributeData Focus;
	ATTRIBUTE_ACCESSORS_BASIC(URpgFocusSet, Focus);

	/**
	 * Focus of a full meter, at least 1; 100 by default. Lowering it below the current focus lowers the focus as well.
	 * Server-authored and replicated to the owning player.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Focus", ReplicatedUsing = OnRep_MaxFocus)
	FGameplayAttributeData MaxFocus;
	ATTRIBUTE_ACCESSORS_BASIC(URpgFocusSet, MaxFocus);

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Focus(const FGameplayAttributeData& OldValue) const;

	UFUNCTION()
	void OnRep_MaxFocus(const FGameplayAttributeData& OldValue) const;

private:
	void ClampFocusAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;
};
