#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "RpgAnimationThreadingTestTypes.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopedCVar.h"
#include "UObject/SoftObjectPath.h"

namespace RpgAnimationFoundationTests
{
class FScopedAnimationWorld
{
public:
	FScopedAnimationWorld()
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

	~FScopedAnimationWorld()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRegularAnimationRemainsParallelTest,
	"SurvivalRpg.Animation.Threading.RegularAnimationRemainsParallel",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgRegularAnimationRemainsParallelTest::RunTest(const FString& Parameters)
{
	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(MeshComponent);

	TestTrue(
		TEXT("Animation outside a server autonomous move tick remains parallel"),
		AnimInstance->CanRunParallelWork());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgServerAutonomousPoseGraphDeltaTest,
	"SurvivalRpg.Animation.Threading.ServerAutonomousPoseConsumesEveryMoveDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgServerAutonomousPoseGraphDeltaTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Keep the regression sensitive to our guard even when commandlet/editor settings disable parallel work.
	FScopedCVar<int32> ParallelUpdate(TEXT("a.ParallelAnimUpdate"), 1);
	FScopedCVar<int32> ParallelEvaluation(TEXT("a.ParallelAnimEvaluation"), 1);
	FScopedCVar<int32> ForceParallel(TEXT("a.ForceParallelAnimUpdate"), 1);

	USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (!TestNotNull(TEXT("Project skeletal mesh loads"), MeshAsset))
	{
		return false;
	}

	for (const ENetMode ServerMode : { NM_ListenServer, NM_DedicatedServer })
	{
		RpgAnimationFoundationTests::FScopedAnimationWorld TestWorld;
		UWorld* World = TestWorld.World;
		if (!TestNotNull(TEXT("Animation test world exists"), World))
		{
			return false;
		}
		ARpgCharacter* Character = World->SpawnActor<ARpgCharacter>();
		if (!TestNotNull(TEXT("Project character spawned"), Character))
		{
			return false;
		}
		// Use the engine's editor net-mode seam; this does not pretend to create a real transport session.
		World->WorldType = EWorldType::PIE;
		World->SetPlayInEditorInitialNetMode(ServerMode);
		TestEqual(TEXT("World enters requested server mode"), World->GetNetMode(), ServerMode);
		Character->SetAutonomousProxy(true);
		TestEqual(TEXT("Server character represents a remote autonomous proxy"), Character->GetRemoteRole(), ROLE_AutonomousProxy);
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		Mesh->SetSkeletalMeshAsset(MeshAsset);
		Mesh->SetAnimInstanceClass(URpgAnimationThreadingTestInstance::StaticClass());
		URpgAnimationThreadingTestInstance* Anim = Cast<URpgAnimationThreadingTestInstance>(Mesh->GetAnimInstance());
		if (!TestNotNull(TEXT("Native graph fixture initializes on character mesh"), Anim))
		{
			return false;
		}
		Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
		Mesh->bOnlyAllowAutonomousTickPose = true;
		Mesh->bIsAutonomousTickPose = false;
		TestTrue(TEXT("Regular server mesh work still permits parallel update"), Anim->CanRunParallelWork());

		// Control: a regular update must remain pending, proving that global settings did not mask the bug.
		Anim->ResetGraphObservations();
		Anim->UpdateAnimation(0.01f, false);
		TestTrue(TEXT("Regular graph update is deferred under parallel settings"), Anim->NeedsUpdate());
		TestEqual(TEXT("Deferred graph has not consumed a delta yet"), Anim->GetGraphUpdateCount(), 0);
		Anim->ParallelUpdateAnimation();
		Anim->PostUpdateAnimation();
		Anim->ResetGraphObservations();

		// TickCharacterPose sets this flag before entering the mesh pose-tick path on each received move.
		Mesh->bIsAutonomousTickPose = true;
		TestTrue(TEXT("Registered character mesh permits autonomous pose ticks"), Mesh->ShouldTickPose());
		const uint64 MoveFrame = GFrameCounter;
		Mesh->TickPose(0.02f, true);
		TestEqual(TEXT("First autonomous move reaches the graph immediately"), Anim->GetGraphUpdateCount(), 1);
		TestFalse(TEXT("First move leaves no overwritten pending graph update"), Anim->NeedsUpdate());
		Mesh->TickPose(0.03f, true);
		TestEqual(TEXT("Both autonomous moves occur in one engine frame"), GFrameCounter, MoveFrame);
		TestEqual(TEXT("Both autonomous moves update the graph"), Anim->GetGraphUpdateCount(), 2);
		TestTrue(TEXT("Graph consumes the full 50 ms rather than only the last 30 ms"),
			FMath::IsNearlyEqual(Anim->GetGraphElapsedSeconds(), 0.05f));
		TestFalse(TEXT("Second move leaves no deferred graph update"), Anim->NeedsUpdate());

		Mesh->bIsAutonomousTickPose = true;
		Character->SetAutonomousProxy(false);
		TestTrue(TEXT("Server AI/simulated-owner pose work remains parallel"), Anim->CanRunParallelWork());
		Character->SetAutonomousProxy(true);
		World->SetPlayInEditorInitialNetMode(NM_Client);
		Character->SetRole(ROLE_AutonomousProxy);
		TestTrue(TEXT("Owning-client pose work remains parallel"), Anim->CanRunParallelWork());
		Mesh->bIsAutonomousTickPose = false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterAnimBlueprintParentTest,
	"SurvivalRpg.Animation.Assets.CharacterAnimBlueprintParent",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterAnimBlueprintParentTest::RunTest(const FString& Parameters)
{
	static const FSoftObjectPath AnimBlueprintPath(
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed"));
	const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(AnimBlueprintPath.TryLoad());
	if (!TestNotNull(TEXT("ABP_Unarmed loads"), AnimBlueprint))
	{
		return false;
	}

	TestTrue(
		TEXT("ABP_Unarmed derives from URpgAnimInstance"),
		AnimBlueprint->ParentClass && AnimBlueprint->ParentClass->IsChildOf(URpgAnimInstance::StaticClass()));
	TestTrue(
		TEXT("ABP_Unarmed allows multi-threaded animation update"),
		AnimBlueprint->bUseMultiThreadedAnimationUpdate);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
