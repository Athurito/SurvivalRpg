// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Misc/AutomationTest.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "SurvivalRpg/Animation/RpgGaspPresentationProfile.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgTurnInPlaceTimingValidationTest,
	"SurvivalRpg.Animation.Gasp.TurnInPlaceTimingValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTurnInPlaceTimingValidationTest::RunTest(const FString& Parameters)
{
	URpgGaspPresentationProfile* Profile = NewObject<URpgGaspPresentationProfile>();
	UAnimSequence* Ground = NewObject<UAnimSequence>();
	UAnimSequence* Jump = NewObject<UAnimSequence>();
	UAnimSequence* Landing = NewObject<UAnimSequence>();
	const auto AddMembership = [Profile](UAnimSequence* Asset, ERpgGaspPresentationAssetCategory Category)
	{
		FRpgGaspPresentationAssetMembership& Membership = Profile->AssetMemberships.AddDefaulted_GetRef();
		Membership.Asset = Asset;
		Membership.Category = Category;
	};
	AddMembership(Ground, ERpgGaspPresentationAssetCategory::GroundMoving);
	AddMembership(Jump, ERpgGaspPresentationAssetCategory::JumpStart);
	AddMembership(Landing, ERpgGaspPresentationAssetCategory::Landing);

	UAnimSequence* SourceTurn = LoadObject<UAnimSequence>(nullptr,
		TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_L.M_Neutral_Stand_Turn_090_L"));
	if (!TestNotNull(TEXT("The timing fixture source animation loads"), SourceTurn) ||
		!TestNotNull(TEXT("The timing fixture source has a valid skeleton"), SourceTurn->GetSkeleton()))
	{
		return false;
	}
	// Generic transient copies retain a valid skeleton/data model without creating a skeletonless
	// Sequencer FK rig. Timing coverage still depends on this test's two database entries, not the
	// pilot's package names, authored markers, or eight angle buckets. Never mutate the source asset.
	UAnimSequence* Turns[2] = {};
	for (UAnimSequence*& Turn : Turns)
	{
		Turn = DuplicateObject<UAnimSequence>(SourceTurn, GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UAnimSequence::StaticClass(), TEXT("TurnTimingFixture")));
		if (!TestNotNull(TEXT("The transient timing fixture is created"), Turn))
		{
			return false;
		}
		Turn->ClearFlags(RF_Public | RF_Standalone);
		Turn->SetFlags(RF_Transient);
		Turn->bLoop = false;
	}
	const float FixtureLength = Turns[0]->GetPlayLength();
	if (!TestTrue(TEXT("The timing fixture contains all valid probe markers"),
		FMath::IsFinite(FixtureLength) && FixtureLength > 1.2f))
	{
		return false;
	}
	for (uint8 RoleValue = static_cast<uint8>(ERpgMotionMatchingDatabaseRole::None) + 1;
		RoleValue < static_cast<uint8>(ERpgMotionMatchingDatabaseRole::Count); ++RoleValue)
	{
		const ERpgMotionMatchingDatabaseRole Role = static_cast<ERpgMotionMatchingDatabaseRole>(RoleValue);
		UPoseSearchDatabase* Database = NewObject<UPoseSearchDatabase>();
		Database->Tags.Add(RpgGaspLocomotionConfig::GetDatabaseRoleTag(Role));
		FPoseSearchDatabaseAnimationAsset Entry;
		if (Role == ERpgMotionMatchingDatabaseRole::StandTurnInPlace)
		{
			for (UAnimSequence* Turn : Turns)
			{
				Entry.AnimAsset = Turn;
				Database->AddAnimationAsset(Entry);
			}
		}
		else
		{
			switch (Role)
			{
			case ERpgMotionMatchingDatabaseRole::Jump:
				Entry.AnimAsset = Jump;
				break;
			case ERpgMotionMatchingDatabaseRole::StandLightLanding:
			case ERpgMotionMatchingDatabaseRole::StandHeavyLanding:
			case ERpgMotionMatchingDatabaseRole::WalkLightLanding:
			case ERpgMotionMatchingDatabaseRole::WalkHeavyLanding:
			case ERpgMotionMatchingDatabaseRole::RunLightLanding:
			case ERpgMotionMatchingDatabaseRole::RunHeavyLanding:
				Entry.AnimAsset = Landing;
				break;
			default:
				Entry.AnimAsset = Ground;
				break;
			}
			Database->AddAnimationAsset(Entry);
		}
		Profile->RuntimeMotionMatchingDatabases.Add(Database);
	}

	FRpgGaspPresentationAssetLookup Lookup;
	float ReentryTime = 99.0f;
	TestTrue(TEXT("A legacy profile without turn timings remains valid"), Profile->ValidateProfile().IsValid());
	TestTrue(TEXT("A legacy profile builds its ordinary presentation lookup"), Lookup.Build(Profile));
	TestFalse(TEXT("Absent timing explicitly requests the full-asset fallback"),
		Lookup.FindTurnInPlaceReentryTime(Turns[0], ReentryTime));
	TestEqual(TEXT("A missing timing never leaks a previous output value"), ReentryTime, 0.0f);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Turns); ++Index)
	{
		FRpgTurnInPlaceClipTiming& Timing = Profile->TurnInPlaceClipTimings.AddDefaulted_GetRef();
		Timing.Asset = Turns[Index];
		Timing.ReentryTimeSeconds = 0.6f + 0.3f * Index;
	}
	TestTrue(TEXT("A complete exact turn timing set validates"), Profile->ValidateProfile().IsValid());
	TestTrue(TEXT("Valid turn timings enter the immutable presentation lookup"), Lookup.Build(Profile));
	Profile->TurnInPlaceClipTimings[0].ReentryTimeSeconds = 1.2f;
	TestTrue(TEXT("The lookup resolves the original exact asset pointer"),
		Lookup.FindTurnInPlaceReentryTime(Turns[0], ReentryTime));
	TestEqual(TEXT("Worker lookup does not reread mutated profile timing"), ReentryTime, 0.6f);
	TestFalse(TEXT("Turn timing does not classify its clip as moving locomotion"),
		Lookup.HasTrait(Turns[0], ERpgGaspPresentationAssetTrait::GroundMoving));

	for (const float InvalidTime : { 0.0f, -0.1f, FixtureLength + 0.01f,
		std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		Profile->TurnInPlaceClipTimings[0].ReentryTimeSeconds = InvalidTime;
		TestTrue(TEXT("Non-positive, non-finite and out-of-asset release times are rejected"),
			Profile->ValidateProfile().bHasInvalidTurnInPlaceTiming);
		TestFalse(TEXT("Invalid turn timing fails lookup construction"), Lookup.Build(Profile));
		TestFalse(TEXT("Failed construction discards the previously cached timing"),
			Lookup.FindTurnInPlaceReentryTime(Turns[0], ReentryTime));
	}
	Profile->TurnInPlaceClipTimings[0].ReentryTimeSeconds = FixtureLength;
	TestTrue(TEXT("An authored marker may include the exact full-asset endpoint"), Profile->ValidateProfile().IsValid());
	Turns[0]->bLoop = true;
	TestTrue(TEXT("A looping turn cannot own an authored release marker"),
		Profile->ValidateProfile().bHasInvalidTurnInPlaceTiming);
	Turns[0]->bLoop = false;
	Profile->TurnInPlaceClipTimings[0].Asset = nullptr;
	TestTrue(TEXT("Null timing assets are rejected"), Profile->ValidateProfile().bHasInvalidTurnInPlaceTiming);
	Profile->TurnInPlaceClipTimings[0].Asset = Turns[1];
	TestTrue(TEXT("Repeated timing assets are rejected"), Profile->ValidateProfile().bHasInvalidTurnInPlaceTiming);
	Profile->TurnInPlaceClipTimings[0].Asset = Ground;
	TestTrue(TEXT("An unrelated asset cannot replace a turn even when counts match"),
		Profile->ValidateProfile().bHasTurnInPlaceTimingCoverageMismatch);
	Profile->TurnInPlaceClipTimings[0].Asset = Turns[0];
	const FRpgTurnInPlaceClipTiming LastTiming = Profile->TurnInPlaceClipTimings.Pop();
	TestTrue(TEXT("Partial timing coverage is rejected instead of mixing early and full completion"),
		Profile->ValidateProfile().bHasTurnInPlaceTimingCoverageMismatch);
	Profile->TurnInPlaceClipTimings.Add(LastTiming);
	TestTrue(TEXT("Restoring complete timings restores the profile"), Profile->ValidateProfile().IsValid());
	TestTrue(TEXT("Restored timings rebuild the cache"), Lookup.Build(Profile));
	Lookup.Reset();
	TestFalse(TEXT("Profile rebinding clears every cached turn time"),
		Lookup.FindTurnInPlaceReentryTime(Turns[0], ReentryTime));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
