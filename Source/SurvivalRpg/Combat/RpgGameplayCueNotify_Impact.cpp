#include "RpgGameplayCueNotify_Impact.h"

#include "RpgCombatImpact.h"

bool URpgGameplayCueNotify_Impact::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	// The prediction itself plays; the server's later copy of a predicted impact is absorbed on the predicting client.
	if (!RpgCombatImpact::IsPlayingPredictedImpact() &&
		RpgCombatImpact::ConsumePredictedImpact(Target, Parameters.GetInstigator()) &&
		bPredictedByAttacker)
	{
		RpgCombatImpact::NoteAbsorbedImpact(Target);
		return false;
	}

	return Super::OnExecute_Implementation(Target, Parameters);
}
