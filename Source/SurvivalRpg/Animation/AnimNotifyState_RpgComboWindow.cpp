#include "AnimNotifyState_RpgComboWindow.h"

#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_RpgComboWindow)

FString UAnimNotifyState_RpgComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combo Window");
}

bool UAnimNotifyState_RpgComboWindow::FindComboWindow(
	const UAnimMontage* Montage,
	float& OutStartTime,
	float& OutEndTime,
	FString& OutFailureReason)
{
	OutStartTime = 0.0f;
	OutEndTime = 0.0f;
	OutFailureReason.Reset();
	if (!Montage)
	{
		OutFailureReason = TEXT("No montage is configured.");
		return false;
	}

	const FAnimNotifyEvent* Window = nullptr;
	int32 WindowCount = 0;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA<UAnimNotifyState_RpgComboWindow>())
		{
			++WindowCount;
			Window = &NotifyEvent;
		}
	}
	if (WindowCount != 1 || !Window)
	{
		OutFailureReason = FString::Printf(TEXT("Expected exactly one Combo Window notify state, found %d."), WindowCount);
		return false;
	}

	OutStartTime = Window->GetTriggerTime();
	OutEndTime = Window->GetEndTriggerTime();
	if (!FMath::IsFinite(OutStartTime) || !FMath::IsFinite(OutEndTime) || OutStartTime < 0.0f
		|| OutEndTime <= OutStartTime || OutEndTime > Montage->GetPlayLength())
	{
		OutFailureReason = FString::Printf(TEXT("Combo Window %.3f..%.3f lies outside the montage (length %.3f)."),
			OutStartTime, OutEndTime, Montage->GetPlayLength());
		return false;
	}
	return true;
}
