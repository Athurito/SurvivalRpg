// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "MotionWarpingComponent.h"
#include "MoverSimulationTypes.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"

namespace RpgMoverRootMotionTests
{
	/** Pure movement-conversion fixture: no map, BeginPlay, animation playback, network sockets or saves. */
	class FScopedWorld final
	{
	public:
		FScopedWorld()
		{
			const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Initialization);
		}
		~FScopedWorld() { if (World) World->DestroyWorld(false); }
		UWorld* World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMoverRootMotionIgnoresSmoothingTest,
	"SurvivalRpg.Network.Mover.RootMotion.SmoothingCannotChangeHistoricalMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMoverRootMotionIgnoresSmoothingTest::RunTest(const FString& Parameters)
{
	RpgMoverRootMotionTests::FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Transient movement fixture exists"), Fixture.World)) return false;
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags = RF_Transient;
	APawn* Pawn = Fixture.World->SpawnActor<APawn>(Spawn);
	if (!TestNotNull(TEXT("Historical owning-client pawn exists"), Pawn)) return false;
	Pawn->SetRole(ROLE_AutonomousProxy);
	USceneComponent* Root = NewObject<USceneComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->SetRootComponent(Root);
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Pawn, NAME_None, RF_Transient);
	Mesh->SetupAttachment(Root);
	const FTransform BaseVisual(FRotator(0.0, -90.0, 0.0), FVector(0.0, 0.0, -88.0));
	Mesh->SetRelativeTransform(BaseVisual);
	URpgCharacterMoverComponent* Mover = NewObject<URpgCharacterMoverComponent>(Pawn, NAME_None, RF_Transient);
	Mover->SetUpdatedComponent(Root);
	Mover->SetPrimaryVisualComponent(Mesh);
	Pawn->SetActorTransform(FTransform(FRotator(0.0, 110.0, 0.0), FVector(750.0, 220.0, 90.0)));
	const FTransform ActorBefore = Pawn->GetActorTransform();
	// SetupAttachment intentionally leaves these unregistered components out of AttachChildren.
	// Refresh the child's cached world transform after moving the parent, as registration would do.
	Mesh->UpdateComponentToWorld();
	TestTrue(TEXT("Unsmoothed reference mesh is at its authored offset from the rendered actor"),
		Mesh->GetComponentTransform().Equals(BaseVisual * ActorBefore, 0.001));

	// The sync actor is deliberately from another frame. The rendered actor and its smoothed mesh
	// must not influence a rollback's gameplay direction, translation or rotation contribution.
	const FTransform HistoricalActor(FRotator(0.0, 27.0, 0.0), FVector(125.0, -47.0, 90.0));
	FMoverTickStartData Start;
	FMoverDefaultSyncState& Sync = Start.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	Sync.SetTransforms_WorldSpace(HistoricalActor.GetLocation(), HistoricalActor.Rotator(), FVector::ZeroVector, FVector::ZeroVector);
	FRpgMoverAbilityRootMotion Move;
	Move.AbilityHandle = FGameplayAbilitySpec(static_cast<UGameplayAbility*>(nullptr)).Handle;
	Move.ActivationPredictionKey = 42;
	Move.MontageSequence = 1;
	Move.StartSimTimeMs = 1000.0;
	Move.MontageState.StartingMontagePosition = 0.2f;
	Move.MontageState.PlayRate = 1.0f;
	Start.InputCmd.InputCollection.FindOrAddMutableDataByType<FRpgMoverAbilityRootMotionInputs>().RootMotion = Move;
	FMoverTimeStep Step;
	Step.ServerFrame = 80;
	Step.BaseSimTimeMs = 1200.0;
	Step.StepMs = 16.0;
	Step.bIsResimulating = true;

