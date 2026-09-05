// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesContainer.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "AssetCompilingManager.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "UObject/UnrealType.h"

namespace RpgServerPoseEvaluationTests
{
class FScopedWorld
{
public:
	FScopedWorld()
	{
		GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
		GameInstance->AddToRoot();
		GameInstance->InitializeStandalone();
		World = GameInstance->GetWorld();
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
		}
	}

	~FScopedWorld()
	{
		if (World)
		{
			World->WorldType = EWorldType::Game;
		}
		GameInstance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		GameInstance->RemoveFromRoot();
	}

	UWorld* World = nullptr;

private:
	UGameInstance* GameInstance = nullptr;
};

/** Counts real graph evaluation while retaining the stock SequencePlayer and OffsetRoot implementations. */
struct FRecordingRoot final : FAnimNode_Base
{
	FPoseLink Source;
	TArray<FBoneIndexType> AttributeMeshBones;
	int32 Evaluations = 0;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override { Source.Initialize(Context); }
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override { Source.CacheBones(Context); }
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override { Source.Update(Context); }
	virtual void Evaluate_AnyThread(FPoseContext& Output) override
	{
		++Evaluations;
		Source.Evaluate(Output);
		// Non-root attributes make a changed compact-pose mapping observable independently
		// of the root-motion attribute, whose root index stays zero across every LOD.
		for (const FBoneIndexType MeshBone : AttributeMeshBones)
		{
			const FCompactPoseBoneIndex CompactIndex = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBone));
			if (CompactIndex.IsValid())
			{
				FFloatAnimationAttribute* Attribute = Output.CustomAttributes.FindOrAdd<FFloatAnimationAttribute>(
					UE::Anim::FAttributeId(TEXT("RecacheBoneMarker"), CompactIndex));
				Attribute->Value = 1000.0f + MeshBone;
			}
		}
	}
};

struct FOtherRoot final : FAnimNode_Base
{
	int32 Evaluations = 0;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override
	{
		++Evaluations;
		Output.ResetToRefPose();
	}
};

struct FGraphProxy final : FRpgAnimInstanceProxy
{
	// Expose engine lifecycle entry points only on the editor fixture, never on the runtime proxy API.
	using FAnimInstanceProxy::Initialize;
	using FAnimInstanceProxy::RecalcRequiredBones;
	using FAnimInstanceProxy::InitializeRootNode;
	using FAnimInstanceProxy::UpdateAnimation;
	using FAnimInstanceProxy::EvaluateAnimation_WithRoot;

	FGraphProxy(UAnimInstance* Instance, UAnimSequence* Sequence, float StartTime)
		: FRpgAnimInstanceProxy(Instance)
	{
		Player.SetSequence(Sequence);
		Player.SetLoopAnimation(false);
		Player.SetStartPosition(StartTime);
		Offset.EvaluationMode = EWarpingEvaluationMode::Graph;
		Offset.RotationMode = EOffsetRootBoneMode::Accumulate;
		Offset.TranslationMode = EOffsetRootBoneMode::Accumulate;
		Offset.Source.SetLinkNode(&Player);
		Root.Source.SetLinkNode(&Offset);
	}

	virtual FAnimNode_Base* GetCustomRootNode() override { return &Root; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes = {&Root, &Offset, &Player}; }

	FRecordingRoot Root;
	FAnimNode_OffsetRootBone Offset;
	FAnimNode_SequencePlayer_Standalone Player;
};

struct FRecordedPose
{
	TArray<FTransform> Bones;
	TMap<FName, TPair<float, UE::Anim::ECurveElementFlags>> Curves;
	UE::Anim::FHeapAttributeContainer Attributes;
	FTransform RootMotion = FTransform::Identity;
	FQuat OffsetRotation = FQuat::Identity;
	float AssetTime = 0.0f;
	bool bHasRootMotion = false;
};

