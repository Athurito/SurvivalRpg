#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "Blueprint/RpgAnimationAssetTools.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRetimeMontageRangeTest,
	"SurvivalRpg.Animation.RetimeMontageRange.StretchesTheRangeAndMovesLaterNotifies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRetimeMontageRangeTest::RunTest(const FString& Parameters)
{
	// A combo step with attack-window, combo-window and cue notifies; the copy lives in an unsaved project package.
	const UAnimMontage* Source = LoadObject<UAnimMontage>(nullptr,
		TEXT("/GF_Combat_Core/Animations/Combo/SwordShield/AM_SwordShieldCombo01_Step1.AM_SwordShieldCombo01_Step1"));
	if (!TestNotNull(TEXT("The source montage loads"), Source))
	{
		return false;
	}
	UPackage* Package = CreatePackage(TEXT("/Game/RpgAutomation/AM_RetimeMontageRangeTest"));
	Package->SetFlags(RF_Transient);
	UAnimMontage* Montage = DuplicateObject<UAnimMontage>(Source, Package, TEXT("AM_RetimeMontageRangeTest"));
	ON_SCOPE_EXIT
	{
		Montage->MarkAsGarbage();
		Package->MarkAsGarbage();
	};

	TArray<float> OldStarts;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		OldStarts.Add(Notify.GetTime());
	}
	const float OldLength = Montage->GetPlayLength();
	constexpr float RangeStart = 0.02f;
	constexpr float RangeEnd = 0.12f;
	constexpr float PlayRate = 0.25f;
	const float AddedTime = (RangeEnd - RangeStart) / PlayRate - (RangeEnd - RangeStart);

	const float NewLength = URpgAnimationAssetTools::RetimeMontageRange(Montage, RangeStart, RangeEnd, PlayRate);
	TestEqual(TEXT("The montage grows by the stretched time"), NewLength, OldLength + AddedTime, 1.0e-3f);
	TestEqual(TEXT("The slot now has three segments"), Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num(), 3);
	for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
	{
		const float OldTime = OldStarts[Index];
		const float Expected = OldTime <= RangeStart ? OldTime
			: OldTime >= RangeEnd ? OldTime + AddedTime
			: RangeStart + (OldTime - RangeStart) / PlayRate;
		TestEqual(*FString::Printf(TEXT("Notify %d moves with the range"), Index), Montage->Notifies[Index].GetTime(), Expected, 1.0e-3f);
	}

	TestTrue(TEXT("A second retime refuses the split montage"),
		URpgAnimationAssetTools::RetimeMontageRange(Montage, RangeStart, RangeEnd, PlayRate) < 0.0f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTrimMontageToRangeTest,
	"SurvivalRpg.Animation.TrimMontageToRange.KeepsTheRangeAndItsNotifies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTrimMontageToRangeTest::RunTest(const FString& Parameters)
{
	// The same combo step; its notifies give the range bounds that keep or refuse them.
	const UAnimMontage* Source = LoadObject<UAnimMontage>(nullptr,
		TEXT("/GF_Combat_Core/Animations/Combo/SwordShield/AM_SwordShieldCombo01_Step1.AM_SwordShieldCombo01_Step1"));
	if (!TestNotNull(TEXT("The source montage loads"), Source) || !TestTrue(TEXT("The source has notifies"), !Source->Notifies.IsEmpty()))
	{
		return false;
	}
	UPackage* Package = CreatePackage(TEXT("/Game/RpgAutomation/AM_TrimMontageToRangeTest"));
	Package->SetFlags(RF_Transient);
	UAnimMontage* Montage = DuplicateObject<UAnimMontage>(Source, Package, TEXT("AM_TrimMontageToRangeTest"));
	ON_SCOPE_EXIT
	{
		Montage->MarkAsGarbage();
		Package->MarkAsGarbage();
	};

	float FirstNotify = TNumericLimits<float>::Max();
	float LastNotifyEnd = 0.0f;
	TArray<float> OldStarts;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		OldStarts.Add(Notify.GetTime());
		FirstNotify = FMath::Min(FirstNotify, Notify.GetTime());
		LastNotifyEnd = FMath::Max(LastNotifyEnd, Notify.GetTime() + Notify.GetDuration());
	}
	const float OldLength = Montage->GetPlayLength();
	TestTrue(TEXT("A range that cuts off a notify is refused"),
		URpgAnimationAssetTools::TrimMontageToRange(Montage, FirstNotify + 0.01f, OldLength) < 0.0f);
	TestEqual(TEXT("A refused trim leaves the length"), Montage->GetPlayLength(), OldLength, 1.0e-4f);

	const float RangeStart = FirstNotify * 0.5f;
	const float RangeEnd = FMath::Max(LastNotifyEnd, (LastNotifyEnd + OldLength) * 0.5f);
	const float NewLength = URpgAnimationAssetTools::TrimMontageToRange(Montage, RangeStart, RangeEnd);
	TestEqual(TEXT("The montage plays only the range"), NewLength, RangeEnd - RangeStart, 1.0e-3f);
	const FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	TestEqual(TEXT("The segment starts at the range start"), Segment.AnimStartTime, RangeStart, 1.0e-4f);
	TestEqual(TEXT("The segment ends at the range end"), Segment.AnimEndTime, RangeEnd, 1.0e-4f);
	TestTrue(TEXT("The section still starts at zero"),
		Montage->CompositeSections.Num() <= 1 &&
			(Montage->CompositeSections.IsEmpty() || FMath::IsNearlyZero(Montage->CompositeSections[0].GetTime())));
	for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Notify %d keeps its animation frame"), Index),
			Montage->Notifies[Index].GetTime(), OldStarts[Index] - RangeStart, 1.0e-3f);
	}

	// A trimmed montage can be cut again, in its own montage time: here its tail after the last notify.
	TestTrue(TEXT("A second trim beyond the montage's end is refused"),
		URpgAnimationAssetTools::TrimMontageToRange(Montage, 0.0f, NewLength + 0.1f) < 0.0f);
	const float SecondEnd = FMath::Max(LastNotifyEnd - RangeStart, NewLength * 0.9f);
	const float ShorterLength = URpgAnimationAssetTools::TrimMontageToRange(Montage, 0.0f, SecondEnd);
	TestEqual(TEXT("The second trim plays the shorter range"), ShorterLength, SecondEnd, 1.0e-3f);
	TestEqual(TEXT("The segment keeps its animation start"), Segment.AnimStartTime, RangeStart, 1.0e-4f);
	TestEqual(TEXT("The segment ends at the second range's end in animation time"), Segment.AnimEndTime, RangeStart + SecondEnd, 1.0e-4f);
	for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Notify %d keeps its animation frame after the second trim"), Index),
			Montage->Notifies[Index].GetTime(), OldStarts[Index] - RangeStart, 1.0e-3f);
	}
	return !HasAnyErrors();
}

#endif
