#include "Network/RpgSkillTreeNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgSkillTreePIETests
{
	// Editor modules cannot define native tags; these come from the runtime module's skill tree automation tests.
	FGameplayTag TreeTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.AutomationTest"));
	}

	FGameplayTag RootTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.AutomationTest.Root"));
	}

	FGameplayTag ExpensiveTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.AutomationTest.Passive"));
	}

	struct FNetworkState : public FBasePIENetworkComponentState
	{
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	/** Root costs one point; Expensive costs three, more than the players earn. */
	URpgSkillTreeDefinition* MakeTree(UObject* Outer)
	{
		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(Outer);
		Tree->TreeTag = TreeTag();
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
		Tree->MaxPoints = 5;

		FRpgSkillTreeNode& Root = Tree->Nodes.AddDefaulted_GetRef();
		Root.NodeTag = RootTag();

		FRpgSkillTreeNode& Expensive = Tree->Nodes.AddDefaulted_GetRef();
		Expensive.NodeTag = ExpensiveTag();
		Expensive.Column = 1;
		Expensive.Cost = 3;
		return Tree;
	}

	ARpgPlayerState* GetOwnPlayerState(const FNetworkState& State)
	{
		const APlayerController* PlayerController = State.World ? State.World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgPlayerState>(PlayerController->PlayerState) : nullptr;
	}

	TArray<ARpgPlayerState*> GetPlayerStates(const FNetworkState& State)
	{
		TArray<ARpgPlayerState*> PlayerStates;
		if (const AGameStateBase* GameState = State.World ? State.World->GetGameState() : nullptr)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				if (ARpgPlayerState* RpgPlayerState = Cast<ARpgPlayerState>(PlayerState))
				{
					PlayerStates.Add(RpgPlayerState);
				}
			}
		}
		return PlayerStates;
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
			NetDriver->ClientConnections.Num() == ExpectedClients &&
			GetPlayerStates(State).Num() == ExpectedClients;
	}

	bool IsClientReady(const FNetworkState& State, const int32 ExpectedPlayers)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client || !State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const ARpgPlayerState* OwnPlayerState = GetOwnPlayerState(State);
		return GameState && GameState->HasMatchStarted() && OwnPlayerState &&
			OwnPlayerState->GetSkillTreeComponent() && GetPlayerStates(State).Num() == ExpectedPlayers;
	}
}

NETWORK_TEST_CLASS(SkillTreePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgSkillTreePIETests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	int32 OwnerPlayerId = INDEX_NONE;

	BEFORE_EACH()
	{
		OwnerPlayerId = INDEX_NONE;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(2)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(ARpgNetworkAutomationProgressionGameMode::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(ClientRequestsAreValidatedAndReachOnlyTheOwner)
	{
		using namespace RpgSkillTreePIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server has both players"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Both clients see both RPG player states"),
				[](FNetworkState& State)
				{
					return IsClientReady(State, 2);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Every player earns two Logging points and the server knows the tree"),
				[this](FNetworkState& State)
				{
					for (ARpgPlayerState* PlayerState : GetPlayerStates(State))
					{
						URpgTradeSkillProgressionComponent* TradeSkills = PlayerState->GetTradeSkillProgressionComponent();
						URpgSkillTreeComponent* SkillTrees = PlayerState->GetSkillTreeComponent();
						ASSERT_THAT(IsNotNull(TradeSkills));
						ASSERT_THAT(IsNotNull(SkillTrees));
						const FGameplayTag Logging = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
						for (int32 Guard = 0; Guard < 10 && TradeSkills->GetSkillLevelByTag(Logging) < 3; ++Guard)
						{
							TradeSkills->AddSkillXPByTag(
								Logging,
								TradeSkills->GetXPToNextLevelByTag(Logging) - TradeSkills->GetSkillXPByTag(Logging));
						}
						SkillTrees->RegisterSkillTree(MakeTree(PlayerState));
						ASSERT_THAT(AreEqual(SkillTrees->GetAvailablePoints(TreeTag()), 2));
					}
				})
			.ThenClient(
				TEXT("The first client requests the root and an unaffordable node"),
				0,
				[this](FNetworkState& State)
				{
					ARpgPlayerState* PlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					URpgSkillTreeComponent* SkillTrees = PlayerState->GetSkillTreeComponent();
					ASSERT_THAT(IsNotNull(SkillTrees));
					OwnerPlayerId = PlayerState->GetPlayerId();

					// A client cannot change its progress locally; only the validated server request counts.
					ASSERT_THAT(IsTrue(SkillTrees->UnlockNode(TreeTag(), RootTag()) !=
						ERpgSkillTreeUnlockResult::Unlockable));
					ASSERT_THAT(IsFalse(SkillTrees->IsNodeUnlocked(TreeTag(), RootTag())));
					SkillTrees->RequestUnlockNode(TreeTag(), RootTag());
					SkillTrees->RequestUnlockNode(TreeTag(), ExpensiveTag());
				})
			.UntilServer(
				TEXT("The server learned only the affordable root, for exactly one player"),
				[](FNetworkState& State)
				{
					int32 RootCount = 0;
					for (const ARpgPlayerState* PlayerState : GetPlayerStates(State))
					{
						const URpgSkillTreeComponent* SkillTrees = PlayerState->GetSkillTreeComponent();
						if (SkillTrees->IsNodeUnlocked(TreeTag(), ExpensiveTag()))
						{
							return false;
						}
						RootCount += SkillTrees->IsNodeUnlocked(TreeTag(), RootTag()) ? 1 : 0;
					}
					return RootCount == 1;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The owner receives its learned root"),
				0,
				[](FNetworkState& State)
				{
					const ARpgPlayerState* PlayerState = GetOwnPlayerState(State);
					return PlayerState &&
						PlayerState->GetSkillTreeComponent()->IsNodeUnlocked(TreeTag(), RootTag());
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The owner never receives the rejected node"),
				0,
				[this](FNetworkState& State)
				{
					const ARpgPlayerState* PlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					ASSERT_THAT(IsFalse(PlayerState->GetSkillTreeComponent()->IsNodeUnlocked(
						TreeTag(), ExpensiveTag())));
				})
			.ThenServer(
				TEXT("The owner's state replicated after the request"),
				[this](FNetworkState& State)
				{
					// Gives the other client a full network round in which an owner-only leak would arrive.
					ASSERT_THAT(AreEqual(GetPlayerStates(State).Num(), 2));
				})
			.ThenClient(
				TEXT("The other client never receives the first player's progress"),
				1,
				[this](FNetworkState& State)
				{
					const ARpgPlayerState* OwnPlayerState = GetOwnPlayerState(State);
					ASSERT_THAT(IsNotNull(OwnPlayerState));
					ASSERT_THAT(IsTrue(OwnPlayerState->GetSkillTreeComponent()->GetTreeStates().IsEmpty()));

					const ARpgPlayerState* OwnerOnThisClient = nullptr;
					for (const ARpgPlayerState* PlayerState : GetPlayerStates(State))
					{
						if (PlayerState->GetPlayerId() == OwnerPlayerId)
						{
							OwnerOnThisClient = PlayerState;
						}
					}
					ASSERT_THAT(IsNotNull(OwnerOnThisClient));
					ASSERT_THAT(IsTrue(OwnerOnThisClient->GetSkillTreeComponent()->GetTreeStates().IsEmpty()));
				});
	}
};

#endif
#endif
