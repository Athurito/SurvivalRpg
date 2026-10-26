#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgAIAttackDriverComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
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

namespace RpgAIAttackDriverTests
{
	/** Delivers a health drop to the driver's health handler as the owner's health component would on the server. */
	void ReportDamage(URpgAIAttackDriverComponent& Driver, URpgHealthComponent* OwnerHealth, AActor* DamageInstigator)
	{
		struct FHealthChangedParams
		{
			URpgHealthComponent* HealthComponent = nullptr;
			float OldValue = 0.0f;
			float NewValue = 0.0f;
			AActor* Instigator = nullptr;
		};
		FHealthChangedParams Params{ OwnerHealth, 100.0f, 90.0f, DamageInstigator };
		Driver.ProcessEvent(Driver.FindFunctionChecked(TEXT("HandleHealthChanged")), &Params);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAIAttackDriverExternalControlTest,
	"SurvivalRpg.AI.AttackDriver.ExternalControlStopsTheTimersAndKeepsTheLastAttackerAfterTheCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAIAttackDriverExternalControlTest::RunTest(const FString& Parameters)
{
	using namespace RpgAIAttackDriverTests;

	FScopedTimerWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Test world exists"), World))
	{
		return false;
	}

	ARpgCombatNetworkTargetFixture* Attacker = World->SpawnActor<ARpgCombatNetworkTargetFixture>();
	ARpgCombatNetworkTargetFixture* FirstTarget = World->SpawnActor<ARpgCombatNetworkTargetFixture>(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator);
	ARpgCombatNetworkTargetFixture* SecondTarget = World->SpawnActor<ARpgCombatNetworkTargetFixture>(FVector(0.0, 100.0, 0.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Attacker exists"), Attacker) || !TestNotNull(TEXT("First target exists"), FirstTarget) ||
		!TestNotNull(TEXT("Second target exists"), SecondTarget))
	{
		return false;
	}
	Attacker->SetGenericTeamId(FGenericTeamId(1));

	URpgAIAttackDriverComponent* Driver = NewObject<URpgAIAttackDriverComponent>(Attacker);
	Driver->RegisterComponent();
	TestWorld.Advance(0.0f);

	Driver->BeginExternalControl(ERpgAICombatTargetPolicy::LastAttacker, 4.0f);
	Driver->SetCombatTarget(FirstTarget);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		TestWorld.Advance(1.0f / 60.0f);
	}
	TestEqual(TEXT("Under external control the timers attempt no attack"), static_cast<int32>(Driver->GetAttackAttemptCountForTests()), 0);

	ReportDamage(*Driver, Attacker->GetHealthComponent(), SecondTarget);
	TestEqual(TEXT("A hit inside the switch cooldown keeps the target"), Driver->GetCombatTarget(), static_cast<AActor*>(FirstTarget));

	for (int32 Frame = 0; Frame < 4 * 60; ++Frame)
	{
		TestWorld.Advance(1.0f / 60.0f);
	}
	ReportDamage(*Driver, Attacker->GetHealthComponent(), SecondTarget);
	TestEqual(TEXT("After the cooldown the last attacker becomes the target"), Driver->GetCombatTarget(), static_cast<AActor*>(SecondTarget));

	TestEqual(TEXT("A target within the lose radius is kept"),
		Driver->RefreshCombatTarget(3000.0f, 4000.0f), static_cast<AActor*>(SecondTarget));
	TestNull(TEXT("A target beyond the lose radius is dropped"), Driver->RefreshCombatTarget(3000.0f, 50.0f));

	Driver->EndExternalControl();
	TestFalse(TEXT("Ending external control returns to the timers"), Driver->IsExternallyControlled());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAIAttackDriverTargetAttackCountTest,
	"SurvivalRpg.AI.AttackDriver.CountsOnlyTheTargetsAttackActivations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAIAttackDriverTargetAttackCountTest::RunTest(const FString& Parameters)
{
	using namespace RpgAIAttackDriverTests;

	// Stable asset contract: the shared weapon attack is an attack for enemy AI, the block is not.
	const UClass* AttackClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/GF_Combat_Core/GAS/Abilities/GA_BasicWeaponAttack.GA_BasicWeaponAttack_C"));
	const UClass* BlockClass = LoadClass<UGameplayAbility>(nullptr, TEXT("/GF_Combat_Core/GAS/Abilities/GA_Combat_Block.GA_Combat_Block_C"));
	if (!TestNotNull(TEXT("GA_BasicWeaponAttack loads"), AttackClass) || !TestNotNull(TEXT("GA_Combat_Block loads"), BlockClass))
	{
		return false;
	}

	FScopedTimerWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Test world exists"), World))
	{
		return false;
	}

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
	Driver->BeginExternalControl(ERpgAICombatTargetPolicy::Nearest, 0.0f);
	Driver->SetCombatTarget(Target);

	UAbilitySystemComponent* TargetAbilitySystem = Target->GetRpgAbilitySystemComponent();
	TargetAbilitySystem->AbilityActivatedCallbacks.Broadcast(AttackClass->GetDefaultObject<UGameplayAbility>());
	TargetAbilitySystem->AbilityActivatedCallbacks.Broadcast(BlockClass->GetDefaultObject<UGameplayAbility>());
	TestEqual(TEXT("Only the attack activation counts"), static_cast<int32>(Driver->GetTargetAttackCount()), 1);

	Driver->ClearCombatTarget();
	TargetAbilitySystem->AbilityActivatedCallbacks.Broadcast(AttackClass->GetDefaultObject<UGameplayAbility>());
	TestEqual(TEXT("A former target's attacks no longer count"), static_cast<int32>(Driver->GetTargetAttackCount()), 1);
	return !HasAnyErrors();
}

#endif
