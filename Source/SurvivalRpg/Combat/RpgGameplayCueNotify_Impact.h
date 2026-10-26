#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "RpgGameplayCueNotify_Impact.generated.h"

/**
 * Burst cue for a melee impact that the attacking client may have predicted (CMB-04 hit feedback).
 *
 * The attacking client plays its prediction through RpgCombatImpact::PlayPredictedImpact, which records it. When the
 * server's copy of the same impact reaches that client, a recent matching prediction absorbs it, so the attacker sees
 * each impact once. Without one, because the attacker's own feedback trace missed, the server's copy still plays, late
 * instead of never. Every other machine plays the server's copy. Presentation only; nothing here affects gameplay.
 */
UCLASS(Abstract, Blueprintable, Category = "GameplayCueNotify", Meta = (DisplayName = "Rpg Impact Burst"))
class SURVIVALRPG_API URpgGameplayCueNotify_Impact : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	/**
	 * Designer flag: whether the attacking client predicts this cue, as it does hits and blocks. A cue it does not predict,
	 * such as a perfect block, always plays, and only clears the prediction it replaces.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	bool bPredictedByAttacker = true;
};
