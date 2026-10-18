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
#include "Engine/StaticMesh.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgHarvestCuePIETests
{
	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationHarvestInstancesFixture* Field = nullptr;
	};

	/** Where every machine loads the strike test's field; the server moves the harvester there. */
	const FVector StrikeFieldLocation(5000.0, 5000.0, 200.0);

	/** Two 100 cm cubes on both sides of the harvester, inside its 250 cm area, relative to StrikeFieldLocation. */
	const TArray<FVector> StrikeFieldOffsets = {FVector(-150.0, 0.0, 0.0), FVector(150.0, 0.0, 0.0)};

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

	/** Static designer profile of the strike field: four sections, one guaranteed material each, dropped if unplaced. */
	URpgHarvestProfile* MakeStrikeProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgNetworkAutomationMaterialDefinition::StaticClass();
		Entry.MinimumQuantity = 1;
		Entry.MaximumQuantity = 1;
		Entry.ChancePercent = 100.0f;
		Profile->LootTable = Table;
		Profile->SectionCount = 4;
		Profile->MinimumRespawnSeconds = 0.0f;
		Profile->MaximumRespawnSeconds = 0.0f;
		Profile->OverflowDropClass = ARpgDroppedInventoryActor::StaticClass();
		return Profile;
	}

	/** Loads the strike field on one machine, the way every machine loads a PCG partition actor. */
	ARpgNetworkAutomationHarvestInstancesFixture* LoadStrikeField(UWorld* World)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ARpgNetworkAutomationHarvestInstancesFixture* Field = World && Cube
			? World->SpawnActorDeferred<ARpgNetworkAutomationHarvestInstancesFixture>(
				ARpgNetworkAutomationHarvestInstancesFixture::StaticClass(),
				FTransform(StrikeFieldLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Field || !Field->ConfigureHarvestProfile(MakeStrikeProfile(Field)))
		{
			return nullptr;
		}
		Field->GetHarvestableInstances()->SetStaticMesh(Cube);
		for (const FVector& Offset : StrikeFieldOffsets)
		{
			Field->GetHarvestableInstances()->AddInstance(FTransform(Offset));
		}
		Field->FinishSpawning(FTransform(StrikeFieldLocation));
		return Field;
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
					RoutedCues.Add(
						{Target ? Target->GetWorld() : nullptr, Target, Tag, Event, Parameters.Location, Parameters.RawMagnitude});
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

	/** Executed cues with Tag (by default the harvest cue) routed in World on the harvester's pawn. */
	TArray<FRoutedCue> HarvesterCues(const UWorld* World, FGameplayTag Tag = FGameplayTag()) const
	{
		if (!Tag.IsValid())
		{
			Tag = URpgNetworkAutomationHarvestCueAbility::GetCueTag();
		}
		TArray<FRoutedCue> Cues;
		for (const FRoutedCue& Cue : RoutedCues)
		{
			const APawn* Target = Cast<APawn>(Cue.Target.Get());
			const APlayerState* TargetPlayerState = Target ? Target->GetPlayerState() : nullptr;
			if (Cue.World.Get() == World && Cue.Tag == Tag &&
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

	TEST_METHOD(TargetCueReachesEveryMachineOnEachHarvestedResource)
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
			.ThenServer(
				TEXT("The harvesting GameFeature adds the instance stock to the GameState"),
				[this](FNetworkState& State)
				{
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
					ASSERT_THAT(IsTrue(Stock->HasStockAuthority()));
				})
			.UntilClients(
				TEXT("Clients receive the replicated instance stock"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the strike field"),
				[this](FNetworkState& State)
				{
					State.Field = LoadStrikeField(State.World);
					ASSERT_THAT(IsNotNull(State.Field));
				})
			.ThenClients(
				TEXT("Clients load the same field and bind each player's pawn as its ability system avatar"),
				[this](FNetworkState& State)
				{
					State.Field = LoadStrikeField(State.World);
					ASSERT_THAT(IsNotNull(State.Field));
					BindPawnAvatars(State);
				})
			.ThenClient(
				TEXT("The first client is the harvester"),
				0,
				[this](FNetworkState& State)
				{
					const ARpgNetworkAutomationKillerState* PlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					HarvesterPlayerId = PlayerState->GetPlayerId();
				})
			.ThenServer(
				TEXT("The server grants the harvest ability and moves the harvester between the two cubes"),
				[this](FNetworkState& State)
				{
					BindPawnAvatars(State);
					bool bMovedHarvester = false;
					for (ARpgNetworkAutomationKillerState* PlayerState : GetPlayerStates(State))
					{
						PlayerState->GetAbilitySystemComponent()->GiveAbility(
							FGameplayAbilitySpec(URpgNetworkAutomationHarvestCueAbility::StaticClass()));
						if (PlayerState->GetPlayerId() == HarvesterPlayerId)
						{
							bMovedHarvester = PlayerState->GetPawn()->SetActorLocation(
								StrikeFieldLocation, false, nullptr, ETeleportType::TeleportPhysics);
						}
					}
					ASSERT_THAT(IsTrue(bMovedHarvester));
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
				TEXT("The first client predicts the harvest"),
				0,
				[this](FNetworkState& State)
				{
					UAbilitySystemComponent* AbilitySystem = GetOwnPlayerState(State)->GetAbilitySystemComponent();
					const FGameplayAbilitySpec* Spec =
						AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationHarvestCueAbility::StaticClass());
					ASSERT_THAT(IsNotNull(Spec));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Spec->Handle)));
				})
			.UntilClients(
				TEXT("Every client, the predicting harvester included, plays one target cue per harvested cube and the "
					"success cue of the same commit"),
				[this](FNetworkState& State)
				{
					return HarvesterCues(State.World, URpgNetworkAutomationHarvestCueAbility::GetTargetCueTag()).Num() ==
						StrikeFieldOffsets.Num() &&
						HarvesterCues(State.World).Num() == 1;
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Each target cue plays on top of its own cube and carries the cube's radius"),
				[this](FNetworkState& State)
				{
					TArray<FVector> ExpectedTops;
					for (const FVector& Offset : StrikeFieldOffsets)
					{
						ExpectedTops.Add(StrikeFieldLocation + Offset + FVector(0.0, 0.0, 50.0));
					}
					for (const FRoutedCue& Cue :
						HarvesterCues(State.World, URpgNetworkAutomationHarvestCueAbility::GetTargetCueTag()))
					{
						const int32 Matched = ExpectedTops.IndexOfByPredicate([&Cue](const FVector& Top)
						{
							return Top.Equals(Cue.Location, 1.0);
						});
						ASSERT_THAT(IsTrue(Matched != INDEX_NONE));
						ExpectedTops.RemoveAt(Matched);
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cue.RawMagnitude, 50.0f, 0.5f)));
					}
					ASSERT_THAT(IsTrue(ExpectedTops.IsEmpty()));
				})
			.ThenServer(
				TEXT("The dedicated server played no target cue, and both cubes lost stock"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(
						HarvesterCues(State.World, URpgNetworkAutomationHarvestCueAbility::GetTargetCueTag()).IsEmpty()));
					const URpgHarvestableInstancesComponent* Instances = State.Field->GetHarvestableInstances();
					ASSERT_THAT(IsTrue(Instances->GetRemainingSections(0) < 4 && Instances->GetRemainingSections(1) < 4));
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
