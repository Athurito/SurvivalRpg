#include "Network/RpgKillXPCueNetworkTestTypes.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgHarvestCuePIETests
{
	struct FNetworkState : public FBasePIENetworkComponentState
	{
	};

	/** One GameplayCue routed on some machine, as the GameplayCue manager saw it. */
	struct FRoutedCue
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> Target;
		FGameplayTag Tag;
		EGameplayCueEvent::Type Event = EGameplayCueEvent::Executed;
		FVector Location = FVector::ZeroVector;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	ARpgNetworkAutomationKillerState* GetOwnPlayerState(const FNetworkState& State)
	{
		const APlayerController* PlayerController = State.World ? State.World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgNetworkAutomationKillerState>(PlayerController->PlayerState) : nullptr;
	}

	TArray<ARpgNetworkAutomationKillerState*> GetPlayerStates(const FNetworkState& State)
	{
		TArray<ARpgNetworkAutomationKillerState*> PlayerStates;
		if (const AGameStateBase* GameState = State.World ? State.World->GetGameState() : nullptr)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				if (ARpgNetworkAutomationKillerState* Harvester = Cast<ARpgNetworkAutomationKillerState>(PlayerState))
				{
					PlayerStates.Add(Harvester);
				}
			}
		}
		return PlayerStates;
	}

	bool HaveAllPawns(const FNetworkState& State, const int32 ExpectedPlayers)
	{
		const TArray<ARpgNetworkAutomationKillerState*> PlayerStates = GetPlayerStates(State);
		return PlayerStates.Num() == ExpectedPlayers &&
			!PlayerStates.ContainsByPredicate([](const APlayerState* PlayerState) { return !PlayerState->GetPawn(); });
	}

	bool IsServerReady(const FNetworkState& State, const int32 ExpectedClients)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_DedicatedServer || !State.World->AreActorsInitialized())
		{
			return false;
		}
		const AGameStateBase* GameState = State.World->GetGameState();
		const UNetDriver* NetDriver = State.World->GetNetDriver();
		return GameState && GameState->HasMatchStarted() && NetDriver && NetDriver->IsServer() &&
			NetDriver->ClientConnections.Num() == ExpectedClients && HaveAllPawns(State, ExpectedClients);
	}

	bool IsClientReady(const FNetworkState& State, const int32 ExpectedPlayers)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client || !State.World->AreActorsInitialized())
		{
			return false;
		}
		const AGameStateBase* GameState = State.World->GetGameState();
		const ARpgNetworkAutomationKillerState* OwnPlayerState = GetOwnPlayerState(State);
		return GameState && GameState->HasMatchStarted() && OwnPlayerState && OwnPlayerState->GetAbilitySystemComponent() &&
			HaveAllPawns(State, ExpectedPlayers);
	}

	/**
	 * Binds every player's pawn as the avatar of its player state's ability system, as PawnExtension does for
	 * characters. A player state is a simulated proxy on clients, and GAS activates nothing on a simulated avatar.
	 */
	void BindPawnAvatars(const FNetworkState& State)
	{
		for (ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
		{
			PlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(PlayerState, PlayerState->GetPawn());
		}
	}
}

NETWORK_TEST_CLASS(HarvestCuePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgHarvestCuePIETests::FNetworkState;
	using FRoutedCue = RpgHarvestCuePIETests::FRoutedCue;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	TArray<FRoutedCue> RoutedCues;
	FDelegateHandle CueRoutedHandle;
	int32 HarvesterPlayerId = INDEX_NONE;

	BEFORE_EACH()
	{
		HarvesterPlayerId = INDEX_NONE;
		RoutedCues.Reset();
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
					const FGameplayCueParameters& Parameters, EGameplayCueExecutionOptions)
				{
					RoutedCues.Add({Target ? Target->GetWorld() : nullptr, Target, Tag, Event, Parameters.Location});
				});
		}

		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(2)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(ARpgNetworkAutomationKillXPGameMode::StaticClass())
			.Build(Network);
	}

	AFTER_EACH()
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->OnGameplayCueRouted().Remove(CueRoutedHandle);
		}
	}

	/** Harvest cues routed in World on the harvester's pawn. */
	TArray<FRoutedCue> HarvesterCues(const UWorld* World) const
	{
		TArray<FRoutedCue> Cues;
		for (const FRoutedCue& Cue : RoutedCues)
		{
			const APawn* Target = Cast<APawn>(Cue.Target.Get());
			const APlayerState* TargetPlayerState = Target ? Target->GetPlayerState() : nullptr;
			if (Cue.World.Get() == World && Cue.Tag == URpgNetworkAutomationHarvestCueAbility::GetCueTag() &&
				Cue.Event == EGameplayCueEvent::Executed && TargetPlayerState &&
				TargetPlayerState->GetPlayerId() == HarvesterPlayerId)
			{
				Cues.Add(Cue);
			}
		}
		return Cues;
	}

	TEST_METHOD(HarvestCueReachesHarvesterAndPeersAfterPredictedCommit)
	{
		using namespace RpgHarvestCuePIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server has both players and their pawns"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Both clients see both player states and their pawns"),
				[](FNetworkState& State)
				{
					return IsClientReady(State, 2);
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Clients bind each player's pawn as its ability system avatar"),
				[](FNetworkState& State)
				{
					BindPawnAvatars(State);
				})
			.ThenServer(
				TEXT("The server binds the avatars and grants the harvest ability"),
				[](FNetworkState& State)
				{
					BindPawnAvatars(State);
					for (ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
					{
						PlayerState->GetAbilitySystemComponent()->GiveAbility(
							FGameplayAbilitySpec(URpgNetworkAutomationHarvestCueAbility::StaticClass()));
					}
				})
			.UntilClient(
				TEXT("The first client owns the harvest ability"),
				0,
				[](FNetworkState& State)
				{
					const ARpgNetworkAutomationKillerState* PlayerState = GetOwnPlayerState(State);
					const UAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
					return AbilitySystem &&
						AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationHarvestCueAbility::StaticClass());
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The first client predicts the harvest, which commits inside the predicted activation"),
				0,
				[this](FNetworkState& State)
				{
					ARpgNetworkAutomationKillerState* PlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					HarvesterPlayerId = PlayerState->GetPlayerId();
					UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
					const FGameplayAbilitySpec* Spec =
						AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationHarvestCueAbility::StaticClass());
					ASSERT_THAT(IsNotNull(Spec));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Spec->Handle)));
				})
			.UntilClients(
				TEXT("Every client, the predicting harvester included, plays the harvest cue once"),
				[this](FNetworkState& State)
				{
					return HarvesterCues(State.World).Num() == 1;
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("The cue plays at the harvester, the center of an area around it"),
				[this](FNetworkState& State)
				{
					const FRoutedCue Cue = HarvesterCues(State.World)[0];
					const AActor* Harvester = Cue.Target.Get();
					ASSERT_THAT(IsNotNull(Harvester));
					ASSERT_THAT(IsTrue(FVector::Dist2D(Cue.Location, Harvester->GetActorLocation()) < 5.0));
				})
			.ThenServer(
				TEXT("The dedicated server played no cue"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(HarvesterCues(State.World).IsEmpty()));
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
