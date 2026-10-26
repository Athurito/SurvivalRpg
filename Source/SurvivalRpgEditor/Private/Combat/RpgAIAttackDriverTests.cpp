#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/Combat/RpgAIAttackDriverComponent.h"
#include "TimerManager.h"

namespace RpgAIAttackDriverTests
{
	/** Standalone game world whose timers the test advances by hand. */
	class FScopedTimerWorld
	{
	public:
		FScopedTimerWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTimerWorld()
		{
			UWorld* WorldToDestroy = World;
			GameInstance->Shutdown();
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
			GFrameCounter = CachedFrameCounter;
		}

		UWorld* GetWorld() const { return World; }

		/** One frame: the timer manager ticks once per frame counter. */
		void Advance(const float DeltaSeconds) const
		{
			World->TimeSeconds += DeltaSeconds;
			World->UnpausedTimeSeconds += DeltaSeconds;
			World->RealTimeSeconds += DeltaSeconds;
			++GFrameCounter;
			World->GetTimerManager().Tick(DeltaSeconds);
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAIAttackDriverConfirmedTargetTest,
	"SurvivalRpg.AI.AttackDriver.ConfirmingTheTargetEveryFrameStillAttacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAIAttackDriverConfirmedTargetTest::RunTest(const FString& Parameters)
{
	using namespace RpgAIAttackDriverTests;

	FScopedTimerWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Test world exists"), World))
	{
		return false;
	}

	// Two team agents with health: the hostile target fixture and an attacker of another team.
	ARpgCombatNetworkTargetFixture* Attacker = World->SpawnActor<ARpgCombatNetworkTargetFixture>();
	ARpgCombatNetworkTargetFixture* Target = World->SpawnActor<ARpgCombatNetworkTargetFixture>(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Attacker exists"), Attacker) || !TestNotNull(TEXT("Target exists"), Target))
	{
		return false;
	}
	Attacker->SetGenericTeamId(FGenericTeamId(1));

	URpgAIAttackDriverComponent* Driver = NewObject<URpgAIAttackDriverComponent>(Attacker);
	Driver->RegisterComponent();
	TestWorld.Advance(0.0f);

	Driver->SetCombatTarget(Target);
	TestEqual(TEXT("The hostile target is accepted"), Driver->GetCombatTarget(), static_cast<AActor*>(Target));

	// The StateTree target task confirms its target every tick, here at 60 frames per second for one second.
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		TestWorld.Advance(1.0f / 60.0f);
		Driver->SetCombatTarget(Target);
	}

	TestTrue(TEXT("The driver attempts an attack although the target is confirmed every frame"),
		Driver->GetAttackAttemptCountForTests() > 0);
	return !HasAnyErrors();
}

#endif
