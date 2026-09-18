#include "NetworkPredictionFixedInterpolationRecovery.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgFixedInterpolationRecoveryTest,
	"SurvivalRpg.NetworkPrediction.FixedInterpolationRecovery.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgFixedInterpolationRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace UE::NetworkPrediction::FixedInterpolationRecovery;
	FFixedTickState State;
	State.FixedStepMS = 20;
	State.FixedStepRealTimeMS = 20.f;
	State.PendingFrame = 71;
	State.Offset = 900;
	State.Interpolation.LatestRecvFrameSP = 100;
	State.Interpolation.ToFrame = 95;
	State.Interpolation.AccumulatedTimeMS = 7.f;
	UpdateTime(State);
	TestFalse(TEXT("Normal 100 ms buffer does not recenter"), Recover(State, 100, 250));
	Advance(State, 8.f);
	TestEqual(TEXT("Substep remains on same ToFrame"), State.Interpolation.ToFrame, 95);
	TestEqual(TEXT("Substep retains fractional time"), State.Interpolation.PCT, .75f);
	Advance(State, 5.f);
	TestEqual(TEXT("Exact step advances once"), State.Interpolation.ToFrame, 96);
	TestEqual(TEXT("Exact step consumes accumulator"), State.Interpolation.AccumulatedTimeMS, 0.f);

	// A two-second network dispatch stall exceeds the actual 64-frame history.
	State.Interpolation.LatestRecvFrameSP = 200;
	TestTrue(TEXT("Overrun recovers before history access"), Recover(State, 100, 250));
	TestEqual(TEXT("Recovery retains five buffered frames"), State.Interpolation.ToFrame, 195);
	TestEqual(TEXT("Recovery FromFrame is live"), State.Interpolation.ToFrame - 1, 194);
	TestEqual(TEXT("Recovery resets fractional progress"), State.Interpolation.PCT, 0.f);
	TestEqual(TEXT("Recovery updates cue presentation time"), State.Interpolation.InterpolatedTimeMS, 3880);
	TestFalse(TEXT("Same receive head cannot cause a recovery loop"), Recover(State, 100, 250));
	Advance(State, 2000.f, true);
	TestEqual(TEXT("Same hitch delta cannot drain recovered buffer"), State.Interpolation.ToFrame, 195);
	State.Interpolation.LatestRecvFrameSP = 201;
	Advance(State, 20.f);
	TestEqual(TEXT("First normal frame resumes without another snap"), State.Interpolation.ToFrame, 196);
	TestEqual(TEXT("Recovery cannot alter local simulation head"), State.PendingFrame, 71);
	TestEqual(TEXT("Recovery cannot alter server offset"), State.Offset, 900);
	State.Interpolation.LatestRecvFrameSP = 209;
	const bool bRecoveredModerateHitch = Recover(State, 100, 250);
	TestTrue(TEXT("Moderate hitch can also exceed the buffer threshold"), bRecoveredModerateHitch);
	Advance(State, 160.f, bRecoveredModerateHitch);
	TestEqual(TEXT("Moderate hitch cannot consume the recovered target buffer"), State.Interpolation.ToFrame, 204);

	// Autonomous receipts also share the world's interpolation timeline.
	State.Interpolation.LatestRecvFrameAP = 301;
	TestTrue(TEXT("AP receipt can recover the shared clock"), Recover(State, 100, 250));
	TestEqual(TEXT("Newest AP/SP frame wins"), State.Interpolation.ToFrame, 296);

	State.Interpolation.ToFrame = 300;
	Advance(State, 100.f);
	TestEqual(TEXT("Advance cannot outrun last received frame"), State.Interpolation.ToFrame, 301);
	TestEqual(TEXT("Starvation holds the exact newest endpoint"), State.Interpolation.PCT, 1.f);
	TestEqual(TEXT("Excess wall time cannot accumulate beyond the endpoint"), State.Interpolation.AccumulatedTimeMS, 20.f);
	TestEqual(TEXT("Newest received state is actually presented"), State.Interpolation.InterpolatedTimeMS, 6020);
	TestFalse(TEXT("Starvation must not rewind the presentation clock"), Recover(State, 100, 250));

	State.Interpolation.ToFrame = 100;
	State.Interpolation.LatestRecvFrameAP = 1000;
	TestTrue(TEXT("Excessive user tuning still recovers into live history"), Recover(State, 10000, 20000));
	TestEqual(TEXT("History reserves a valid FromFrame"), State.Interpolation.ToFrame, 938);
	TestTrue(TEXT("From and To are inside history"), 1000 - (State.Interpolation.ToFrame - 1) < HistoryCapacity);
	State.Interpolation.ToFrame = 937;
	TestTrue(TEXT("A FromFrame alias at exactly one full ring must recover"), Recover(State, 100, 20000));
	TestEqual(TEXT("Boundary recovery uses configured target"), State.Interpolation.ToFrame, 995);

	State.Interpolation.ToFrame = INDEX_NONE;
	TestFalse(TEXT("Unstarted interpolation retains engine initialization"), Recover(State, 100, 250));
	return true;
}
#endif
