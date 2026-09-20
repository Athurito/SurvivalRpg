// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Animation/BlendProfile.h"
#include "Components/BoxComponent.h"
#include "Curves/CurveFloat.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/GarbageCollection.h"
#include "SurvivalRpg/Core/Character/RpgMoverTraversalTypes.h"

namespace RpgMoverTraversalStateTests
{
FRpgMoverTraversalCommand Command(int16 Key)
{
	FRpgMoverTraversalCommand Result;
	Result.Identity.ActivationPredictionKey = Key;
	Result.Identity.MontageSequence = 1;
	Result.Phase = ERpgMoverTraversalPhase::Active;
	return Result;
}

bool RoundTrip(FRpgMoverTraversalSyncState State, FRpgMoverTraversalSyncState& Result, int64& OutBytes)
{
	TArray<uint8> Bytes;
	FMemoryWriter Storage(Bytes);
	FObjectAndNameAsStringProxyArchive Writer(Storage, false);
	bool bWritten = false;
	State.NetSerialize(Writer, nullptr, bWritten);
	OutBytes = Bytes.Num();
	FMemoryReader ReadStorage(Bytes);
	FObjectAndNameAsStringProxyArchive Reader(ReadStorage, false);
	bool bRead = false;
	Result.NetSerialize(Reader, nullptr, bRead);
	return bWritten && bRead && !Reader.IsError() && !Writer.IsError();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMoverTraversalLineageTest,
	"SurvivalRpg.Network.Mover.Traversal.CommandPredecessors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMoverTraversalLineageTest::RunTest(const FString& Parameters)
{
	using namespace RpgMoverTraversalStateTests;
	auto Z = Command(120);
	auto A = Command(121);
	A.RecordPredecessors(Z, Z.Identity);
	auto B = Command(122);
	B.RecordPredecessors(A, A.Identity);
	const FRpgMoverTraversalIdentity ZIdentity = Z.Identity;
	const FRpgMoverTraversalIdentity AIdentity = A.Identity;

	TestTrue(TEXT("New B follows A after ordinary local activation"), B.IsSuccessorOf(AIdentity));
	TestFalse(TEXT("Delayed active A cannot replace already-corrected B"), A.IsSuccessorOf(B.Identity));
	A.Phase = ERpgMoverTraversalPhase::Cancelled;
	TestFalse(TEXT("Delayed A cancellation cannot terminate corrected B"), A.IsSuccessorOf(B.Identity));
	TestTrue(TEXT("Rejected A was absent on authority: B still follows authority Z through retained input history"), B.IsSuccessorOf(ZIdentity));

	// Each native input frame owns its node. Releasing older frames must release indirect provenance too.
	FRpgMoverTraversalCommand RetainedAFrame = A;
	A = {};
	Z = {};
	TestTrue(TEXT("A frame copy retains the actual Z-to-B predecessor path"), B.IsSuccessorOf(ZIdentity));
	RetainedAFrame = {};
	TestFalse(TEXT("Expired NP frames do not create an unbounded strong predecessor chain"), B.IsSuccessorOf(ZIdentity));
	TestTrue(TEXT("Direct predecessor remains available after older input frames expire"), B.IsSuccessorOf(AIdentity));

	auto AfterRejectedA = Command(123);
	AfterRejectedA.RecordPredecessors(B, ZIdentity);
	TestTrue(TEXT("A newly observed authority predecessor remains usable after rejected local history expires"), AfterRejectedA.IsSuccessorOf(ZIdentity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMoverTraversalSnapshotTest,
	"SurvivalRpg.Network.Mover.Traversal.SnapshotSerializationAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMoverTraversalSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace RpgMoverTraversalStateTests;
	FRpgMoverTraversalSyncState Active;
	Active.Command = Command(130);
	Active.Command.bHasPresentationPlayId = true;
	Active.Command.PresentationPlayId = 17;
	Active.Command.Context.FrontLedgeTarget = FTransform(FRotator(0, 35, 0), FVector(125, -27, 100));
	Active.Command.Context.BackLedgeWarpTargetName = TEXT("BackLedge");
	Active.Command.Context.BackLedgeTarget = FTransform(FRotator(0, 35, 0), FVector(175, -27, 100));
	Active.Command.Context.LandingCapsuleLocation = FVector(180, -27, 190);
	Active.Command.BaseVisualTransform = FTransform(FRotator(0, -90, 0), FVector(0, 0, -88));
	Active.MontagePosition = .42f;
	Active.bStartApplied = true;
	FRpgMoverWarpModifierState& Warp = Active.WarpModifiers.AddDefaulted_GetRef();
	Warp.WindowIndex = 1;
	Warp.Value.State = ERootMotionModifierState::Active;
	Warp.Value.StartTime = .3f;
	Warp.Value.EndTime = .8f;
	Warp.Value.PreviousPosition = .4f;
	Warp.Value.CurrentPosition = .42f;
	Warp.Value.StartTransform = FTransform(FRotator(0, 19, 0), FVector(35, -4, 62));
	Warp.Value.ActualStartTime = .31f;
	Warp.Value.TotalRootMotionWithinWindow = FTransform(FVector(70, 3, 80));
	Warp.Value.CachedTargetTransform = Active.Command.Context.FrontLedgeTarget;
	Warp.Value.CachedOffsetFromWarpPoint = FTransform(FRotator(0, 8, 0), FVector(2, 4, -15));
	Warp.Value.RootMotionRemainingAfterNotify = FTransform(FVector(5, 1, 2));
	Warp.Value.AdditionalRotationOffset = FRotator(0, 3, 0);
	Warp.Value.RotationOffset = FRotator(0, 5, 0).Quaternion();
	Warp.Value.bWarpingPaused = true;
	Warp.Value.bRootMotionPaused = true;
	FRpgMoverTraversalSyncState Loaded;
	int64 ActiveBytes = 0;
	TestTrue(TEXT("Full native warp snapshot round-trips"), RoundTrip(Active, Loaded, ActiveBytes));
	TestFalse(TEXT("Round-trip preserves all reconciled stock modifier values"), Active.ShouldReconcile(Loaded));
	TestTrue(TEXT("Active history retains its ordinary GAS play receipt correlation"),
		Loaded.Command.bHasPresentationPlayId && Loaded.Command.PresentationPlayId == Active.Command.PresentationPlayId);
	TestEqual(TEXT("Vault rear target name survives serialization"), Loaded.Command.Context.BackLedgeWarpTargetName, FName(TEXT("BackLedge")));
	TestTrue(TEXT("Vault rear transform survives serialization"), Loaded.Command.Context.BackLedgeTarget.Equals(Active.Command.Context.BackLedgeTarget));
	Loaded.Command.Context.BackLedgeTarget.AddToTranslation(FVector(10, 0, 0));
	TestTrue(TEXT("Correcting only the rear ledge requires replay"), Active.ShouldReconcile(Loaded));
	Loaded = Active;
	Loaded.Command.Context.BackLedgeWarpTargetName = NAME_None;
	TestTrue(TEXT("Correcting rear target ownership requires replay"), Active.ShouldReconcile(Loaded));
	Loaded = Active;
	Loaded.WarpModifiers[0].Value.CachedOffsetFromWarpPoint = FTransform(FVector(9, 0, 0));
	TestTrue(TEXT("A corrected bone warp offset requires replay"), Active.ShouldReconcile(Loaded));

	FRpgMoverTraversalSyncState Terminal = Active;
	Terminal.Command.Phase = ERpgMoverTraversalPhase::Finished;
	Terminal.Command.CompactAppliedEnd();
	Terminal.WarpModifiers.Reset();
	Terminal.MontagePosition = 0.f;
	Terminal.bStartApplied = false;
	Terminal.bEndApplied = true;
	int64 TerminalBytes = 0;
	TestTrue(TEXT("Applied terminal identity round-trips"), RoundTrip(Terminal, Loaded, TerminalBytes));
	TestFalse(TEXT("Compact terminal is stable under reconciliation"), Terminal.ShouldReconcile(Loaded));
	TestTrue(TEXT("Terminal sync stops sending the active warp/context payload"), TerminalBytes * 4 < ActiveBytes);
	TestTrue(TEXT("Terminal carries no modifier/collider/montage resources"), Loaded.WarpModifiers.IsEmpty() &&
		!Loaded.Command.Context.Collider.IsValid() && !Loaded.Command.Context.Montage && Loaded.bEndApplied);
	TestEqual(TEXT("Terminal retains the rear name needed to release its published target"), Loaded.Command.Context.BackLedgeWarpTargetName, FName(TEXT("BackLedge")));
	TestTrue(TEXT("Terminal discards the rear transform payload"), Loaded.Command.Context.BackLedgeTarget.Equals(FTransform::Identity));
	Terminal.Command.PresentationEndPosition = .73f;
	Terminal.Command.PresentationEndBlend.Blend.BlendTime = .37f;
	Terminal.Command.PresentationEndBlend.Blend.BlendOption = EAlphaBlendOption::Custom;
	Terminal.Command.PresentationEndBlend.Blend.CustomCurve = NewObject<UCurveFloat>();
	Terminal.Command.PresentationEndBlend.BlendProfile = NewObject<UBlendProfile>();
	Terminal.Command.RetainObjectsForHistory();
	TestTrue(TEXT("Terminal source-stop presentation round-trips independently of gameplay context"), RoundTrip(Terminal, Loaded, TerminalBytes));
	TestTrue(TEXT("Compact terminal retains the exact play correlation and authored stop pose"), Loaded.Command.bHasPresentationPlayId
		&& Loaded.Command.PresentationPlayId == Terminal.Command.PresentationPlayId
		&& FMath::IsNearlyEqual(Loaded.Command.PresentationEndPosition, Terminal.Command.PresentationEndPosition));
	TestTrue(TEXT("Compact terminal retains effective blend duration, curve, option and profile"),
		FMath::IsNearlyEqual(Loaded.Command.PresentationEndBlend.Blend.BlendTime, Terminal.Command.PresentationEndBlend.Blend.BlendTime)
		&& Loaded.Command.PresentationEndBlend.Blend.BlendOption == Terminal.Command.PresentationEndBlend.Blend.BlendOption
		&& Loaded.Command.PresentationEndBlend.Blend.CustomCurve == Terminal.Command.PresentationEndBlend.Blend.CustomCurve
		&& Loaded.Command.PresentationEndBlend.BlendProfile == Terminal.Command.PresentationEndBlend.BlendProfile);
	Loaded.Command.PresentationPlayId = 23;
	Loaded.Command.PresentationEndPosition += .1f;
	Loaded.Command.PresentationEndBlend = FMontageBlendSettings{};
	TestFalse(TEXT("Cosmetic receipt and blend metadata cannot trigger movement reconciliation"), Terminal.ShouldReconcile(Loaded));

	// Native NP buffers pin asset data themselves, but must observe geometry destruction without retaining
	// its actor/world until the buffer's later GC destruction (the EndPlayMap regression).
	FRpgMoverTraversalSyncState NativeFrame;
	NativeFrame.Command = Command(140);
	NativeFrame.Command.Context.Montage = NewObject<UAnimMontage>();
	NativeFrame.Command.Context.Collider = NewObject<UBoxComponent>();
	NativeFrame.Command.PresentationEndPosition = .5f;
	NativeFrame.Command.PresentationEndBlend.Blend.CustomCurve = NewObject<UCurveFloat>();
	NativeFrame.Command.PresentationEndBlend.BlendProfile = NewObject<UBlendProfile>();
	TestFalse(TEXT("Lifetime-test montage is not an editor-retained standalone asset"), NativeFrame.Command.Context.Montage->HasAnyFlags(RF_Standalone));
	TestFalse(TEXT("Lifetime-test collider is not an editor-retained standalone asset"), NativeFrame.Command.Context.Collider->HasAnyFlags(RF_Standalone));
	TestFalse(TEXT("Lifetime-test curve is not an editor-retained standalone asset"), NativeFrame.Command.PresentationEndBlend.Blend.CustomCurve->HasAnyFlags(RF_Standalone));
	TestFalse(TEXT("Lifetime-test profile is not an editor-retained standalone asset"), NativeFrame.Command.PresentationEndBlend.BlendProfile->HasAnyFlags(RF_Standalone));
	NativeFrame.Command.RetainObjectsForHistory();
	TWeakObjectPtr<UAnimMontage> Montage = NativeFrame.Command.Context.Montage;
	TWeakObjectPtr<UPrimitiveComponent> Collider = NativeFrame.Command.Context.Collider;
	TWeakObjectPtr<UCurveFloat> Curve = NativeFrame.Command.PresentationEndBlend.Blend.CustomCurve;
	TWeakObjectPtr<UBlendProfile> Profile = NativeFrame.Command.PresentationEndBlend.BlendProfile;
	FRpgMoverTraversalSyncState RetainedFrame = NativeFrame;
	NativeFrame = {};
	// Match editor/PIE GC: GlobalMapOverride leaves its source map loaded as an RF_Standalone asset.
	// Collecting unrelated editor assets with RF_NoFlags bypasses their normal unload lifecycle; our
	// transient fixtures above have no keep flags, so their history ownership is still tested fully.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("Copied native frame pins montage data through GC"), Montage.IsValid());
	TestTrue(TEXT("Copied native frame pins source-tail curve and profile through GC"), Curve.IsValid() && Profile.IsValid());
	TestFalse(TEXT("History does not keep destroyed world-owned geometry alive"), Collider.IsValid());
	TestFalse(TEXT("Retained historical collider reference safely expires after GC"), RetainedFrame.Command.Context.Collider.IsValid());
	RetainedFrame.Command.Phase = ERpgMoverTraversalPhase::Cancelled;
	RetainedFrame.Command.CompactAppliedEnd();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Applied end releases the last montage history reference"), Montage.IsValid());
	TestTrue(TEXT("Compact terminal keeps its source-tail curve and profile for delayed observers"), Curve.IsValid() && Profile.IsValid());
	RetainedFrame = {};
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Expiring the last terminal history frame releases its curve"), Curve.IsValid());
	TestFalse(TEXT("Expiring the last terminal history frame releases its profile"), Profile.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMoverFixedPredictionHeadWitnessTest,
	"SurvivalRpg.Network.Mover.Traversal.FixedPredictionHeadWitness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMoverFixedPredictionHeadWitnessTest::RunTest(const FString& Parameters)
{
	using RpgMoverPredictionTests::FFixedPredictionHeadSnapshot;
	FFixedPredictionHeadSnapshot Before;
	Before.bValid = true;
	Before.LocalPendingFrame = 122;
	Before.ServerOffset = 26;
	Before.StepMs = 20;
	Before.ServerFrame = 146;
	Before.SimulationTimeMs = 2900.0;
	FFixedPredictionHeadSnapshot After = Before;
	TestTrue(TEXT("An unchanged valid prediction head is sampled before forward work"), Before.IsSameLocalHead(After));
	// The real correction witness refreshed a stale liaison cache and changed the AP server offset,
	// while the world input head stayed at 122. Neither server-view change is a forward local tick.
	After.ServerOffset = 27;
	After.ServerFrame = 149;
	After.SimulationTimeMs = 2960.0;
	TestTrue(TEXT("A rollback can refresh the stale liaison view without advancing the local prediction head"), Before.IsSameLocalHead(After));
	++After.LocalPendingFrame;
	TestFalse(TEXT("A real forward local step cannot be hidden by refreshed liaison clocks"), Before.IsSameLocalHead(After));
	After = Before;
	++After.LocalPendingFrame;
	TestFalse(TEXT("A real forward local step is rejected even if the liaison cache has not updated"), Before.IsSameLocalHead(After));
	After = Before;
	After.bValid = false;
	TestFalse(TEXT("An unavailable after snapshot cannot prove absence of forward work"), Before.IsSameLocalHead(After));
	TestFalse(TEXT("An unavailable before snapshot cannot prove absence of forward work"), After.IsSameLocalHead(Before));
	After = Before;
	After.StepMs = 10;
	TestFalse(TEXT("A changed fixed step cannot share the same prediction-head witness"), Before.IsSameLocalHead(After));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMoverTraversalPresentationInterpolationTest,
	"SurvivalRpg.Network.Mover.Traversal.PresentationInterpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMoverTraversalPresentationInterpolationTest::RunTest(const FString& Parameters)
{
	using namespace RpgMoverTraversalStateTests;
	FRpgMoverTraversalSyncState From;
	From.Command = Command(150);
	From.Command.Context.Montage = NewObject<UAnimMontage>();
	From.Command.Context.FrontLedgeTarget = FTransform(FVector(100, 0, 90));
	From.MontagePosition = .4f;
	From.bStartApplied = true;
	FRpgMoverWarpModifierState& InitialWarp = From.WarpModifiers.AddDefaulted_GetRef();
	InitialWarp.WindowIndex = 1;
	InitialWarp.Value.State = ERootMotionModifierState::Active;
	InitialWarp.Value.StartTransform = FTransform(FVector(10, 20, 30));
	InitialWarp.Value.CurrentPosition = .4f;
	FRpgMoverTraversalSyncState To = From;
	To.MontagePosition = .8f;
	To.Command.Context.FrontLedgeTarget = FTransform(FVector(120, 10, 90));
	To.WarpModifiers[0].Value.StartTransform = FTransform(FVector(40, 50, 60));
	To.WarpModifiers[0].Value.CurrentPosition = .8f;
	FRpgMoverTraversalSyncState Presented;
	Presented.Interpolate(From, To, .5f);
	TestTrue(TEXT("A single active play presents its montage at the interpolated movement time"), FMath::IsNearlyEqual(Presented.MontagePosition, .6f));
	TestTrue(TEXT("Target, lifecycle and identity remain the From snapshot until the endpoint"), Presented.Command.Equals(From.Command) && Presented.bStartApplied && !Presented.bEndApplied);
	TestTrue(TEXT("Mutable SkewWarp caches are never synthesized between history snapshots"), Presented.WarpModifiers.Num() == 1 && Presented.WarpModifiers[0].Equals(From.WarpModifiers[0]));
	Presented.Interpolate(From, To, 0.f);
	TestFalse(TEXT("The first endpoint exactly reproduces From"), Presented.ShouldReconcile(From));
	Presented.Interpolate(From, To, 1.f);
	TestFalse(TEXT("The last endpoint exactly reproduces To"), Presented.ShouldReconcile(To));

	// Reusing the montage asset does not identify the play: the next activation/sequence is discrete.
	FRpgMoverTraversalSyncState NextPlay = To;
	++NextPlay.Command.Identity.MontageSequence;
	NextPlay.MontagePosition = .05f;
	Presented.Interpolate(From, NextPlay, .75f);
	TestFalse(TEXT("A different play of the same asset cannot pull the current montage toward its start"), Presented.ShouldReconcile(From));
	Presented.Interpolate(From, NextPlay, 1.f);
	TestFalse(TEXT("The successor play becomes visible only at its own endpoint"), Presented.ShouldReconcile(NextPlay));

	FRpgMoverTraversalSyncState DifferentAsset = To;
	DifferentAsset.Command.Context.Montage = NewObject<UAnimMontage>();
	Presented.Interpolate(From, DifferentAsset, .5f);
	TestFalse(TEXT("Different montage assets never share an interpolated playback cursor"), Presented.ShouldReconcile(From));

	const FRpgMoverTraversalSyncState Idle;
	FRpgMoverTraversalSyncState Onset = To;
	Onset.Command.Context.StartTimeSeconds = .2f;
	Presented.Interpolate(Idle, Onset, 0.f);
	TestFalse(TEXT("Traversal onset preserves the exact inactive first endpoint"), Presented.ShouldReconcile(Idle));
	// Stock Mover presents the destination movement mode during the interval. NP may fill several
	// missing frames here, so the full traversal must accompany that mode before the final endpoint.
	for (float Pct : { .001f, .25f, .5f, .75f, .99f })
	{
		Presented.Interpolate(Idle, Onset, Pct);
		TestTrue(TEXT("Traversal onset accompanies the destination movement mode throughout a filled gap"),
			Presented.Command.IsActive() && Presented.bStartApplied && !Presented.bEndApplied);
		TestTrue(TEXT("Onset playback interpolates from the authored start rather than waiting for the endpoint"),
			FMath::IsNearlyEqual(Presented.MontagePosition, FMath::Lerp(.2f, .8f, Pct)));
		TestTrue(TEXT("Onset copies the complete destination command and targets without blending"), Presented.Command.Equals(Onset.Command));
		TestTrue(TEXT("Onset copies the exact destination warp cache without synthesizing modifier state"),
			Presented.WarpModifiers.Num() == 1 && Presented.WarpModifiers[0].Equals(Onset.WarpModifiers[0]));
	}
	Presented.Interpolate(Idle, Onset, 1.f);
	TestFalse(TEXT("Traversal onset preserves the exact active last endpoint"), Presented.ShouldReconcile(Onset));
	for (ERpgMoverTraversalPhase Phase : { ERpgMoverTraversalPhase::Finished, ERpgMoverTraversalPhase::Cancelled })
	{
		FRpgMoverTraversalSyncState Terminal = To;
		Terminal.Command.Phase = Phase;
		Terminal.bEndApplied = true;
		Terminal.Command.CompactAppliedEnd();
		Terminal.WarpModifiers.Reset();
		Terminal.MontagePosition = 0.f;
		Terminal.bStartApplied = false;
		Presented.Interpolate(From, Terminal, .99f);
		TestFalse(TEXT("A terminal tombstone cannot rewind or stop the preceding active montage early"), Presented.ShouldReconcile(From));
		Presented.Interpolate(From, Terminal, 1.f);
		TestFalse(TEXT("The terminal endpoint releases the exact play identity"), Presented.ShouldReconcile(Terminal));
	}
	return true;
}

#endif
