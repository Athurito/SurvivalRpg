#include "Network/RpgKillXPCueNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AwardKillXP.h"
#include "SurvivalRpg/Progression/Player/Data/RpgPlayerProgressionData.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "UObject/StrongObjectPtr.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgKillXPCuePIETests
{
	constexpr TCHAR ProgressionFeatureDataPath[] = TEXT("/GF_Progression/GF_Progression.GF_Progression");
	const FVector VictimLocation(400.0f, -150.0f, 120.0f);

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
		float RawMagnitude = 0.0f;
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
				if (ARpgNetworkAutomationKillerState* Killer = Cast<ARpgNetworkAutomationKillerState>(PlayerState))
				{
					PlayerStates.Add(Killer);
				}
			}
		}
		return PlayerStates;
	}

	ARpgNetworkAutomationKillerState* FindPlayerState(const FNetworkState& State, const int32 PlayerId)
	{
		for (ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
		{
			if (PlayerState->GetPlayerId() == PlayerId)
			{
				return PlayerState;
			}
		}
		return nullptr;
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
			NetDriver->ClientConnections.Num() == ExpectedClients && GetPlayerStates(State).Num() == ExpectedClients;
	}

	bool IsClientReady(const FNetworkState& State, const int32 ExpectedPlayers)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client || !State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const ARpgNetworkAutomationKillerState* OwnPlayerState = GetOwnPlayerState(State);
		const TArray<ARpgNetworkAutomationKillerState*> PlayerStates = GetPlayerStates(State);
		return GameState && GameState->HasMatchStarted() && OwnPlayerState &&
			OwnPlayerState->GetAbilitySystemComponent() && PlayerStates.Num() == ExpectedPlayers &&
			!PlayerStates.ContainsByPredicate([](const APlayerState* PlayerState) { return !PlayerState->GetPawn(); });
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

	/** The configured kill XP action of GF_Progression, the source of the cue tag the test expects. */
	const URpgGameFeatureAction_AwardKillXP* FindProgressionKillXPAction()
	{
		const UGameFeatureData* FeatureData = LoadObject<UGameFeatureData>(nullptr, ProgressionFeatureDataPath);
		if (!FeatureData)
		{
			return nullptr;
		}

		for (const UGameFeatureAction* Action : FeatureData->GetActions())
		{
			if (const URpgGameFeatureAction_AwardKillXP* KillXP = Cast<URpgGameFeatureAction_AwardKillXP>(Action))
			{
				return KillXP;
			}
		}
		return nullptr;
	}
}

