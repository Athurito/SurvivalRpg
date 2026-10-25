#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "AnimNotifyState_RpgComboWindow.generated.h"

class UAnimMontage;

/**
 * Marks the span of a combo attack montage in which the next attack of its chain may start. Designer-authored on the
 * montage; the owner and the server read the span from the asset and schedule it from montage time, so the notify itself
 * sends no event and needs no server animation tick. A press before the span is buffered and chains at its start.
 */
UCLASS(meta = (DisplayName = "RPG Combo Window"))
class SURVIVALRPG_API UAnimNotifyState_RpgComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * Finds the montage's single combo window in montage seconds. Fails with a reason when the montage has none, more than
	 * one, or a span outside the montage.
	 */
	static bool FindComboWindow(const UAnimMontage* Montage, float& OutStartTime, float& OutEndTime, FString& OutFailureReason);
};