struct FFixture
{
	bool Initialize(FAutomationTestBase& Test, ENetMode NetMode)
	{
		if (!Test.TestNotNull(TEXT("Animation fixture world exists"), TestWorld.World))
		{
			return false;
		}
		MeshAsset = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		Sequence = LoadObject<UAnimSequence>(nullptr,
			TEXT("/RpgGaspLocomotion/Animations/Stand/Idle/M_Neutral_Stand_Turn_090_R.M_Neutral_Stand_Turn_090_R"));
		if (!Test.TestNotNull(TEXT("Project mesh loads"), MeshAsset) ||
			!Test.TestNotNull(TEXT("A curated turning sequence loads"), Sequence))
		{
			return false;
		}
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (!Test.TestTrue(TEXT("The fixture sequence supplies real root motion"), Sequence->HasRootMotion()) ||
			!Test.TestNotNull(TEXT("AnimationWarping root-motion provider is available"), UE::Anim::IAnimRootMotionProvider::Get()))
		{
			return false;
		}
		// Find motion in the asset rather than fixing a visual timing/angle contract to this clip.
		StartTime = -1.0f;
		for (float Time = 0.0f; Time + 2.0f * StepSeconds < Sequence->GetPlayLength(); Time += StepSeconds)
		{
			const FTransform First = Sequence->ExtractRootMotion(
				FAnimExtractContext(Time, true, FDeltaTimeRecord(StepSeconds), false));
			const FTransform Next = Sequence->ExtractRootMotion(
				FAnimExtractContext(Time + StepSeconds, true, FDeltaTimeRecord(StepSeconds), false));
			if (FMath::RadiansToDegrees(First.GetRotation().GetAngle()) > 0.25f &&
				FMath::RadiansToDegrees(Next.GetRotation().GetAngle()) > 0.25f)
			{
				StartTime = Time;
				break;
			}
		}
		if (!Test.TestTrue(TEXT("Two consecutive update intervals contain authored rotation"), StartTime >= 0.0f))
		{
			return false;
		}
		Character = TestWorld.World->SpawnActor<ARpgCharacter>();
		if (!Test.TestNotNull(TEXT("Project character spawns"), Character))
		{
			return false;
		}
		TestWorld.World->WorldType = EWorldType::PIE;
		TestWorld.World->SetPlayInEditorInitialNetMode(NetMode);
		Character->SetAutonomousProxy(true);
		Mesh = Character->GetMesh();
		Mesh->SetSkeletalMeshAsset(MeshAsset);
		Mesh->SetAnimInstanceClass(URpgAnimInstance::StaticClass());
		Anim = Cast<URpgAnimInstance>(Mesh->GetAnimInstance());
		if (!Test.TestNotNull(TEXT("Native RPG animation instance initializes"), Anim))
		{
			return false;
		}
		TrajectoryProperty = FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("bGeneratePoseSearchTrajectory"));
		if (!Test.TestNotNull(TEXT("Existing GASP configuration seam is reflected"), TrajectoryProperty))
		{
			return false;
		}
		TrajectoryProperty->SetPropertyValue_InContainer(Anim, true);
		Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
		Mesh->bOnlyAllowAutonomousTickPose = true;
		Mesh->bIsAutonomousTickPose = false;
		Proxy = MakeUnique<FGraphProxy>(Anim, Sequence, StartTime);
		Proxy->Initialize(Anim);
		Proxy->RecalcRequiredBones(Mesh, MeshAsset);
		Proxy->InitializeRootNode();
		return Test.TestTrue(TEXT("Graph uses a valid real bone container"), Proxy->GetRequiredBones().IsValid());
	}

	void Update(float DeltaSeconds = StepSeconds)
	{
		FMemMark Mark(FMemStack::Get());
		Proxy->PreUpdate(Anim, DeltaSeconds);
		Proxy->UpdateAnimation();
	}

	void CompleteAutonomousMove(float DeltaSeconds)
	{
		const bool bWasAutonomousPoseTick = Mesh->bIsAutonomousTickPose;
		Mesh->bIsAutonomousTickPose = true;
		Update(DeltaSeconds);
		Proxy->PostUpdate(Anim);
		Mesh->bIsAutonomousTickPose = bWasAutonomousPoseTick;
	}

	void ChangeRequiredBones(const TArray<FBoneIndexType>& BoneIndices)
	{
		// Exercise a real same-mesh compact mapping change without depending on authored
		// render LOD counts. Both arrays retain the root and every required parent.
		Proxy->GetRequiredBones().InitializeTo(BoneIndices, Mesh->GetCurveFilterSettings(), *MeshAsset);
		Proxy->RecalcRequiredBones(Mesh, MeshAsset);
	}

	FRecordedPose Evaluate(FAnimNode_Base* OtherRoot = nullptr)
	{
		FMemMark Mark(FMemStack::Get());
		Proxy->PreEvaluateAnimation(Anim);
		FPoseContext Output(Proxy.Get());
		Proxy->EvaluateAnimation_WithRoot(Output, OtherRoot ? OtherRoot : Proxy->GetRootNode());
		FRecordedPose Result;
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			Result.Bones.Add(Output.Pose[Index]);
		}
		Output.Curve.ForEachElement([&Result](const UE::Anim::FCurveElement& Curve)
		{
			Result.Curves.Add(Curve.Name, {Curve.Value, Curve.Flags});
		});
		Result.Attributes.CopyFrom(Output.CustomAttributes);
		Result.bHasRootMotion = UE::Anim::IAnimRootMotionProvider::Get()->ExtractRootMotion(Output.CustomAttributes, Result.RootMotion);
		Result.OffsetRotation = Proxy->Offset.GetOffsetRootRotation();
		Result.AssetTime = Proxy->Player.GetCurrentAssetTime();
		return Result;
	}

	static constexpr float StepSeconds = 0.05f;
	FScopedWorld TestWorld;
	ARpgCharacter* Character = nullptr;
	USkeletalMeshComponent* Mesh = nullptr;
	URpgAnimInstance* Anim = nullptr;
	USkeletalMesh* MeshAsset = nullptr;
	UAnimSequence* Sequence = nullptr;
	FBoolProperty* TrajectoryProperty = nullptr;
	float StartTime = 0.0f;
	TUniquePtr<FGraphProxy> Proxy;
};

