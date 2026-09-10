// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgAnimInstanceTestTypes.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopedCVar.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgAnimInstanceTests
{
	class FScopedAnimationWorld
	{
	public:
		FScopedAnimationWorld()
		{
			const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &InitializationValues);
			if (!World)
			{
				return;
			}

			// Only the net-mode fallback is needed. No GameMode, Experience, map load,
			// InitializeActorsForPlay, BeginPlay, or save lifecycle participates in this test.
			World->SetPlayInEditorInitialNetMode(NM_ListenServer);
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			Character = World->SpawnActor<ARpgAnimInstanceTestCharacter>(SpawnParameters);
			if (!Character)
			{
				return;
			}

			Character->SetAutonomousProxy(true);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Mesh = Character->GetMesh();
			Mesh->bOnlyAllowAutonomousTickPose = true;
			Mesh->bIsAutonomousTickPose = true;
			Instance.Reset(NewObject<URpgAnimInstanceTestInstance>(Mesh, NAME_None, RF_Transient));
			Instance->bUseMultiThreadedAnimationUpdate = true;
			Instance->RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
			Instance->InitializeAnimation();
		}

		~FScopedAnimationWorld()
		{
			if (Instance.IsValid())
			{
				FlushPendingGraphUpdate();
				Instance->UninitializeAnimation();
				Instance.Reset();
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		void FlushPendingGraphUpdate() const
		{
			if (Instance.IsValid() && Instance->NeedsUpdate())
			{
				Instance->ParallelUpdateAnimation();
				Instance->PostUpdateAnimation();
			}
		}

		UWorld* World = nullptr;
		ARpgAnimInstanceTestCharacter* Character = nullptr;
		USkeletalMeshComponent* Mesh = nullptr;
		TStrongObjectPtr<URpgAnimInstanceTestInstance> Instance;
	};

	class FScopedAnimationCVar
	{
	public:
		FScopedAnimationCVar(const TCHAR* Name, int32 Value)
			: Variable(IConsoleManager::Get().FindConsoleVariable(Name))
		{
			if (!Variable)
			{
				return;
			}

			// The engine's temporary override preserves lower-priority history, including
			// constructor defaults. An existing console value has higher priority than
			// SetByTemp, so replace and restore that entry in place, retaining its tag.
			bHadConsoleOverride = (Variable->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsole;
			if (bHadConsoleOverride)
			{
				OriginalConsoleValue = Variable->GetInt();
				Variable->ReplaceCurrentPriorityAndTag(Value);
			}
			else
			{
				TemporaryValue = MakeUnique<FScopedCVar<int32>>(Name, Value, TEXT("RpgAnimInstanceTests"));
			}
		}

		~FScopedAnimationCVar()
		{
			if (bHadConsoleOverride)
			{
				Variable->ReplaceCurrentPriorityAndTag(OriginalConsoleValue);
			}
		}

		bool HasValue(int32 ExpectedValue) const
		{
			return Variable && Variable->GetInt() == ExpectedValue;
		}

	private:
		IConsoleVariable* Variable;
		TUniquePtr<FScopedCVar<int32>> TemporaryValue;
		int32 OriginalConsoleValue = 0;
		bool bHadConsoleOverride = false;
	};

	class FScopedParallelAnimationSettings
	{
	public:
		FScopedParallelAnimationSettings()
			: ParallelUpdate(TEXT("a.ParallelAnimUpdate"), 1)
			, ParallelEvaluation(TEXT("a.ParallelAnimEvaluation"), 1)
			, ForceParallelUpdate(TEXT("a.ForceParallelAnimUpdate"), 0)
			, OriginalAllowMultithreading(GetDefault<UEngine>()->bAllowMultiThreadedAnimationUpdate)
		{
			GetMutableDefault<UEngine>()->bAllowMultiThreadedAnimationUpdate = true;
		}

		~FScopedParallelAnimationSettings()
		{
			GetMutableDefault<UEngine>()->bAllowMultiThreadedAnimationUpdate = OriginalAllowMultithreading;
		}

		bool IsConfigured() const
		{
			return ParallelUpdate.HasValue(1) && ParallelEvaluation.HasValue(1) && ForceParallelUpdate.HasValue(0);
		}

	private:
		FScopedAnimationCVar ParallelUpdate;
		FScopedAnimationCVar ParallelEvaluation;
		FScopedAnimationCVar ForceParallelUpdate;
		bool OriginalAllowMultithreading;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRemoteAutonomousAnimationDeltasTest,
	"SurvivalRpg.Animation.ListenServer.PreservesAutonomousMoveGraphDeltas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRemoteAutonomousAnimationDeltasTest::RunTest(const FString& Parameters)
{
	using namespace RpgAnimInstanceTests;
	FScopedParallelAnimationSettings Settings;
	FScopedAnimationWorld Fixture;
	if (!TestTrue(TEXT("Parallel animation is enabled for the regression"), Settings.IsConfigured()) ||
		!TestNotNull(TEXT("Transient native anim instance exists"), Fixture.Instance.Get()))
	{
		return false;
	}
	TestNull(TEXT("Fixture never creates a game mode"), Fixture.World->GetAuthGameMode());
	TestFalse(TEXT("Fixture never begins gameplay"), Fixture.World->HasBegunPlay());
	TestEqual(TEXT("Fixture represents a listen server"), Fixture.World->GetNetMode(), NM_ListenServer);
	TestEqual(TEXT("Remote player is authoritative here"), Fixture.Character->GetLocalRole(), ROLE_Authority);
	TestEqual(TEXT("Peer owns the autonomous role"), Fixture.Character->GetRemoteRole(), ROLE_AutonomousProxy);

	// The identical native proxy with the ordinary UAnimInstance policy reproduces
	// the lost first delta without any animation Blueprint or migrated content.
	{
		FScopedAnimationWorld EngineControl;
		if (!TestNotNull(TEXT("Engine baseline control exists"), EngineControl.Instance.Get()))
		{
			return false;
		}
		EngineControl.Instance->bUseRpgTimingGuard = false;
		const uint64 ControlFrame = GFrameCounter;
		EngineControl.Instance->UpdateAnimation(0.01f, true);
		EngineControl.Instance->UpdateAnimation(0.02f, true);
		TestTrue(TEXT("Engine baseline defers autonomous graph updates"), EngineControl.Instance->NeedsUpdate());
		EngineControl.FlushPendingGraphUpdate();
		TestEqual(TEXT("Control moves share one frame"), GFrameCounter, ControlFrame);
		TestEqual(TEXT("Engine baseline consumes only one graph update"), EngineControl.Instance->GetGraphUpdateCount(), 1);
		TestTrue(TEXT("Engine baseline retains only the final 20 milliseconds"),
			FMath::IsNearlyEqual(EngineControl.Instance->GetGraphElapsedSeconds(), 0.02f, KINDA_SMALL_NUMBER));
	}

	const uint64 StartingFrame = GFrameCounter;
	Fixture.Instance->UpdateAnimation(0.01f, true);
	Fixture.Instance->UpdateAnimation(0.02f, true);
	Fixture.FlushPendingGraphUpdate();
	TestEqual(TEXT("Both movement deltas were submitted during the same frame"), GFrameCounter, StartingFrame);
	TestEqual(TEXT("Each autonomous move advances the graph"), Fixture.Instance->GetGraphUpdateCount(), 2);
	TestTrue(TEXT("Graph consumes the full 30 milliseconds instead of only the last move"),
		FMath::IsNearlyEqual(Fixture.Instance->GetGraphElapsedSeconds(), 0.03f, KINDA_SMALL_NUMBER));

	// Prove this test can reach the deferred engine branch, rather than accidentally
	// passing because parallel animation was disabled globally or by the fixture.
	Fixture.Mesh->bIsAutonomousTickPose = false;
	Fixture.Instance->UpdateAnimation(0.04f, false);
	TestTrue(TEXT("Ordinary updates retain deferred parallel eligibility"), Fixture.Instance->NeedsUpdate());
	TestEqual(TEXT("Ordinary graph update remains pending"), Fixture.Instance->GetGraphUpdateCount(), 2);
	Fixture.FlushPendingGraphUpdate();
	TestEqual(TEXT("Deferred graph executes once when completed"), Fixture.Instance->GetGraphUpdateCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgRemoteAutonomousAnimationScopeTest,
	"SurvivalRpg.Animation.ListenServer.ScopesImmediateUpdateToRemoteAutonomousPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRemoteAutonomousAnimationScopeTest::RunTest(const FString& Parameters)
{
	using namespace RpgAnimInstanceTests;
	FScopedAnimationWorld Fixture;
	if (!TestNotNull(TEXT("Transient native anim instance exists"), Fixture.Instance.Get()))
	{
		return false;
	}

	TestFalse(TEXT("Remote autonomous pose uses immediate graph update"), Fixture.Instance->CanRunParallelWork());
	Fixture.Mesh->bIsAutonomousTickPose = false;
	TestTrue(TEXT("Outside the movement-owned pose scope parallel work is retained"), Fixture.Instance->CanRunParallelWork());
	Fixture.Mesh->bIsAutonomousTickPose = true;
	Fixture.Mesh->bOnlyAllowAutonomousTickPose = false;
	TestTrue(TEXT("Ordinary mesh ticking retains parallel work"), Fixture.Instance->CanRunParallelWork());
	Fixture.Mesh->bOnlyAllowAutonomousTickPose = true;

	Fixture.Character->SetAutonomousProxy(false);
	TestTrue(TEXT("Host and AI without remote autonomous ownership retain parallel work"), Fixture.Instance->CanRunParallelWork());
	Fixture.Character->SetAutonomousProxy(true);
	Fixture.Character->SetTestLocalRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("Non-authoritative representations retain parallel work"), Fixture.Instance->CanRunParallelWork());
	Fixture.Character->SetTestLocalRole(ROLE_Authority);

	for (const ENetMode NetMode : {NM_Client, NM_DedicatedServer, NM_Standalone})
	{
		Fixture.World->SetPlayInEditorInitialNetMode(NetMode);
		TestTrue(FString::Printf(TEXT("Net mode %d retains parallel work"), static_cast<int32>(NetMode)),
			Fixture.Instance->CanRunParallelWork());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgUnarmedAnimationBaseContractTest,
	"SurvivalRpg.Animation.ListenServer.UnarmedUsesNativeAnimationFoundation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUnarmedAnimationBaseContractTest::RunTest(const FString& Parameters)
{
	const UClass* UnarmedClass = LoadClass<UAnimInstance>(nullptr,
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (!TestNotNull(TEXT("Unarmed animation Blueprint class loads"), UnarmedClass))
	{
		return false;
	}
	TestTrue(TEXT("Player animation inherits the native networking seam"),
		UnarmedClass->IsChildOf(URpgAnimInstance::StaticClass()));
	return true;
}

#endif
