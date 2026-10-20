#include "Network/RpgPortalNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

namespace RpgPortalCloseTests
{
	/** A standalone game world for portal tests without networking. */
	class FScopedStandaloneWorld
	{
	public:
		FScopedStandaloneWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedStandaloneWorld()
		{
			UWorld* WorldToDestroy = World;
			GameInstance->Shutdown();
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	ARpgNetworkAutomationPortalFixture* FindPortal(UWorld* World)
	{
		for (TActorIterator<ARpgNetworkAutomationPortalFixture> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPortalClosePresentationTest,
	"SurvivalRpg.Portals.Close.PresentsAFreshCloseOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPortalClosePresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgPortalCloseTests::FScopedStandaloneWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgNetworkAutomationPortalFixture* Portal = World ? World->SpawnActor<ARpgNetworkAutomationPortalFixture>() : nullptr;
	if (!TestNotNull(TEXT("Portal fixture exists"), Portal))
	{
		return false;
	}

	TestFalse(TEXT("An open portal cannot be closed before it is sealable"), Portal->TryClosePortal(nullptr));
	TestTrue(TEXT("The test helper makes the started outbreak sealable"), Portal->StartOutbreak());
	TestTrue(TEXT("The outbreak is sealable"), Portal->IsSealable());
	TestEqual(TEXT("Nothing is presented before the close"), Portal->GetClosedPresentations().Num(), 0);
	TestTrue(TEXT("A sealable portal closes"), Portal->TryClosePortal(nullptr));
	TestEqual(TEXT("The closing machine presents the close once"), Portal->GetClosedPresentations().Num(), 1);
	if (Portal->GetClosedPresentations().Num() == 1)
	{
		TestFalse(TEXT("The closing machine presents a fresh close, not the end state"), Portal->GetClosedPresentations()[0]);
	}
	TestFalse(TEXT("A closed portal does not close again"), Portal->TryClosePortal(nullptr));
	TestEqual(TEXT("A refused close presents nothing"), Portal->GetClosedPresentations().Num(), 1);
	TestFalse(TEXT("A closed portal cannot be made sealable again"), Portal->ForceSealableForTesting());
	return true;
}

#if ENABLE_PIE_NETWORK_TEST

namespace RpgPortalCloseTests
{
	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationPortalFixture* Portal = nullptr;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	bool IsServerReady(const FNetworkState& State, const int32 ExpectedClients)
	{
		const UNetDriver* NetDriver = IsValid(State.World) ? State.World->GetNetDriver() : nullptr;
		const AGameStateBase* GameState = IsValid(State.World) ? State.World->GetGameState() : nullptr;
		return NetDriver && NetDriver->IsServer() && State.World->GetNetMode() == NM_DedicatedServer &&
			State.World->AreActorsInitialized() && GameState && GameState->HasMatchStarted() &&
			NetDriver->ClientConnections.Num() == ExpectedClients;
	}

	bool IsClientReady(const FNetworkState& State)
	{
		const AGameStateBase* GameState = IsValid(State.World) ? State.World->GetGameState() : nullptr;
		return GameState && State.World->GetNetMode() == NM_Client && State.World->AreActorsInitialized() &&
			GameState->HasMatchStarted() && State.World->GetFirstPlayerController();
	}

	/** The portal this client received, once its state has replicated. */
	ARpgNetworkAutomationPortalFixture* FindReplicatedPortal(const FNetworkState& State, const ERpgPortalState ExpectedState)
	{
		ARpgNetworkAutomationPortalFixture* Portal = IsValid(State.World) ? FindPortal(State.World) : nullptr;
		return Portal && Portal->GetPortalState() == ExpectedState ? Portal : nullptr;
	}
}

NETWORK_TEST_CLASS(PortalClosePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgPortalCloseTests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(2)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(CloseIsPresentedOnceOnClientsAndAsEndStateOnLateJoin)
	{
		using namespace RpgPortalCloseTests;

		Network
			.UntilServer(
				TEXT("Dedicated server has both clients"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Both clients are ready"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationPortalFixture,
				&FNetworkState::Portal>(
				[this](ARpgNetworkAutomationPortalFixture& Portal)
				{
					ASSERT_THAT(IsTrue(Portal.StartOutbreak()));
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Both clients see the sealable portal"),
				[](FNetworkState& State)
				{
					return FindReplicatedPortal(State, ERpgPortalState::Sealable) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server closes the portal; a dedicated server presents nothing"),
				[this](FNetworkState& State)
				{
					ARpgNetworkAutomationPortalFixture* Portal = RpgPortalCloseTests::FindPortal(State.World);
					ASSERT_THAT(IsNotNull(Portal));
					ASSERT_THAT(IsTrue(Portal->TryClosePortal(nullptr)));
					ASSERT_THAT(IsTrue(Portal->GetClosedPresentations().IsEmpty()));
				})
			.UntilClients(
				TEXT("Every client presents the close once, as a fresh close"),
				[](FNetworkState& State)
				{
					const ARpgNetworkAutomationPortalFixture* Portal = FindReplicatedPortal(State, ERpgPortalState::Closed);
					return Portal && Portal->GetClosedPresentations() == TArray<bool>{false};
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("The late joiner is connected"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 3);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The late joiner presents the closed end state instead of the closing sequence"),
				2,
				[](FNetworkState& State)
				{
					const ARpgNetworkAutomationPortalFixture* Portal =
						IsClientReady(State) ? FindReplicatedPortal(State, ERpgPortalState::Closed) : nullptr;
					return Portal && Portal->GetClosedPresentations() == TArray<bool>{true};
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("The first two clients did not present the close again"),
				[this](FNetworkState& State)
				{
					const ARpgNetworkAutomationPortalFixture* Portal = FindReplicatedPortal(State, ERpgPortalState::Closed);
					ASSERT_THAT(IsNotNull(Portal));
					ASSERT_THAT(IsTrue(Portal->GetClosedPresentations().Num() == 1));
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
