// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
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
	Active.Command.Context.FrontLedgeTarget = FTransform(FRotator(0, 35, 0), FVector(125, -27, 100));
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

	// Native NP buffers pin asset data themselves, but must observe geometry destruction without retaining
	// its actor/world until the buffer's later GC destruction (the EndPlayMap regression).
	FRpgMoverTraversalSyncState NativeFrame;
	NativeFrame.Command = Command(140);
	NativeFrame.Command.Context.Montage = NewObject<UAnimMontage>();
	NativeFrame.Command.Context.Collider = NewObject<UBoxComponent>();
	NativeFrame.Command.RetainObjectsForHistory();
	TWeakObjectPtr<UAnimMontage> Montage = NativeFrame.Command.Context.Montage;
	TWeakObjectPtr<UPrimitiveComponent> Collider = NativeFrame.Command.Context.Collider;
	FRpgMoverTraversalSyncState RetainedFrame = NativeFrame;
	NativeFrame = {};
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Copied native frame pins montage data through GC"), Montage.IsValid());
	TestFalse(TEXT("History does not keep destroyed world-owned geometry alive"), Collider.IsValid());
	TestFalse(TEXT("Retained historical collider reference safely expires after GC"), RetainedFrame.Command.Context.Collider.IsValid());
	RetainedFrame.Command.Phase = ERpgMoverTraversalPhase::Cancelled;
	RetainedFrame.Command.CompactAppliedEnd();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Applied end releases the last montage history reference"), Montage.IsValid());
	return true;
}

#endif
