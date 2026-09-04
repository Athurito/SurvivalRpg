#pragma once

#include "SurvivalRpg/Animation/RpgAnimInstance.h"

#include "RpgAnimationThreadingTestTypes.generated.h"

/** Editor-only graph fixture that records the deltas actually consumed by the animation root. */
UCLASS(NotBlueprintable, Transient)
class URpgAnimationThreadingTestInstance final : public URpgAnimInstance
{
	GENERATED_BODY()

public:
	/** Observations come from the graph node update, not the once-per-frame native update callback. */
	int32 GetGraphUpdateCount() const;
	float GetGraphElapsedSeconds() const;
	void ResetGraphObservations();

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};
