#include "Animation/RpgAnimationRetargetLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRetargetTrackPreservationTest,
	"SurvivalRpg.Animation.Retarget.PreservesCuratedData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRetargetTrackPreservationTest::RunTest(const FString& Parameters)
{
	UAnimSequence* Existing = LoadObject<UAnimSequence>(nullptr,
		TEXT("/RpgGaspLocomotion/Animations/Stand/Walk/M_Neutral_Walk_Loop_F.M_Neutral_Walk_Loop_F"));
	if (!TestNotNull(TEXT("Curated animation loads"), Existing))
	{
		return false;
	}
	UAnimSequence* Candidate = DuplicateObject<UAnimSequence>(Existing, GetTransientPackage());
	UAnimSequence* Destination = DuplicateObject<UAnimSequence>(Existing, GetTransientPackage());
	const IAnimationDataModel* TargetModel = Destination->GetDataModel();
	TArray<FTransform> OriginalRoots;
	TArray<FTransform> OriginalFeet;
	TargetModel->GetBoneTrackTransforms(TEXT("root"), OriginalRoots);
	TargetModel->GetBoneTrackTransforms(TEXT("foot_l"), OriginalFeet);
	Destination->bEnableRootMotion = false;
	Destination->RateScale = 0.75f;

	// Add independent metadata to the destination so preserving candidate metadata would fail.
	IAnimationDataController& TargetController = Destination->GetController();
	const FAnimationCurveIdentifier ProbeCurve(TEXT("RetargetMetadataProbe"), ERawCurveTrackTypes::RCT_Float);
	TestTrue(TEXT("Metadata fixture curve is created"), TargetController.AddCurve(ProbeCurve, 4, false));
	FRichCurveKey ProbeKey(0.0f, 1.0f);
	ProbeKey.InterpMode = RCIM_Cubic;
	ProbeKey.TangentMode = RCTM_User;
	ProbeKey.LeaveTangent = 0.25f;
	const TArray<FRichCurveKey> ProbeKeys = { ProbeKey, FRichCurveKey(0.25f, 2.0f) };
	TestTrue(TEXT("Metadata fixture retains rich curve keys"), TargetController.SetCurveKeys(ProbeCurve, ProbeKeys, false));
	UAnimNotify_PlaySound* ProbeNotify = NewObject<UAnimNotify_PlaySound>(Destination);
	ProbeNotify->VolumeMultiplier = 0.5f;
	Destination->Notifies.AddDefaulted_GetRef().Notify = ProbeNotify;
	const FAnimationCurveData OriginalCurves = TargetModel->GetCurveData();
	const int32 OriginalNotifyCount = Destination->Notifies.Num();
	FString MetadataBefore;
	TestTrue(TEXT("Native metadata export accepts curated model and protected preview properties"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataBefore));
	TestFalse(TEXT("Successful metadata export is nonempty"), MetadataBefore.IsEmpty());

	IAnimationDataController& SourceController = Candidate->GetController();
	for (FName Bone : { FName(TEXT("root")), FName(TEXT("foot_l")) })
	{
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<FVector> Scales;
		const TArray<FTransform>& Original = Bone == TEXT("root") ? OriginalRoots : OriginalFeet;
		for (const FTransform& Key : Original)
		{
			Positions.Add(Key.GetTranslation() + FVector(7.0, 0.0, 0.0));
			Rotations.Add(Key.GetRotation());
			Scales.Add(Key.GetScale3D());
		}
		TestTrue(TEXT("Fixture changes candidate tracks"), SourceController.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false));
	}

	FString Error;
	TestTrue(TEXT("Compatible retarget tracks apply"),
		URpgAnimationRetargetLibrary::CopyRetargetedBoneTracks(Candidate, Destination, Error));
	TestTrue(TEXT("Successful copy has no error"), Error.IsEmpty());
	FString MetadataAfter;
	TestTrue(TEXT("Metadata can be exported after bone editing"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataAfter));
	TestEqual(TEXT("All preserved metadata remains identical after bone copying"), MetadataAfter, MetadataBefore);
	TArray<FTransform> Roots;
	TArray<FTransform> Feet;
	TargetModel->GetBoneTrackTransforms(TEXT("root"), Roots);
	TargetModel->GetBoneTrackTransforms(TEXT("foot_l"), Feet);
	TestEqual(TEXT("Root sample count is preserved"), Roots.Num(), OriginalRoots.Num());
	for (int32 Index = 0; Index < Roots.Num(); ++Index)
	{
		TestTrue(TEXT("Destination root survives different candidate root"), Roots[Index].Equals(OriginalRoots[Index], 1.e-5));
		TestTrue(TEXT("Non-root authored keys are copied"),
			Feet[Index].GetTranslation().Equals(OriginalFeet[Index].GetTranslation() + FVector(7.0, 0.0, 0.0), 1.e-4));
	}
	TestFalse(TEXT("Destination root-motion setting survives"), Destination->bEnableRootMotion);
	TestEqual(TEXT("Destination playback setting survives"), Destination->RateScale, 0.75f);
	TestEqual(TEXT("Destination notify count survives"), Destination->Notifies.Num(), OriginalNotifyCount);
	const TArray<FFloatCurve>& Curves = TargetModel->GetCurveData().FloatCurves;
	TestEqual(TEXT("Destination curve domain survives"), Curves.Num(), OriginalCurves.FloatCurves.Num());
	for (int32 Index = 0; Index < Curves.Num(); ++Index)
	{
		TestTrue(TEXT("Destination rich curve data survives"),
			Curves[Index].FloatCurve == OriginalCurves.FloatCurves[Index].FloatCurve);
	}

	SourceController.SetNumberOfFrames(FFrameNumber(2), false);
	TestFalse(TEXT("Different sample grid is rejected"),
		URpgAnimationRetargetLibrary::CopyRetargetedBoneTracks(Candidate, Destination, Error));
	TArray<FTransform> AfterRejectedCopy;
	TargetModel->GetBoneTrackTransforms(TEXT("foot_l"), AfterRejectedCopy);
	TestEqual(TEXT("Rejected copy does not resize target tracks"), AfterRejectedCopy.Num(), Feet.Num());
	for (int32 Index = 0; Index < Feet.Num(); ++Index)
	{
		TestTrue(TEXT("Rejected copy leaves authored target keys untouched"), AfterRejectedCopy[Index].Equals(Feet[Index], 1.e-5));
	}
	TestFalse(TEXT("Null candidate is rejected"),
		URpgAnimationRetargetLibrary::CopyRetargetedBoneTracks(nullptr, Destination, Error));

	// The single-key Sequencer API converts tangent units differently. Mutate and restore a
	// complete, actually authored key list through the same API used to create the fixture.
	const TArray<FRichCurveKey> AuthoredProbeKeys = TargetModel->GetRichCurve(ProbeCurve).GetConstRefOfKeys();
	if (!TestFalse(TEXT("Authored tangent probe contains keys"), AuthoredProbeKeys.IsEmpty()))
	{
		return false;
	}
	TestTrue(TEXT("Fixture round-trips its authored curve keys"), TargetController.SetCurveKeys(ProbeCurve, AuthoredProbeKeys, false));
	FString MetadataBeforeProbe;
	TestTrue(TEXT("Round-tripped probe metadata exports"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataBeforeProbe));
	TestEqual(TEXT("Full curve-key round-trip preserves metadata"), MetadataBeforeProbe, MetadataBefore);
	TArray<FRichCurveKey> ChangedProbeKeys = AuthoredProbeKeys;
	ChangedProbeKeys[0].LeaveTangent += 0.5f;
	TestTrue(TEXT("Fixture changes only a curve tangent"), TargetController.SetCurveKeys(ProbeCurve, ChangedProbeKeys, false));
	TestTrue(TEXT("Changed curve metadata exports"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataAfter));
	TestNotEqual(TEXT("Signature detects tangent changes without key time/value changes"), MetadataAfter, MetadataBeforeProbe);
	TestTrue(TEXT("Fixture restores authored curve keys"), TargetController.SetCurveKeys(ProbeCurve, AuthoredProbeKeys, false));
	TestTrue(TEXT("Restored curve metadata exports"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataAfter));
	TestEqual(TEXT("Restoring authored keys restores the exact metadata signature"), MetadataAfter, MetadataBeforeProbe);
	ProbeNotify->VolumeMultiplier = 0.75f;
	TestTrue(TEXT("Changed notify metadata exports"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(Destination, MetadataAfter));
	TestNotEqual(TEXT("Signature detects notify subobject settings with unchanged event identity"), MetadataAfter, MetadataBefore);
	TestFalse(TEXT("Null sequence cannot produce a successful metadata signature"),
		URpgAnimationRetargetLibrary::ExportPreservedMetadata(nullptr, MetadataAfter));
	TestTrue(TEXT("Rejected export clears any stale signature"), MetadataAfter.IsEmpty());
	return !HasAnyErrors();
}

#endif
