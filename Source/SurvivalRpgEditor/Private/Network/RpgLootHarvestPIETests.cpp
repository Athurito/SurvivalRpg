#include "Network/RpgLootHarvestNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "AbilitySystemComponent.h"
#include "Components/PIENetworkComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFeaturesSubsystem.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgGatheringSet.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgLootHarvestPIETests
{
	constexpr int32 LootQuantity = 1;
	constexpr int32 FirstOverflowQuantity = 3;
	constexpr int32 SecondOverflowQuantity = 2;
	constexpr int32 HarvestExperience = 10;

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationLootFixture* LootFixture = nullptr;
		ARpgNetworkAutomationHarvesterState* Harvester = nullptr;
		ARpgNetworkAutomationHarvestFixture* HarvestFixture = nullptr;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	URpgLootTable* MakeGuaranteedLootTable(
		UObject* Outer,
		const TArray<TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>>& Rewards)
	{
		URpgLootTable* Table = NewObject<URpgLootTable>(Outer);
		if (!Table)
		{
			return nullptr;
		}

		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Reward : Rewards)
		{
			FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
			Entry.ItemDefinition = Reward.Key;
			Entry.MinimumQuantity = Reward.Value;
			Entry.MaximumQuantity = Reward.Value;
			Entry.ChancePercent = 100.0f;
		}
		return Table;
	}

	URpgHarvestProfile* MakeOverflowHarvestProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		if (!Profile)
		{
			return nullptr;
		}

		Profile->LootTable = MakeGuaranteedLootTable(
			Profile,
			{
				TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
					URpgNetworkAutomationMaterialDefinition::StaticClass(),
					FirstOverflowQuantity),
				TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
					URpgNetworkAutomationSecondMaterialDefinition::StaticClass(),
					SecondOverflowQuantity)
			});
		Profile->SkillTag =
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = HarvestExperience;
		Profile->MinimumRespawnSeconds = 0.0f;
		Profile->MaximumRespawnSeconds = 0.0f;
		Profile->OverflowDropClass = ARpgDroppedInventoryActor::StaticClass();
		return Profile;
	}

	bool IsServerReady(const FNetworkState& State, const int32 ExpectedClients)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_DedicatedServer ||
			!State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const UNetDriver* NetDriver = State.World->GetNetDriver();
		if (!GameState || !GameState->HasMatchStarted() || !NetDriver ||
			!NetDriver->IsServer() ||
			NetDriver->ClientConnections.Num() != ExpectedClients)
		{
			return false;
		}

		for (const UNetConnection* Connection : NetDriver->ClientConnections)
		{
			if (!IsValid(Connection) || !IsValid(Connection->ViewTarget))
			{
				return false;
			}
		}
		return true;
	}

	bool IsClientReady(const FNetworkState& State)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client ||
			!State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const APlayerController* PlayerController =
			State.World->GetFirstPlayerController();
		return GameState && GameState->HasMatchStarted() && PlayerController;
	}

	int32 CountWorldDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	ARpgDroppedInventoryActor* FindOnlyWorldDrop(UWorld* World)
	{
		ARpgDroppedInventoryActor* Result = nullptr;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (Result)
			{
				return nullptr;
			}
			Result = *It;
		}
		return Result;
	}

	bool HasCompleteOverflowDrop(UWorld* World)
	{
		ARpgDroppedInventoryActor* Drop = FindOnlyWorldDrop(World);
		URpgInventoryManagerComponent* DropInventory =
			Drop ? Drop->GetLootInventoryManager() : nullptr;
		return Drop && Drop->IsLootInventoryCanonical() && DropInventory &&
			DropInventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationMaterialDefinition::StaticClass()) ==
				FirstOverflowQuantity &&
			DropInventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationSecondMaterialDefinition::StaticClass()) ==
				SecondOverflowQuantity;
	}

	FRpgHarvestRequest MakeHarvestRequest(
		ARpgNetworkAutomationHarvestFixture* HarvestFixture,
		AActor* Harvester)
	{
		FRpgHarvestRequest Request;
		URpgHarvestableInstancedMeshComponent* Component = HarvestFixture
			? HarvestFixture->GetHarvestableInstances()
			: nullptr;
		FTransform InstanceTransform = FTransform::Identity;
		if (Component)
		{
			Component->GetInstanceTransform(0, InstanceTransform, true);
		}

		Request.Harvester = Harvester;
		Request.AbilityId =
			RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.TraceOrigin = Harvester
			? Harvester->GetActorLocation()
			: FVector::ZeroVector;
		Request.Hit = FHitResult(
			HarvestFixture,
			Component,
			InstanceTransform.GetLocation(),
			FVector::UpVector);
		Request.Hit.Item = 0;
		Request.ExpectedRevision = Component
			? Component->GetResourceInstanceRevision(0)
			: INDEX_NONE;
		Request.HarvestPower = 1.0f;
		return Request;
	}

	bool HasReplicatedLoot(
		const FNetworkState& State,
		const FRpgInventoryItemId& ExpectedItemId)
	{
		if (!IsValid(State.LootFixture) || State.LootFixture->HasAuthority())
		{
			return false;
		}

		const URpgInventoryManagerComponent* Inventory =
			State.LootFixture->GetInventory();
		if (!Inventory ||
			Inventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationMaterialDefinition::StaticClass()) !=
				LootQuantity)
		{
			return false;
		}

		const TArray<URpgInventoryItemInstance*> Items = Inventory->GetAllItems();
		return Items.Num() == 1 && IsValid(Items[0]) &&
			Items[0]->GetItemId() == ExpectedItemId;
	}

	bool HasReplicatedHarvestState(const FNetworkState& State)
	{
		const URpgHarvestableInstancedMeshComponent* Component =
			IsValid(State.HarvestFixture)
			? State.HarvestFixture->GetHarvestableInstances()
			: nullptr;
		return Component && !State.HarvestFixture->HasAuthority() &&
			Component->GetInstanceCount() == 1 &&
			!Component->IsResourceInstanceActive(0) &&
			Component->GetResourceInstanceRevision(0) == 1;
	}

	int32 CountGatheringSets(const FNetworkState& State)
	{
		const UAbilitySystemComponent* AbilitySystem = IsValid(State.Harvester)
			? State.Harvester->GetAbilitySystemComponent()
			: nullptr;
		if (!AbilitySystem)
		{
			return 0;
		}

		int32 Count = 0;
		for (const UAttributeSet* AttributeSet : AbilitySystem->GetSpawnedAttributes())
		{
			if (IsValid(AttributeSet) && AttributeSet->IsA<URpgGatheringSet>())
			{
				++Count;
			}
		}
		return Count;
	}
}

