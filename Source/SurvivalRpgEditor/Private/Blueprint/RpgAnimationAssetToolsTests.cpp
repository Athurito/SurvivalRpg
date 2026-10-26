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

#endif
