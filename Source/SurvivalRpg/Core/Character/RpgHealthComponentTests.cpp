#include "RpgHealthComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgHealthComponentTests
{
class FScopedHealthWorld
{
public:
	FScopedHealthWorld()
	{
		GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
		GameInstance->AddToRoot();
		GameInstance->InitializeStandalone();
		World = GameInstance->GetWorld();
	}

	~FScopedHealthWorld()
	{
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
	FRpgHealthLateAbilitySystemInitializationTest,
	"SurvivalRpg.Health.Lifecycle.DeathStateBeforeAbilitySystemInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHealthLateAbilitySystemInitializationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgHealthComponentTests::FScopedHealthWorld TestWorld;
	if (!TestNotNull(TEXT("Health lifecycle test world exists"), TestWorld.World))
	{
		return false;
	}

	for (const ERpgDeathState ReplicatedState : {
		ERpgDeathState::NotDead, ERpgDeathState::DeathStarted, ERpgDeathState::DeathFinished })
	{
		AActor* Owner = TestWorld.World->SpawnActor<AActor>();
		if (!TestNotNull(TEXT("Health owner exists"), Owner))
		{
			return false;
		}
		Owner->SetRole(ROLE_SimulatedProxy);
		URpgHealthComponent* Health = NewObject<URpgHealthComponent>(Owner);
		URpgAbilitySystemComponent* ASC = NewObject<URpgAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->AddAttributeSetSubobject(NewObject<URpgHealthSet>(Owner));
		ASC->InitAbilityActorInfo(Owner, Owner);

		// Exercise the real RepNotify ordering used when death arrives before PlayerState/PawnData.
		if (ReplicatedState != ERpgDeathState::NotDead)
		{
			Health->DeathState = ReplicatedState;
			Health->OnRep_DeathState(ERpgDeathState::NotDead);
		}
		TestEqual(TEXT("RepNotify retains death state without an ASC"), Health->GetDeathState(), ReplicatedState);

		const auto CheckTags = [this, ASC](ERpgDeathState ExpectedState)
		{
			TestEqual(TEXT("Animation death gate mirrors lifecycle"),
				ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead), ExpectedState != ERpgDeathState::NotDead);
			TestEqual(TEXT("Death parent tag mirrors lifecycle"),
				ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death), ExpectedState != ERpgDeathState::NotDead);
			TestEqual(TEXT("Dying tag exists only during death start"),
				ASC->GetTagCount(RpgGameplayTags::Status_Death_Dying), ExpectedState == ERpgDeathState::DeathStarted ? 1 : 0);
			TestEqual(TEXT("Dead tag exists only after death finish"),
				ASC->GetTagCount(RpgGameplayTags::Status_Death_Dead), ExpectedState == ERpgDeathState::DeathFinished ? 1 : 0);
		};

		// A persistent PlayerState ASC can still contain tags from its previous avatar.
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Dead, 1);
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 1);
		ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 1);
		Health->InitializeWithAbilitySystem(ASC);
		CheckTags(ReplicatedState);

		Health->UninitializeFromAbilitySystem();
		CheckTags(ERpgDeathState::NotDead);
		TestEqual(TEXT("ASC detachment preserves replicated lifecycle"), Health->GetDeathState(), ReplicatedState);
		Health->InitializeWithAbilitySystem(ASC);
		CheckTags(ReplicatedState);

		if (ReplicatedState == ERpgDeathState::DeathStarted)
		{
			Health->DeathState = ERpgDeathState::DeathFinished;
			Health->OnRep_DeathState(ERpgDeathState::DeathStarted);
			CheckTags(ERpgDeathState::DeathFinished);
		}

		Health->UninitializeFromAbilitySystem();
		URpgHealthComponent* RespawnHealth = NewObject<URpgHealthComponent>(Owner);
		RespawnHealth->InitializeWithAbilitySystem(ASC);
		CheckTags(ERpgDeathState::NotDead);
		RespawnHealth->UninitializeFromAbilitySystem();
		ASC->ClearActorInfo();
		ASC->UnregisterComponent();
	}
	return true;
}

#endif