NETWORK_TEST_CLASS(LootHarvestPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgLootHarvestPIETests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FRpgInventoryItemId LootItemId;
	FString HarvestingPluginURL;
	bool bPluginTransitionComplete = false;
	bool bPluginTransitionSucceeded = false;

	BEFORE_EACH()
	{
		HarvestingPluginURL.Reset();
		bPluginTransitionComplete = false;
		bPluginTransitionSucceeded = false;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(ServerAuthorityOverflowReplicationAndLateJoin)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server match and first connection are ready"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client owns a ready pawn"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationLootFixture,
				&FNetworkState::LootFixture>(
				[this](ARpgNetworkAutomationLootFixture& Fixture)
				{
					URpgLootTable* Table = MakeGuaranteedLootTable(
						&Fixture,
						{
							TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
								URpgNetworkAutomationMaterialDefinition::StaticClass(),
								LootQuantity)
						});
					if (!Table || !Fixture.GetLootSource())
					{
						TestRunner->AddError(
							TEXT("Failed to configure the transient loot-source fixture."));
						return;
					}
					Fixture.GetLootSource()->ConfigureLootTable(Table);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Client cannot populate authoritative loot"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.LootFixture));
					ASSERT_THAT(IsNotNull(State.LootFixture->GetInventory()));
					ASSERT_THAT(IsNotNull(State.LootFixture->GetLootSource()));
					ASSERT_THAT(IsTrue(!State.LootFixture->HasAuthority()));
					State.LootFixture->GetLootSource()->PopulateLoot();
					ASSERT_THAT(AreEqual(
						State.LootFixture->GetInventory()
							->GetTotalItemCountByDefinition(
								URpgNetworkAutomationMaterialDefinition::StaticClass()),
						0));
				})
			.ThenServer(
				TEXT("Server populates the loot source exactly once"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.LootFixture));
					ASSERT_THAT(IsTrue(State.LootFixture->HasAuthority()));
					URpgInventoryManagerComponent* Inventory =
						State.LootFixture->GetInventory();
					URpgNetworkAutomationLootSourceComponent* LootSource =
						State.LootFixture->GetLootSource();
					ASSERT_THAT(IsNotNull(Inventory));
					ASSERT_THAT(IsNotNull(LootSource));

					LootSource->PopulateLoot();
					LootSource->PopulateLoot();
					ASSERT_THAT(AreEqual(
						Inventory->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						LootQuantity));
					const TArray<URpgInventoryItemInstance*> Items =
						Inventory->GetAllItems();
					ASSERT_THAT(AreEqual(Items.Num(), 1));
					ASSERT_THAT(IsNotNull(Items[0]));
					LootItemId = Items[0]->GetItemId();
					ASSERT_THAT(IsTrue(LootItemId.IsValid()));
				})
			.UntilClient(
				TEXT("Initial client receives the concrete loot item"),
				0,
				[this](FNetworkState& State)
				{
					return HasReplicatedLoot(State, LootItemId);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[this](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					URpgInventoryManagerComponent* Inventory =
						Harvester.GetInventoryManagerComponent();
					if (!Inventory)
					{
						TestRunner->AddError(
							TEXT("Harvester fixture has no inventory manager."));
						return;
					}
					Inventory->SetFixedMaxEntries(0);
					Inventory->SetCapacityMode(
						ERpgInventoryCapacityMode::FixedEntries);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvestFixture,
				&FNetworkState::HarvestFixture>(
				[this](ARpgNetworkAutomationHarvestFixture& Fixture)
				{
					URpgHarvestProfile* Profile =
						MakeOverflowHarvestProfile(&Fixture);
					if (!Profile || !Fixture.ConfigureHarvestProfile(Profile))
					{
						TestRunner->AddError(
							TEXT("Failed to configure the transient harvest profile."));
					}
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Client cannot commit the harvest"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestFixture));
					ASSERT_THAT(IsNotNull(State.Harvester));
					URpgHarvestableInstancedMeshComponent* Component =
						State.HarvestFixture->GetHarvestableInstances();
					ASSERT_THAT(IsNotNull(Component));
					const FRpgHarvestRequest Request = MakeHarvestRequest(
						State.HarvestFixture,
						State.Harvester);
					ASSERT_THAT(IsTrue(
						!Component->CommitHarvest_Implementation(Request)));
					ASSERT_THAT(IsTrue(Component->IsResourceInstanceActive(0)));
					ASSERT_THAT(AreEqual(
						Component->GetResourceInstanceRevision(0),
						0));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
				})
			.ThenServer(
				TEXT("Server harvest atomically creates one complete overflow drop"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestFixture));
					ASSERT_THAT(IsNotNull(State.Harvester));
					URpgHarvestableInstancedMeshComponent* Component =
						State.HarvestFixture->GetHarvestableInstances();
					URpgInventoryManagerComponent* HarvesterInventory =
						State.Harvester->GetInventoryManagerComponent();
					URpgTradeSkillProgressionComponent* TradeSkills =
						State.Harvester->GetTradeSkillProgressionComponent();
					ASSERT_THAT(IsNotNull(Component));
					ASSERT_THAT(IsNotNull(HarvesterInventory));
					ASSERT_THAT(IsNotNull(TradeSkills));

					const FRpgHarvestRequest Request = MakeHarvestRequest(
						State.HarvestFixture,
						State.Harvester);
					ASSERT_THAT(IsTrue(
						Component->CanAcceptHarvest_Implementation(Request)));
					ASSERT_THAT(IsTrue(
						Component->CommitHarvest_Implementation(Request)));
					ASSERT_THAT(IsTrue(
						!Component->IsResourceInstanceActive(0)));
					ASSERT_THAT(AreEqual(
						Component->GetResourceInstanceRevision(0),
						1));
					ASSERT_THAT(AreEqual(
						HarvesterInventory->GetUsedEntryCount(),
						0));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						TradeSkills->GetSkillXPByTag(
							RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
						static_cast<float>(HarvestExperience))));

					ASSERT_THAT(IsTrue(
						!Component->CommitHarvest_Implementation(Request)));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						TradeSkills->GetSkillXPByTag(
							RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
						static_cast<float>(HarvestExperience))));
				})
			.UntilClient(
				TEXT("Initial client receives depleted HISM and complete overflow"),
				0,
				[](FNetworkState& State)
				{
					return HasReplicatedHarvestState(State) &&
						HasCompleteOverflowDrop(State.World);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second server connection"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late-joining PIE client owns a ready pawn"),
				1,
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late join reconstructs loot, harvest state, and overflow"),
				1,
				[this](FNetworkState& State)
				{
					return HasReplicatedLoot(State, LootItemId) &&
						HasReplicatedHarvestState(State) &&
						HasCompleteOverflowDrop(State.World);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Late join never re-rolls or re-grants rewards"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(IsServerReady(State, 2)));
					ASSERT_THAT(AreEqual(
						State.LootFixture->GetInventory()
							->GetTotalItemCountByDefinition(
								URpgNetworkAutomationMaterialDefinition::StaticClass()),
						LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
				})
			.ThenClients(
				TEXT("Both clients expose identical replicated terminal state"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(IsClientReady(State)));
					ASSERT_THAT(IsTrue(
						HasReplicatedLoot(State, LootItemId)));
					ASSERT_THAT(IsTrue(HasReplicatedHarvestState(State)));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
				});
	}

	TEST_METHOD(GatheringSetGameFeatureReactivationIsIdempotent)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and initial client are ready for GameFeature lifecycle"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("PIE client is ready for GameFeature lifecycle"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Activate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.Harvester));
					UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					if (!Subsystem.GetPluginURLByName(
							TEXT("GF_Harvesting_Magic"),
							HarvestingPluginURL))
					{
						TestRunner->AddError(
							TEXT("Could not resolve GF_Harvesting_Magic plugin URL."));
						bPluginTransitionComplete = true;
						return;
					}
					Subsystem.LoadAndActivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginLoadComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature activation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Duplicate readiness events do not duplicate GatheringSet"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(bPluginTransitionSucceeded));
					ASSERT_THAT(IsNotNull(State.Harvester));
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
				})
			.UntilServer(
				TEXT("Server owns exactly one GatheringSet after activation"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client receives exactly one replicated GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Deactivate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginDeactivateComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature deactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Server removes every GatheringSet on deactivation"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionSucceeded && CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client removes the replicated GatheringSet on deactivation"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Reactivate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginLoadComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature reactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Reactivated feature receives the normal ability-ready event"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(bPluginTransitionSucceeded));
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
				})
			.UntilServer(
				TEXT("Reactivation grants exactly one server GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Reactivation replicates exactly one client GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cleanup deactivates the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginDeactivateComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Cleanup deactivation completes without residual server grants"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete && bPluginTransitionSucceeded &&
						CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Cleanup leaves no replicated GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 0;
				},
				NetworkTimeout());
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