NETWORK_TEST_CLASS(KillXPCuePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgKillXPCuePIETests::FNetworkState;
	using FRoutedCue = RpgKillXPCuePIETests::FRoutedCue;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	TStrongObjectPtr<URpgGameFeatureAction_AwardKillXP> KillXPAction;
	FGameFeatureActivatingContext KillXPContext;
	FGameplayTag CueTag;
	TArray<FRoutedCue> RoutedCues;
	FDelegateHandle CueRoutedHandle;
	int32 KillerPlayerId = INDEX_NONE;
	float AwardedXP = 0.0f;

	BEFORE_EACH()
	{
		KillerPlayerId = INDEX_NONE;
		AwardedXP = 0.0f;
		RoutedCues.Reset();
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
					const FGameplayCueParameters& Parameters, EGameplayCueExecutionOptions)
				{
					RoutedCues.Add({Target ? Target->GetWorld() : nullptr, Target, Tag, Event, Parameters.Location,
						Parameters.RawMagnitude});
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
		if (KillXPAction)
		{
			FGameFeatureDeactivatingContext Deactivating(TEXT("KillXPCuePIE"), [](FStringView) {});
			static_cast<FGameFeatureStateChangeContext&>(Deactivating) = KillXPContext;
			KillXPAction->OnGameFeatureDeactivating(Deactivating);
			KillXPAction.Reset();
		}
	}

	int32 CountKillerCues(const UWorld* World) const
	{
		int32 Count = 0;
		for (const FRoutedCue& Cue : RoutedCues)
		{
			const APawn* Target = Cast<APawn>(Cue.Target.Get());
			const APlayerState* TargetPlayerState = Target ? Target->GetPlayerState() : nullptr;
			if (Cue.World.Get() == World && Cue.Tag == CueTag && Cue.Event == EGameplayCueEvent::Executed &&
				TargetPlayerState && TargetPlayerState->GetPlayerId() == KillerPlayerId)
			{
				++Count;
			}
		}
		return Count;
	}

	TEST_METHOD(SoulHarvestCueReachesKillerAndPeersAfterPredictedKill)
	{
		using namespace RpgKillXPCuePIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server has both players and their pawns"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2) &&
						!GetPlayerStates(State).ContainsByPredicate([](const APlayerState* PlayerState) { return !PlayerState->GetPawn(); });
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
				TEXT("The server runs GF_Progression's kill XP action, spawns a victim and grants the kill ability"),
				[this](FNetworkState& State)
				{
					const URpgGameFeatureAction_AwardKillXP* ConfiguredAction = FindProgressionKillXPAction();
					ASSERT_THAT(IsNotNull(ConfiguredAction));
					CueTag = ConfiguredAction->XPAwardGameplayCueTag;
					ASSERT_THAT(IsTrue(CueTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue")))));

					// The plain test game mode loads no Experience. Unless GF_Progression is already active, a copy of
					// its action listens in the server world only.
					if (!UGameFeaturesSubsystem::Get().IsGameFeaturePluginActiveByName(TEXT("GF_Progression")))
					{
						const FWorldContext* ServerContext = GEngine->GetWorldContextFromWorld(State.World);
						ASSERT_THAT(IsNotNull(ServerContext));
						KillXPContext.SetRequiredWorldContextHandle(ServerContext->ContextHandle);
						KillXPAction.Reset(DuplicateObject(ConfiguredAction, GetTransientPackage()));
						KillXPAction->OnGameFeatureActivating(KillXPContext);
					}

					BindPawnAvatars(State);

					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					ASSERT_THAT(IsNotNull(State.World->SpawnActor<ARpgNetworkAutomationKillVictim>(
						VictimLocation, FRotator::ZeroRotator, SpawnParameters)));

					for (ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
					{
						URpgPlayerProgressionComponent* Progression = PlayerState->GetPlayerProgressionComponent();
						ASSERT_THAT(IsNotNull(Progression));
						// Without a level curve the XP only accumulates, which keeps the award readable.
						Progression->ConfigData = NewObject<URpgPlayerProgressionData>(PlayerState);
						ASSERT_THAT(IsTrue(Progression->GetXP() == 0.0f));
						PlayerState->GetAbilitySystemComponent()->GiveAbility(
							FGameplayAbilitySpec(URpgNetworkAutomationKillAbility::StaticClass()));
					}
				})
			.UntilClient(
				TEXT("The first client owns the kill ability"),
				0,
				[](FNetworkState& State)
				{
					const ARpgNetworkAutomationKillerState* PlayerState = GetOwnPlayerState(State);
					const UAbilitySystemComponent* AbilitySystem = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
					return AbilitySystem &&
						AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationKillAbility::StaticClass());
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The first client predicts the kill ability"),
				0,
				[this](FNetworkState& State)
				{
					ARpgNetworkAutomationKillerState* PlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					KillerPlayerId = PlayerState->GetPlayerId();
					UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
					const FGameplayAbilitySpec* Spec =
						AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationKillAbility::StaticClass());
					ASSERT_THAT(IsNotNull(Spec));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Spec->Handle)));
				})
			.UntilServer(
				TEXT("The server awarded the kill to the first client"),
				[this](FNetworkState& State)
				{
					const ARpgNetworkAutomationKillerState* Killer = FindPlayerState(State, KillerPlayerId);
					AwardedXP = Killer ? Killer->GetPlayerProgressionComponent()->GetXP() : 0.0f;
					return AwardedXP > 0.0f;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Only the killer earned XP, and the dedicated server played no cue"),
				[this](FNetworkState& State)
				{
					for (const ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
					{
						const float XP = PlayerState->GetPlayerProgressionComponent()->GetXP();
						ASSERT_THAT(IsTrue(PlayerState->GetPlayerId() == KillerPlayerId ? XP == AwardedXP : XP == 0.0f));
					}
					ASSERT_THAT(IsTrue(CountKillerCues(State.World) == 0));
				})
			.UntilClients(
				TEXT("Every client, the predicting killer included, plays the cue"),
				[this](FNetworkState& State)
				{
					return CountKillerCues(State.World) == 1;
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("The cue carries the victim location and the awarded XP"),
				[this](FNetworkState& State)
				{
					for (const FRoutedCue& Cue : RoutedCues)
					{
						if (Cue.World.Get() == State.World && Cue.Tag == CueTag)
						{
							ASSERT_THAT(IsTrue(Cue.Location.Equals(VictimLocation, 0.1)));
							ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cue.RawMagnitude, AwardedXP)));
						}
					}
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