void TestIdenticalPose(FAutomationTestBase& Test, FRecordedPose& First, FRecordedPose& Repeated)
{
	Test.TestTrue(TEXT("Repeated evaluation preserves sequence time"), FMath::IsNearlyEqual(First.AssetTime, Repeated.AssetTime));
	Test.TestTrue(TEXT("Repeated evaluation preserves the OffsetRoot state"), First.OffsetRotation.Equals(Repeated.OffsetRotation, 1.e-6));
	bool bBonesEqual = First.Bones.Num() == Repeated.Bones.Num();
	for (int32 Index = 0; bBonesEqual && Index < First.Bones.Num(); ++Index)
	{
		bBonesEqual = First.Bones[Index].Equals(Repeated.Bones[Index], 1.e-6);
	}
	Test.TestTrue(TEXT("Every compact-pose bone survives the cache copy"), bBonesEqual);
	bool bCurvesEqual = First.Curves.Num() == Repeated.Curves.Num();
	for (const auto& Curve : First.Curves)
	{
		const auto* Other = Repeated.Curves.Find(Curve.Key);
		bCurvesEqual &= Other && FMath::IsNearlyEqual(Curve.Value.Key, Other->Key, 1.e-6f) && Curve.Value.Value == Other->Value;
	}
	Test.TestTrue(TEXT("All curve values and flags survive the cache copy"), bCurvesEqual);
	Test.TestFalse(TEXT("Every custom attribute survives the cache copy"), First.Attributes != Repeated.Attributes);
	Test.TestTrue(TEXT("The sampled root-motion attribute is preserved"), First.bHasRootMotion && Repeated.bHasRootMotion &&
		First.RootMotion.Equals(Repeated.RootMotion, 1.e-6));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerRepeatedPoseEvaluationTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.RepeatedEvaluation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerRepeatedPoseEvaluationTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	for (const ENetMode NetMode : {NM_ListenServer, NM_DedicatedServer})
	{
		FFixture Fixture;
		if (!Fixture.Initialize(*this, NetMode)) { return false; }
		Fixture.Update();
		Fixture.Proxy->PostUpdate(Fixture.Anim);
		TestEqual(TEXT("A regular server update outside CMC does not force early evaluation"), Fixture.Proxy->Root.Evaluations, 0);
		FRecordedPose First = Fixture.Evaluate();
		TestTrue(TEXT("Remote autonomous server uses the held-pose scope"), Fixture.Proxy->bUseServerPoseEvaluationCache);
		TestTrue(TEXT("The real player consumes an update delta"), First.AssetTime > Fixture.StartTime);
		TestTrue(TEXT("Authored curves are present, not an empty ref-pose control"), !First.Curves.IsEmpty());
		TestTrue(TEXT("A nonzero root-motion delta reached the graph"), First.bHasRootMotion &&
			FMath::RadiansToDegrees(First.RootMotion.GetRotation().GetAngle()) > 0.1f);
		const int32 EvaluationCount = Fixture.Proxy->Root.Evaluations;
		const int16 EngineEvaluationCounter = Fixture.Proxy->GetEvaluationCounter().Get();
		FRecordedPose Repeated = Fixture.Evaluate();
		TestEqual(TEXT("A second refresh without update does not re-enter the graph"), Fixture.Proxy->Root.Evaluations, EvaluationCount);
		TestEqual(TEXT("A held pose does not publish a new graph evaluation epoch"), Fixture.Proxy->GetEvaluationCounter().Get(), EngineEvaluationCounter);
		TestIdenticalPose(*this, First, Repeated);
		Fixture.Update();
		FRecordedPose Advanced = Fixture.Evaluate();
		TestEqual(TEXT("The next genuine update evaluates the graph again"), Fixture.Proxy->Root.Evaluations, EvaluationCount + 1);
		TestTrue(TEXT("The next update advances sequence time"), Advanced.AssetTime > Repeated.AssetTime);
		TestFalse(TEXT("The next authored delta advances OffsetRoot"), Advanced.OffsetRotation.Equals(Repeated.OffsetRotation, 1.e-4));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerPoseScopeTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.ScopeIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerPoseScopeTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this, NM_ListenServer)) { return false; }
	FByteProperty* RemoteRoleProperty = FindFProperty<FByteProperty>(AActor::StaticClass(), TEXT("RemoteRole"));
	if (!TestNotNull(TEXT("Actor remote role is reflected for this native role fixture"), RemoteRoleProperty))
	{
		return false;
	}
	struct FScopeCase { ENetMode NetMode; ENetRole LocalRole; ENetRole RemoteRole; bool bAutonomousTicks; bool bGasp; ERootMotionMode::Type RootMode; };
	const FScopeCase Cases[] = {
		{NM_Client, ROLE_AutonomousProxy, ROLE_Authority, true, true, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_Client, ROLE_SimulatedProxy, ROLE_Authority, true, true, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_ListenServer, ROLE_Authority, ROLE_SimulatedProxy, true, true, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_Standalone, ROLE_Authority, ROLE_AutonomousProxy, true, true, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_ListenServer, ROLE_Authority, ROLE_AutonomousProxy, false, true, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_ListenServer, ROLE_Authority, ROLE_AutonomousProxy, true, false, ERootMotionMode::RootMotionFromMontagesOnly},
		{NM_ListenServer, ROLE_Authority, ROLE_AutonomousProxy, true, true, ERootMotionMode::RootMotionFromEverything}};
	for (const FScopeCase& Scope : Cases)
	{
		Fixture.TestWorld.World->SetPlayInEditorInitialNetMode(Scope.NetMode);
		Fixture.Character->SetRole(Scope.LocalRole);
		RemoteRoleProperty->SetPropertyValue_InContainer(Fixture.Character, static_cast<uint8>(Scope.RemoteRole));
		Fixture.Mesh->bOnlyAllowAutonomousTickPose = Scope.bAutonomousTicks;
		Fixture.TrajectoryProperty->SetPropertyValue_InContainer(Fixture.Anim, Scope.bGasp);
		Fixture.Anim->SetRootMotionMode(Scope.RootMode);
		Fixture.Update();
		const int32 Before = Fixture.Proxy->Root.Evaluations;
		Fixture.Mesh->bIsAutonomousTickPose = true;
		Fixture.Proxy->PostUpdate(Fixture.Anim);
		Fixture.Mesh->bIsAutonomousTickPose = false;
		TestEqual(TEXT("Out-of-scope PostUpdate never performs a server-only eager evaluation"), Fixture.Proxy->Root.Evaluations, Before);
		FRecordedPose First = Fixture.Evaluate();
		FRecordedPose Repeated = Fixture.Evaluate();
		TestFalse(TEXT("Owner, simulated, regular and non-GASP paths remain uncached"), Fixture.Proxy->bUseServerPoseEvaluationCache);
		TestEqual(TEXT("Uncached paths execute both evaluations"), Fixture.Proxy->Root.Evaluations, Before + 2);
		TestTrue(TEXT("The control repeats evaluation without advancing player time"), FMath::IsNearlyEqual(First.AssetTime, Repeated.AssetTime));
		if (&Scope == &Cases[0])
		{
			TestFalse(TEXT("The real uncached graph reproduces duplicate OffsetRoot accumulation"), First.OffsetRotation.Equals(Repeated.OffsetRotation, 1.e-4));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerPoseInvalidationTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.Invalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerPoseInvalidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this, NM_ListenServer)) { return false; }
	Fixture.Update();
	FRecordedPose First = Fixture.Evaluate();
	int32 Count = Fixture.Proxy->Root.Evaluations;
	Fixture.Proxy->InitializeRootNode();
	FRecordedPose Initialized = Fixture.Evaluate();
	TestEqual(TEXT("Root reinitialization cannot reuse an older pose"), Fixture.Proxy->Root.Evaluations, ++Count);
	TestTrue(TEXT("Reinitialization resets actual sequence playback"), FMath::IsNearlyEqual(Initialized.AssetTime, Fixture.StartTime));
	Fixture.Proxy->RecalcRequiredBones(Fixture.Mesh, Fixture.MeshAsset);
	FRecordedPose Recached = Fixture.Evaluate();
	TestEqual(TEXT("A required-bones recache holds the completed update"), Fixture.Proxy->Root.Evaluations, Count);
	TestIdenticalPose(*this, Initialized, Recached);
	FRecordedPose Repeated = Fixture.Evaluate();
	TestEqual(TEXT("The newly cached bones permit holding the completed pose"), Fixture.Proxy->Root.Evaluations, Count);
	TestIdenticalPose(*this, Recached, Repeated);
	FOtherRoot OtherRoot;
	Fixture.Evaluate(&OtherRoot);
	Fixture.Evaluate(&OtherRoot);
	TestEqual(TEXT("Alternate graph roots always evaluate themselves"), OtherRoot.Evaluations, 2);
	FRecordedPose MainAgain = Fixture.Evaluate();
	TestEqual(TEXT("An alternate root cannot replace the main graph cache"), Fixture.Proxy->Root.Evaluations, Count);
	TestIdenticalPose(*this, Repeated, MainAgain);
	Fixture.Proxy->InvalidateServerPoseEvaluationCache();
	Fixture.Evaluate();
	TestEqual(TEXT("Explicit lifecycle invalidation forces a new evaluation"), Fixture.Proxy->Root.Evaluations, Count + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerPoseMultipleUpdatesTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.MultipleMovesBeforeRender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerPoseMultipleUpdatesTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	// Both patterns cover the same authored 100 ms, with different packet/move subdivisions.
	const TArray<TArray<float>> MovePatterns = {{0.05f, 0.05f}, {0.02f, 0.03f, 0.01f, 0.04f}};
	for (const ENetMode NetMode : {NM_ListenServer, NM_DedicatedServer})
	{
		for (const TArray<float>& Deltas : MovePatterns)
		{
			FFixture DeferredRender;
			FFixture RenderEveryMove;
			if (!DeferredRender.Initialize(*this, NetMode) || !RenderEveryMove.Initialize(*this, NetMode))
			{
				return false;
			}
			const uint64 MoveFrame = GFrameCounter;
			const FQuat InitialRoot = DeferredRender.Proxy->Offset.GetOffsetRootRotation();
			FRecordedPose Reference;
			float TotalSeconds = 0.0f;
			for (const float DeltaSeconds : Deltas)
			{
				// CMC finishes several updates before the server's normal render refresh. The
				// production PostUpdate hook must consume each SequencePlayer DeltaTimeRecord.
				DeferredRender.CompleteAutonomousMove(DeltaSeconds);
				RenderEveryMove.CompleteAutonomousMove(DeltaSeconds);
				Reference = RenderEveryMove.Evaluate();
				TotalSeconds += DeltaSeconds;
			}
			TestEqual(TEXT("The packet batch completes within one engine frame"), GFrameCounter, MoveFrame);
			TestEqual(TEXT("Every completed move evaluates once before any render refresh"),
				DeferredRender.Proxy->Root.Evaluations, Deltas.Num());
			const int16 EvaluationEpoch = DeferredRender.Proxy->GetEvaluationCounter().Get();
			FRecordedPose Batched = DeferredRender.Evaluate();
			TestEqual(TEXT("The delayed render refresh does not consume the last move again"),
				DeferredRender.Proxy->Root.Evaluations, Deltas.Num());
			TestEqual(TEXT("The delayed refresh retains the completed evaluation epoch"),
				DeferredRender.Proxy->GetEvaluationCounter().Get(), EvaluationEpoch);
			TestEqual(TEXT("Rendering after each move also consumes each update only once"),
				RenderEveryMove.Proxy->Root.Evaluations, Deltas.Num());
			TestTrue(TEXT("Actual sequence time includes every move delta"), FMath::IsNearlyEqual(
				Batched.AssetTime, DeferredRender.StartTime + TotalSeconds, 1.e-5f));
			TestFalse(TEXT("The comparison includes real accumulated root rotation"),
				Batched.OffsetRotation.Equals(InitialRoot, 1.e-4));
			TestIdenticalPose(*this, Reference, Batched);
			FRecordedPose RepeatedRender = DeferredRender.Evaluate();
			TestIdenticalPose(*this, Batched, RepeatedRender);
			TestEqual(TEXT("Further render refreshes cannot duplicate the final root delta"),
				DeferredRender.Proxy->Root.Evaluations, Deltas.Num());
			TestEqual(TEXT("Move-driven pose evaluation keeps the montage-only extraction policy"),
				DeferredRender.Anim->RootMotionMode.GetValue(), ERootMotionMode::RootMotionFromMontagesOnly);
			TestFalse(TEXT("Locomotion root attributes do not become authoritative extracted root motion"),
				DeferredRender.Proxy->GetExtractedRootMotion().bHasRootMotion);
			TestFalse(TEXT("The per-move reference also leaves montage root-motion ownership unchanged"),
				RenderEveryMove.Proxy->GetExtractedRootMotion().bHasRootMotion);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerPoseRecacheWithoutMoveTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.BoneRecacheWithoutMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerPoseRecacheWithoutMoveTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this, NM_ListenServer)) { return false; }
	Fixture.CompleteAutonomousMove(FFixture::StepSeconds);
	FRecordedPose Before = Fixture.Evaluate();
	if (!TestTrue(TEXT("The recache follows a nonzero authored root delta"), Before.bHasRootMotion &&
		FMath::RadiansToDegrees(Before.RootMotion.GetRotation().GetAngle()) > 0.1f))
	{
		return false;
	}
	const int16 UpdateEpoch = Fixture.Proxy->GetUpdateCounter().Get();
	const int16 BonesEpoch = Fixture.Proxy->GetCachedBonesCounter().Get();
	Fixture.Proxy->RecalcRequiredBones(Fixture.Mesh, Fixture.MeshAsset);
	FRecordedPose After = Fixture.Evaluate();
	const double ReplayedDegrees = FMath::RadiansToDegrees(Before.OffsetRotation.AngularDistance(After.OffsetRotation));
	AddInfo(FString::Printf(TEXT("Bone recache without move: assetTime %.6f -> %.6f, update %d -> %d, bones %d -> %d, root replay %.6f degrees"),
		Before.AssetTime, After.AssetTime, UpdateEpoch, Fixture.Proxy->GetUpdateCounter().Get(),
		BonesEpoch, Fixture.Proxy->GetCachedBonesCounter().Get(), ReplayedDegrees));
	TestEqual(TEXT("The recache did not receive a new animation update"), Fixture.Proxy->GetUpdateCounter().Get(), UpdateEpoch);
	TestTrue(TEXT("A real engine bone-cache invalidation occurred"), Fixture.Proxy->GetCachedBonesCounter().Get() != BonesEpoch);
	TestIdenticalPose(*this, Before, After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgServerPoseMappingWithoutMoveTest,
	"SurvivalRpg.Animation.Threading.ServerPoseEvaluation.BoneMappingWithoutMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerPoseMappingWithoutMoveTest::RunTest(const FString& Parameters)
{
	using namespace RpgServerPoseEvaluationTests;
	for (const bool bStartWithReducedBones : {false, true})
	{
		FFixture Fixture;
		if (!Fixture.Initialize(*this, NM_ListenServer)) { return false; }
		const TArray<FBoneIndexType> FullBones = Fixture.Proxy->GetRequiredBones().GetBoneIndicesArray();
		const FReferenceSkeleton& RefSkeleton = Fixture.MeshAsset->GetRefSkeleton();
		int32 RemovedCompactIndex = INDEX_NONE;
		for (int32 Index = 1; Index + 1 < FullBones.Num(); ++Index)
		{
			const FBoneIndexType Candidate = FullBones[Index];
			const bool bHasRequiredChild = FullBones.ContainsByPredicate([&RefSkeleton, Candidate](FBoneIndexType Bone)
			{
				return RefSkeleton.GetParentIndex(Bone) == Candidate;
			});
			if (!bHasRequiredChild)
			{
				RemovedCompactIndex = Index;
				break;
			}
		}
		if (!TestTrue(TEXT("The real mesh has a removable leaf before another required bone"), RemovedCompactIndex != INDEX_NONE))
		{
			return false;
		}
		const FBoneIndexType RemovedMeshBone = FullBones[RemovedCompactIndex];
		const FBoneIndexType RetainedMeshBone = FullBones.Last();
		TArray<FBoneIndexType> ReducedBones = FullBones;
		ReducedBones.RemoveAt(RemovedCompactIndex);
		const TArray<FBoneIndexType>& OriginalBones = bStartWithReducedBones ? ReducedBones : FullBones;
		Fixture.Proxy->Root.AttributeMeshBones = {RemovedMeshBone, RetainedMeshBone};
		Fixture.ChangeRequiredBones(OriginalBones);
		Fixture.CompleteAutonomousMove(FFixture::StepSeconds);
		FRecordedPose Original = Fixture.Evaluate();
		const int32 Evaluations = Fixture.Proxy->Root.Evaluations;
		const int16 UpdateEpoch = Fixture.Proxy->GetUpdateCounter().Get();
		if (!TestTrue(TEXT("Mapping changes follow a real nonzero turn delta"), Original.bHasRootMotion &&
			FMath::RadiansToDegrees(Original.RootMotion.GetRotation().GetAngle()) > 0.1f))
		{
			return false;
		}
		TestTrue(TEXT("The retained bone has different compact indices in the two mappings"),
			FullBones.IndexOfByKey(RetainedMeshBone) != ReducedBones.IndexOfByKey(RetainedMeshBone));
		AddInfo(FString::Printf(TEXT("Mapping recache: initially reduced=%d, bones=%d/%d, removed mesh bone=%d, retained compact=%d/%d"),
			bStartWithReducedBones, ReducedBones.Num(), FullBones.Num(), RemovedMeshBone,
			ReducedBones.IndexOfByKey(RetainedMeshBone), FullBones.IndexOfByKey(RetainedMeshBone)));

		auto VerifyHeldMapping = [&](const TArray<FBoneIndexType>& CurrentBones)
		{
			Fixture.ChangeRequiredBones(CurrentBones);
			FRecordedPose Held = Fixture.Evaluate();
			TestEqual(TEXT("Changing compact mappings cannot re-enter the graph"), Fixture.Proxy->Root.Evaluations, Evaluations);
			TestEqual(TEXT("A mapping change supplies no new update epoch"), Fixture.Proxy->GetUpdateCounter().Get(), UpdateEpoch);
			TestTrue(TEXT("A mapping change preserves sequence time"), FMath::IsNearlyEqual(Held.AssetTime, Original.AssetTime));
			TestTrue(TEXT("A mapping change cannot replay OffsetRoot motion"), Held.OffsetRotation.Equals(Original.OffsetRotation, 1.e-6));
			TestTrue(TEXT("Root-motion attributes survive mapping changes unchanged"), Held.bHasRootMotion &&
				Held.RootMotion.Equals(Original.RootMotion, 1.e-6));
			bool bKnownBonesPreserved = Held.Bones.Num() == CurrentBones.Num();
			bool bNewBonesUseLocalReference = true;
			for (int32 Index = 0; Index < CurrentBones.Num() && Held.Bones.IsValidIndex(Index); ++Index)
			{
				const int32 OriginalIndex = OriginalBones.IndexOfByKey(CurrentBones[Index]);
				if (OriginalIndex != INDEX_NONE)
				{
					bKnownBonesPreserved &= Held.Bones[Index].Equals(Original.Bones[OriginalIndex], 1.e-6);
				}
				else
				{
					bNewBonesUseLocalReference &= Held.Bones[Index].Equals(
						Fixture.Proxy->GetRequiredBones().GetRefPoseTransform(FCompactPoseBoneIndex(Index)), 1.e-6);
				}
			}
			TestTrue(TEXT("Every originally evaluated bone retains its local pose by bone identity"), bKnownBonesPreserved);
			TestTrue(TEXT("Only previously unevaluated bones use the current local reference transform"), bNewBonesUseLocalReference);
			bool bCurvesPreserved = Held.Curves.Num() == Original.Curves.Num();
			for (const auto& Curve : Original.Curves)
			{
				const auto* Value = Held.Curves.Find(Curve.Key);
				bCurvesPreserved &= Value && Value->Value == Curve.Value.Value && FMath::IsNearlyEqual(Value->Key, Curve.Value.Key, 1.e-6f);
			}
			TestTrue(TEXT("The completed curve values and flags survive a mapping change"), bCurvesPreserved);
			const FCompactPoseBoneIndex RetainedIndex(CurrentBones.IndexOfByKey(RetainedMeshBone));
			const FFloatAnimationAttribute* RetainedAttribute = Held.Attributes.Find<FFloatAnimationAttribute>(
				UE::Anim::FAttributeId(TEXT("RecacheBoneMarker"), RetainedIndex));
			TestTrue(TEXT("A non-root attribute moves to its bone's new compact index"), RetainedAttribute &&
				FMath::IsNearlyEqual(RetainedAttribute->Value, 1000.0f + RetainedMeshBone));
			const int32 AddedIndex = CurrentBones.IndexOfByKey(RemovedMeshBone);
			if (AddedIndex != INDEX_NONE)
			{
				const FFloatAnimationAttribute* AddedAttribute = Held.Attributes.Find<FFloatAnimationAttribute>(
					UE::Anim::FAttributeId(TEXT("RecacheBoneMarker"), FCompactPoseBoneIndex(AddedIndex)));
				TestTrue(TEXT("Returning attributes exist exactly when their bone was originally evaluated"), bStartWithReducedBones
					? AddedAttribute == nullptr
					: AddedAttribute && FMath::IsNearlyEqual(AddedAttribute->Value, 1000.0f + RemovedMeshBone));
			}
			FRecordedPose Repeated = Fixture.Evaluate();
			TestIdenticalPose(*this, Held, Repeated);
		};

		if (bStartWithReducedBones) { VerifyHeldMapping(FullBones); }
		VerifyHeldMapping(ReducedBones);
		VerifyHeldMapping(FullBones);
		Fixture.CompleteAutonomousMove(FFixture::StepSeconds);
		FRecordedPose Advanced = Fixture.Evaluate();
		TestEqual(TEXT("The next real move evaluates the current mapping once"), Fixture.Proxy->Root.Evaluations, Evaluations + 1);
		TestTrue(TEXT("The next real move advances sequence playback"), Advanced.AssetTime > Original.AssetTime);
		TestFalse(TEXT("The next authored delta advances OffsetRoot normally"), Advanced.OffsetRotation.Equals(Original.OffsetRotation, 1.e-4));
		const FFloatAnimationAttribute* NewlySampled = Advanced.Attributes.Find<FFloatAnimationAttribute>(
			UE::Anim::FAttributeId(TEXT("RecacheBoneMarker"), FCompactPoseBoneIndex(FullBones.IndexOfByKey(RemovedMeshBone))));
		TestTrue(TEXT("A real update samples attributes on newly required bones"), NewlySampled &&
			FMath::IsNearlyEqual(NewlySampled->Value, 1000.0f + RemovedMeshBone));
		FRecordedPose Repeated = Fixture.Evaluate();
		TestIdenticalPose(*this, Advanced, Repeated);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