	// Synthetic extraction keeps this test about conversion, independent of a particular montage asset.
	// Real playback, correction and notify delivery remain covered by the network gameplay tests.
	const FTransform LocalDelta(FRotator(3.0, 12.0, -2.0), FVector(12.0, 4.0, 2.0));
	const FVector WorldAdjustment(3.0, -2.0, 1.0);
	int32 LocalCalls = 0, WorldCalls = 0;
	bool bContextPreserved = true;
	Mover->ProcessLocalRootMotionDelegate.BindLambda([&](const FTransform&, float DeltaSeconds, const FMotionWarpingUpdateContext* Context)
	{
		++LocalCalls;
		if (Context)
			bContextPreserved &= FMath::IsNearlyEqual(DeltaSeconds, 0.016f)
				&& FMath::IsNearlyEqual(Context->PreviousPosition, 0.4f)
				&& FMath::IsNearlyEqual(Context->CurrentPosition, 0.416f);
		return LocalDelta;
	});
	Mover->ProcessWorldRootMotionDelegate.BindLambda([&](const FTransform& RootMotion, float, const FMotionWarpingUpdateContext*)
	{
		++WorldCalls;
		FTransform Result = RootMotion;
		Result.AddToTranslation(WorldAdjustment);
		return Result;
	});
	const FTransform ExpectedWorld = Mover->ConvertLocalRootMotionToWorld(FTransform::Identity, 0.016f, &HistoricalActor);
	FProposedMove Baseline;
	TestTrue(TEXT("Historical input produces a root-motion contribution"), Move.GenerateMove(Start, Step, Mover, nullptr, Baseline));
	TestTrue(TEXT("World conversion preserves the engine's unsmoothed mesh-base semantics"),
		Baseline.LinearVelocity.Equals(ExpectedWorld.GetTranslation() / 0.016, 0.01));
	const FVector ExpectedAngularVelocity = FMath::RadiansToDegrees(
		ExpectedWorld.GetRotation().GetShortestArcWith(FQuat::Identity).ToRotationVector() / 0.016);
	TestTrue(TEXT("World conversion preserves the engine's unsmoothed root-rotation semantics"),
		Baseline.AngularVelocityDegrees.Equals(ExpectedAngularVelocity, 0.01));
	TestEqual(TEXT("Baseline invokes each original conversion delegate once"), LocalCalls, 2);
	TestEqual(TEXT("Baseline preserves the original world-space delegate"), WorldCalls, 2);

	Mesh->SetRelativeTransform(FTransform(FRotator(8.0, -54.0, -5.0), FVector(-23.0, 17.0, -95.0)));
	const FTransform SmoothedBefore = Mesh->GetComponentTransform();
	FProposedMove Smoothed;
	TestTrue(TEXT("The same historical input also runs while the mesh is being smoothed"), Move.GenerateMove(Start, Step, Mover, nullptr, Smoothed));
	TestTrue(TEXT("Smoothing cannot rotate or translate the gameplay root-motion contribution"),
		Smoothed.LinearVelocity.Equals(Baseline.LinearVelocity, 0.01));
	TestTrue(TEXT("Smoothing cannot change the gameplay angular velocity"),
		Smoothed.AngularVelocityDegrees.Equals(Baseline.AngularVelocityDegrees, 0.01));
	TestEqual(TEXT("The second move invokes the local delegate exactly once"), LocalCalls, 3);
	TestEqual(TEXT("The second move invokes the world delegate exactly once"), WorldCalls, 3);
	TestTrue(TEXT("Engine extraction timing reaches the original delegates"), bContextPreserved);
	TestTrue(TEXT("Conversion leaves the actor transform untouched"), Pawn->GetActorTransform().Equals(ActorBefore));
	TestTrue(TEXT("Conversion leaves the smoothed mesh transform untouched"), Mesh->GetComponentTransform().Equals(SmoothedBefore));
	TestTrue(TEXT("Conversion preserves the captured gameplay mesh base"), Mover->GetBaseVisualComponentTransform().Equals(BaseVisual));

	TestTrue(TEXT("Original local delegate remains installed after the scope"),
		Mover->ProcessLocalRootMotionDelegate.Execute(FTransform::Identity, 0.016f, nullptr).Equals(LocalDelta));
	TestTrue(TEXT("Original world delegate remains installed after the scope"),
		Mover->ProcessWorldRootMotionDelegate.Execute(FTransform::Identity, 0.016f, nullptr).GetLocation().Equals(WorldAdjustment));
	TestEqual(TEXT("Restored local delegate is invoked once"), LocalCalls, 4);
	TestEqual(TEXT("Restored world delegate is invoked once"), WorldCalls, 4);
	Mover->ProcessLocalRootMotionDelegate.Unbind();
	Mover->ProcessWorldRootMotionDelegate.Unbind();
	return true;
}

#endif
