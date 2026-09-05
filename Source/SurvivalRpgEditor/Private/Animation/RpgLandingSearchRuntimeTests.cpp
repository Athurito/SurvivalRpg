// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgLandingRuntime.h"
#include "SurvivalRpg/Animation/RpgMotionMatchingRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLandingSearchRuntimeTest,
	"SurvivalRpg.Animation.Jump.Runtime.LandingGroundSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLandingSearchRuntimeTest::RunTest(const FString& Parameters)
{
	const FRpgGaspLocomotionTuning Tuning;
	FRpgLandingActiveSnapshot Moving;
	Moving.Eligibility.MovementState = ERpgLocomotionMovementState::Grounded;
	Moving.Eligibility.bIsMovingOnGround = true;
	Moving.Availability.bStandLight = true;
	Moving.Availability.bStandHeavy = true;
	Moving.Availability.bWalkLight = true;
	Moving.Availability.bWalkHeavy = true;
	Moving.Availability.bRunLight = true;
	Moving.Availability.bRunHeavy = true;
	Moving.LiveGait = ERpgLocomotionGait::Run;
	Moving.GroundSpeed = 450.0f;
	Moving.bChooserMoving = true;

	auto BeginLanding = [&Tuning](ERpgMotionMatchingDatabaseRole Role)
	{
		return RpgLandingRuntime::BeginRequest(FRpgLandingRuntimeState(), Role, true, Tuning).State;
	};
	auto SearchMode = [&Tuning](const FRpgLandingRuntimeState& State, bool bDomainChanged = false)
	{
		return RpgLandingRuntime::ResolveSearchMode(State, true, true, bDomainChanged, Tuning);
	};

	FRpgLandingRuntimeState Initial = BeginLanding(ERpgMotionMatchingDatabaseRole::RunLightLanding);
	TestEqual(TEXT("Touchdown starts with one landing search"), SearchMode(Initial),
		ERpgLandingSearchMode::SearchRequestedLanding);
	TestTrue(TEXT("The initial landing search consumes ForceInterrupt once"),
		RpgLandingRuntime::ConsumeForceInterrupt(true, true, Initial));
	TestFalse(TEXT("Repeated callbacks cannot consume another touchdown interrupt"),
		RpgLandingRuntime::ConsumeForceInterrupt(true, true, Initial));
	Initial.bSelectionLatched = true;
	TestEqual(TEXT("A selected contact pose initially excludes other candidates"), SearchMode(Initial),
		ERpgLandingSearchMode::ContinueSelectedLanding);

	const int32 FrameRates[] = {15, 30, 60, 120};
	for (const int32 FrameRate : FrameRates)
	{
		const float DeltaSeconds = 1.0f / static_cast<float>(FrameRate);
		FRpgLandingRuntimeState State = Initial;
		float ReleaseTime = -1.0f;
		for (int32 Frame = 0; Frame < FrameRate / 2; ++Frame)
		{
			const FRpgLandingRuntimeResult Result = RpgLandingRuntime::UpdateActive(State, Moving, DeltaSeconds, Tuning);
			State = Result.State;
			const bool bWindowElapsed = State.TouchdownElapsed >= Tuning.LandingExclusiveSearchDuration;
			TestEqual(*FString::Printf(TEXT("%d FPS frame %d selects by physical touchdown age"), FrameRate, Frame),
				SearchMode(State), bWindowElapsed ? ERpgLandingSearchMode::SearchGroundDuringLanding
					: ERpgLandingSearchMode::ContinueSelectedLanding);
			TestEqual(TEXT("Opening ground search does not end the selected playback"), Result.Transition,
				ERpgLandingRuntimeTransition::None);
			TestFalse(TEXT("Opening ground search neither clears selection nor arms completion"),
				Result.bClearSelection || State.bCompletionArmed);
			TestEqual(TEXT("Ground search never creates another request"), State.RequestSerial, Initial.RequestSerial);
			TestEqual(TEXT("Ground search never rearms ForceInterrupt"), State.InterruptedRequestSerial,
				Initial.InterruptedRequestSerial);
			if (State.bGroundSearchReleased && ReleaseTime < 0.0f)
			{
				ReleaseTime = State.TouchdownElapsed;
			}
		}
		TestTrue(*FString::Printf(TEXT("%d FPS opens within one update after the contact window"), FrameRate),
			ReleaseTime >= Tuning.LandingExclusiveSearchDuration &&
			ReleaseTime <= Tuning.LandingExclusiveSearchDuration + DeltaSeconds + UE_KINDA_SMALL_NUMBER);
	}

	// A hitch crosses the boundary once; no frame-count deadline or playback-clock reset may extend it.
	FRpgLandingRuntimeState HitchState = Initial;
	const float HitchSteps[] = {0.08f, 0.11f, 0.2f, 0.016f};
	for (const float Step : HitchSteps)
	{
		HitchState.StateElapsed = 0.0f;
		HitchState = RpgLandingRuntime::UpdateActive(HitchState, Moving, Step, Tuning).State;
	}
	TestTrue(TEXT("Hitches and playback-clock resets retain physical touchdown age"),
		FMath::IsNearlyEqual(HitchState.TouchdownElapsed, 0.406f));
	TestEqual(TEXT("A boundary-crossing hitch leaves ground search available"), SearchMode(HitchState),
		ERpgLandingSearchMode::SearchGroundDuringLanding);

	FRpgLandingRuntimeState Boundary = Initial;
	Boundary.TouchdownElapsed = Tuning.LandingExclusiveSearchDuration - 0.0001f;
	TestEqual(TEXT("The instant before the boundary retains the contact pose"), SearchMode(Boundary),
		ERpgLandingSearchMode::ContinueSelectedLanding);
	Boundary.TouchdownElapsed = Tuning.LandingExclusiveSearchDuration;
	TestEqual(TEXT("The exact contact boundary opens ground candidates"), SearchMode(Boundary),
		ERpgLandingSearchMode::SearchGroundDuringLanding);
	Boundary.bSelectionLatched = false;
	TestEqual(TEXT("An unselected late request cannot start after the contact window"), SearchMode(Boundary),
		ERpgLandingSearchMode::SearchGroundDuringLanding);

	FRpgGroundMotionMatchingDomainState Running;
	Running.PhysicalMovementState = ERpgLocomotionMovementState::Grounded;
	Running.Stance = ERpgLocomotionStance::Standing;
	Running.Gait = ERpgLocomotionGait::Run;
	Running.bChooserMoving = true;
	FRpgGroundMotionMatchingDomainState Airborne = Running;
	Airborne.PhysicalMovementState = ERpgLocomotionMovementState::Airborne;
	TestFalse(TEXT("The physical touchdown edge does not interrupt its own landing"),
		RpgLandingRuntime::DidGroundDomainChange(true, Airborne, Running));
	TestFalse(TEXT("Missing history does not create a false live-input interruption"),
		RpgLandingRuntime::DidGroundDomainChange(false, Running, Running));
	TestFalse(TEXT("Held movement does not create a live-domain interruption"),
		RpgLandingRuntime::DidGroundDomainChange(true, Running, Running));
	FRpgGroundMotionMatchingDomainState Stopped = Running;
	Stopped.bChooserMoving = false;
	Stopped.Gait = ERpgLocomotionGait::Idle;
	FRpgGroundMotionMatchingDomainState Walking = Running;
	Walking.Gait = ERpgLocomotionGait::Walk;
	const FRpgGroundMotionMatchingDomainState ChangedDomains[] = {Stopped, Walking};
	for (const FRpgGroundMotionMatchingDomainState& Changed : ChangedDomains)
	{
		const bool bChanged = RpgLandingRuntime::DidGroundDomainChange(true, Running, Changed);
		TestTrue(TEXT("Stop and live gait changes release a selected landing early"), bChanged);
		FRpgLandingRuntimeState State = Initial;
		State.TouchdownElapsed = 0.1f;
		const ERpgLandingSearchMode Mode = SearchMode(State, bChanged);
		TestEqual(TEXT("The changed domain can be searched in the same callback"), Mode,
			ERpgLandingSearchMode::SearchGroundDuringLanding);
		TestFalse(TEXT("The search decision is pure and leaves mutation to the facade"), State.bGroundSearchReleased);
		State.bGroundSearchReleased = Mode == ERpgLandingSearchMode::SearchGroundDuringLanding;
		State = RpgLandingRuntime::UpdateActive(State, Moving, 0.01f, Tuning).State;
		TestEqual(TEXT("Consuming the domain change never restores the exclusive landing hold"), SearchMode(State),
			ERpgLandingSearchMode::SearchGroundDuringLanding);
		TestEqual(TEXT("Early release keeps the original selected role and request identity"), State.ActiveRole,
			Initial.ActiveRole);
		TestEqual(TEXT("Early release does not create a new request"), State.RequestSerial, Initial.RequestSerial);
	}

	FRpgLandingRuntimeState Stationary = BeginLanding(ERpgMotionMatchingDatabaseRole::StandHeavyLanding);
	Stationary.TouchdownElapsed = 0.29f;
	Stationary.StateElapsed = 0.2f;
	Stationary.bSelectionLatched = true;
	const FRpgLandingRuntimeResult Handoff = RpgLandingRuntime::UpdateActive(Stationary, Moving, 0.0f, Tuning);
	TestEqual(TEXT("Movement at 0.29 seconds still permits the single heavy handoff"), Handoff.Transition,
		ERpgLandingRuntimeTransition::BeginLanding);
	TestEqual(TEXT("The early handoff preserves Heavy severity"), Handoff.State.ActiveRole,
		ERpgMotionMatchingDatabaseRole::RunHeavyLanding);
	TestEqual(TEXT("The handoff preserves touchdown age"), Handoff.State.TouchdownElapsed, Stationary.TouchdownElapsed);
	FRpgLandingRuntimeState SelectedHandoff = Handoff.State;
	SelectedHandoff.bSelectionLatched = true;
	SelectedHandoff.StateElapsed = 0.0f;
	const FRpgLandingRuntimeResult ReleasedHandoff =
		RpgLandingRuntime::UpdateActive(SelectedHandoff, Moving, 0.02f, Tuning);
	TestTrue(TEXT("A handoff and later playback observation cannot restart the contact window"),
		ReleasedHandoff.State.bGroundSearchReleased);
	TestEqual(TEXT("Handoff release keeps its existing request serial"), ReleasedHandoff.State.RequestSerial,
		Handoff.State.RequestSerial);
	Stationary.bGroundSearchReleased = true;
	const FRpgLandingRuntimeResult NoHandoff = RpgLandingRuntime::UpdateActive(Stationary, Moving, 0.0f, Tuning);
	TestEqual(TEXT("Previously released stationary playback cannot hand off again"), NoHandoff.Transition,
		ERpgLandingRuntimeTransition::None);
	TestEqual(TEXT("A released stationary request keeps its serial"), NoHandoff.State.RequestSerial, Stationary.RequestSerial);
	const FRpgLandingRuntimeResult DirectHandoff = RpgLandingRuntime::BeginRequest(
		Stationary, ERpgMotionMatchingDatabaseRole::RunHeavyLanding, false, Tuning);
	TestEqual(TEXT("The handoff API also rejects released requests"), DirectHandoff.State.RequestSerial,
		Stationary.RequestSerial);
	TestEqual(TEXT("Rejected handoff keeps the original landing playback"), DirectHandoff.State.ActiveRole,
		Stationary.ActiveRole);
	TestFalse(TEXT("A released request cannot consume a delayed ForceInterrupt"),
		RpgLandingRuntime::ConsumeForceInterrupt(true, true, Stationary));
	FRpgLandingRuntimeState CompletedStationary = Stationary;
	CompletedStationary.bGroundSearchReleased = false;
	CompletedStationary.bCompletionArmed = true;
	const FRpgLandingRuntimeResult CompletedWithNewInput =
		RpgLandingRuntime::UpdateActive(CompletedStationary, Moving, 0.0f, Tuning);
	TestEqual(TEXT("New movement cannot hand off already completed landing playback"),
		CompletedWithNewInput.Transition, ERpgLandingRuntimeTransition::ResetGrounded);
	TestEqual(TEXT("Completion wins over a handoff without creating another request"),
		CompletedWithNewInput.State.RequestSerial, CompletedStationary.RequestSerial);

	const FRpgLandingRuntimeState NewTouchdown = RpgLandingRuntime::BeginRequest(
		Stationary, ERpgMotionMatchingDatabaseRole::RunLightLanding, true, Tuning).State;
	TestFalse(TEXT("A new physical touchdown clears the old release latch"), NewTouchdown.bGroundSearchReleased);
	TestEqual(TEXT("A new physical touchdown starts a fresh contact clock"), NewTouchdown.TouchdownElapsed, 0.0f);
	TestFalse(TEXT("Lifecycle reset clears the release latch"),
		RpgLandingRuntime::Reset(Stationary, Tuning).State.bGroundSearchReleased);
	Initial.bCompletionArmed = true;
	TestEqual(TEXT("Completed playback exits through ordinary locomotion"), SearchMode(Initial),
		ERpgLandingSearchMode::NormalLocomotion);
	TestEqual(TEXT("A missing landing database fails open to ordinary locomotion"),
		RpgLandingRuntime::ResolveSearchMode(NewTouchdown, true, false, false, Tuning),
		ERpgLandingSearchMode::NormalLocomotion);
	TestEqual(TEXT("An interrupted landing phase uses ordinary locomotion"),
		RpgLandingRuntime::ResolveSearchMode(NewTouchdown, false, true, false, Tuning),
		ERpgLandingSearchMode::NormalLocomotion);

	FRpgGaspLocomotionTuning CustomTuning = Tuning;
	CustomTuning.LandingExclusiveSearchDuration = 0.0f;
	TestTrue(TEXT("Zero allows a designer to omit exclusive contact search"),
		RpgGaspLocomotionConfig::IsTuningRuntimeValid(CustomTuning));
	TestEqual(TEXT("Zero contact duration immediately permits ground candidates"),
		RpgLandingRuntime::ResolveSearchMode(NewTouchdown, true, true, false, CustomTuning),
		ERpgLandingSearchMode::SearchGroundDuringLanding);
	CustomTuning.LandingExclusiveSearchDuration = -0.01f;
	TestFalse(TEXT("Negative contact duration is invalid"), RpgGaspLocomotionConfig::IsTuningRuntimeValid(CustomTuning));
	CustomTuning.LandingExclusiveSearchDuration = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Non-finite contact duration is invalid"), RpgGaspLocomotionConfig::IsTuningRuntimeValid(CustomTuning));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
