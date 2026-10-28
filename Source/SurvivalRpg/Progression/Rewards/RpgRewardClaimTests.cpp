#include "RpgRewardClaimComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "RpgRewardClaimDefinition.h"
#include "SurvivalRpg/Core/Game/RpgPlayerSaveData.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"
#include "SurvivalRpg/Runes/RpgRuneAutomationTestTypes.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RewardClaimTest_Tree, "SkillTree.Tree.AutomationReward");

namespace RpgRewardClaimTests
{
	const FName StorageId(TEXT("Storage"));

	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	/** A choice between rune A and one automation unit. */
	URpgRewardClaimDefinition* CreateChoiceReward()
	{
		URpgRewardClaimDefinition* Reward =
			NewObject<URpgRewardClaimDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Reward->DisplayName = FText::FromString(TEXT("Automation choice"));
		FRpgRewardClaimOption& Rune = Reward->Options.AddDefaulted_GetRef();
		Rune.ItemDefinition = URpgRuneAutomationTestRuneDefinition::StaticClass();
		FRpgRewardClaimOption& Unit = Reward->Options.AddDefaulted_GetRef();
		Unit.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
		return Reward;
	}

	/** A loot pool that always rolls two automation units. */
	URpgRewardClaimDefinition* CreateLootReward()
	{
		URpgLootTable* Table = NewObject<URpgLootTable>(GetTransientPackage(), NAME_None, RF_Transient);
		FRpgLootEntry& Entry = Table->Groups.AddDefaulted_GetRef().Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
		Entry.MinimumQuantity = 2;
		Entry.MaximumQuantity = 2;
		URpgRewardClaimDefinition* Reward =
			NewObject<URpgRewardClaimDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Reward->DisplayName = FText::FromString(TEXT("Automation pool"));
		Reward->LootTable = Table;
		return Reward;
	}

	FRpgInventoryGridPlacement Storage(const int32 X)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(StorageId));
		Placement.X = X;
		Placement.Y = 0;
		return Placement;
	}

	int32 CountOf(const URpgInventoryManagerComponent& Inventory, const TSubclassOf<URpgInventoryItemDefinition> Definition)
	{
		return Inventory.GetTotalItemCountByDefinition(Definition);
	}

	/** Without a controller the player inventory has no layout; a 4x1 default storage grid holds its items. */
	void SetStorage(URpgInventoryManagerComponent& Inventory)
	{
		FRpgInventoryGridSize GridSize;
		GridSize.Width = 4;
		GridSize.Height = 1;
		Inventory.SetDefaultGridSize(GridSize);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRewardClaimLifecycleTest,
	"SurvivalRpg.Progression.Rewards.ClaimsAreGrantedOnceAndSurviveAFullInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRewardClaimLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgRewardClaimTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgRewardClaimComponent* Claims = PlayerState ? PlayerState->GetRewardClaimComponent() : nullptr;
	URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (!TestNotNull(TEXT("The player has reward claims"), Claims) || !TestNotNull(TEXT("The player has an inventory"), Inventory))
	{
		return false;
	}
	SetStorage(*Inventory);
	const TSubclassOf<URpgInventoryItemDefinition> Rune = URpgRuneAutomationTestRuneDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> Unit = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	const FName ChoiceId(TEXT("Automation/Choice"));
	const FName LootId(TEXT("Automation/Loot"));

	URpgRewardClaimDefinition* Choice = CreateChoiceReward();
	TestTrue(TEXT("A milestone grants its reward"), Claims->GrantClaim(ChoiceId, Choice, RpgTradeSkillGameplayTags::Skill_Gathering_Logging, 3));
	TestFalse(TEXT("The same grant is refused while open"), Claims->GrantClaim(ChoiceId, Choice, FGameplayTag(), 0));
	TestFalse(TEXT("An invalid reward is refused"), Claims->GrantClaim(TEXT("Automation/Empty"), NewObject<URpgRewardClaimDefinition>(), FGameplayTag(), 0));
	TestEqual(TEXT("One claim is open"), Claims->GetPendingClaims().Num(), 1);
	TestEqual(TEXT("A choice that does not exist is refused"), Claims->ClaimReward(ChoiceId, 2), ERpgRewardClaimResult::InvalidOption);
	TestEqual(TEXT("An unknown claim is refused"), Claims->ClaimReward(TEXT("Automation/Unknown"), 0), ERpgRewardClaimResult::UnknownClaim);

	// A full inventory keeps the claim open and changes nothing.
	TArray<URpgInventoryItemInstance*> Fillers;
	for (int32 X = 0; X < 4; ++X)
	{
		Fillers.Add(Inventory->AddItemDefinitionToPlacement(Unit, 1, Storage(X)));
	}
	if (!TestEqual(TEXT("Four fillers fill the storage"), CountOf(*Inventory, Unit), 4))
	{
		return false;
	}
	TestEqual(TEXT("A full inventory has no room"), Claims->ClaimReward(ChoiceId, 0), ERpgRewardClaimResult::NoRoom);
	TestNotNull(TEXT("The claim stays open"), Claims->FindPendingClaim(ChoiceId));
	TestEqual(TEXT("Nothing was added"), CountOf(*Inventory, Rune), 0);

	TestTrue(TEXT("One filler is dropped"), Inventory->ConsumeItemById(Fillers[0]->GetItemId(), 1).IsSuccess());
	TestEqual(TEXT("Choosing the rune claims it"), Claims->ClaimReward(ChoiceId, 0), ERpgRewardClaimResult::Claimed);
	TestEqual(TEXT("The rune is in the inventory"), CountOf(*Inventory, Rune), 1);
	TestNull(TEXT("The claim is resolved"), Claims->FindPendingClaim(ChoiceId));
	TestTrue(TEXT("The id is remembered"), Claims->HasResolvedClaim(ChoiceId));
	TestEqual(TEXT("A resolved claim cannot be claimed again"), Claims->ClaimReward(ChoiceId, 1), ERpgRewardClaimResult::UnknownClaim);
	TestFalse(TEXT("A resolved claim cannot be granted again"), Claims->GrantClaim(ChoiceId, Choice, FGameplayTag(), 0));

	// A loot pool rolls when claimed; without room it rolls the same items on the retry.
	TestTrue(TEXT("A loot pool reward is granted"), Claims->GrantClaim(LootId, CreateLootReward(), FGameplayTag(), 0));
	TestEqual(TEXT("The full inventory has no room for the pool"), Claims->ClaimReward(LootId, 0), ERpgRewardClaimResult::NoRoom);
	TestTrue(TEXT("A second filler is dropped"), Inventory->ConsumeItemById(Fillers[1]->GetItemId(), 1).IsSuccess());
	TestTrue(TEXT("A third filler is dropped"), Inventory->ConsumeItemById(Fillers[2]->GetItemId(), 1).IsSuccess());
	TestEqual(TEXT("The pool is claimed once there is room"), Claims->ClaimReward(LootId, 0), ERpgRewardClaimResult::Claimed);
	TestEqual(TEXT("The pool gave two units"), CountOf(*Inventory, Unit), 3);

	// Saves keep open claims and resolved ids.
	TestTrue(TEXT("A third reward is granted"), Claims->GrantClaim(TEXT("Automation/Open"), Choice, FGameplayTag(), 5));
	const FRpgRewardClaimSaveData Saved = Claims->ExportSaveData();
	TestEqual(TEXT("The save holds the open claim"), Saved.PendingClaims.Num(), 1);
	TestEqual(TEXT("The save holds both resolved ids"), Saved.ResolvedClaimIds.Num(), 2);
	Claims->ResetToDefaults();
	TestTrue(TEXT("A reset forgets every claim"), Claims->GetPendingClaims().IsEmpty() && !Claims->HasResolvedClaim(ChoiceId));
	TestTrue(TEXT("The save imports"), Claims->ImportSaveData(Saved));
	const FRpgRewardClaim* Restored = Claims->FindPendingClaim(TEXT("Automation/Open"));
	TestTrue(TEXT("The open claim is back with its reward"), Restored && Restored->Reward == Choice && Restored->SourceLevel == 5);
	TestFalse(TEXT("A loaded resolved claim cannot be granted again"), Claims->GrantClaim(ChoiceId, Choice, FGameplayTag(), 0));

	FRpgRewardClaimSaveData Invalid = Saved;
	Invalid.ResolvedClaimIds.Add(TEXT("Automation/Open"));
	TestFalse(TEXT("An id both open and resolved is invalid"), URpgRewardClaimComponent::ValidateSaveData(Invalid));
	TestFalse(TEXT("Invalid claims are not imported"), Claims->ImportSaveData(Invalid));
	FRpgPlayerSaveData PlayerSave;
	PlayerSave.bHasRewardClaims = true;
	PlayerSave.RewardClaims = Invalid;
	TestFalse(TEXT("A player save with invalid claims is rejected"), PlayerSave.IsSchemaSupported());
	PlayerSave.RewardClaims = Saved;
	TestTrue(TEXT("A player save with valid claims is accepted"), PlayerSave.IsSchemaSupported());

	PlayerState->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client cannot grant"), Claims->GrantClaim(TEXT("Automation/Client"), Choice, FGameplayTag(), 0));
	TestEqual(TEXT("A client cannot claim"), Claims->ClaimReward(TEXT("Automation/Open"), 0), ERpgRewardClaimResult::Rejected);
	PlayerState->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRewardClaimMasteryMilestoneTest,
	"SurvivalRpg.Progression.Rewards.MasteryMilestonesGrantOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRewardClaimMasteryMilestoneTest::RunTest(const FString& Parameters)
{
	using namespace RpgRewardClaimTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgRewardClaimComponent* Claims = PlayerState ? PlayerState->GetRewardClaimComponent() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	URpgTradeSkillProgressionComponent* TradeSkills = PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (!TestNotNull(TEXT("The player has reward claims"), Claims) || !TestNotNull(TEXT("The player has skill trees"), SkillTrees) ||
		!TestNotNull(TEXT("The player has trade skills"), TradeSkills) || !TestNotNull(TEXT("The player has an inventory"), Inventory))
	{
		return false;
	}
	SetStorage(*Inventory);

	URpgRewardClaimDefinition* Reward = CreateChoiceReward();
	URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->TreeTag = TAG_RewardClaimTest_Tree;
	Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	for (const int32 Level : {2, 4})
	{
		FRpgSkillTreeMasteryMilestone& Milestone = Tree->MasteryMilestones.AddDefaulted_GetRef();
		Milestone.MasteryLevel = Level;
		Milestone.Reward = Reward;
	}
	TArray<FText> TreeErrors;
	TestTrue(TEXT("The milestones are valid"), Tree->ValidateTree(TreeErrors) || !TreeErrors.ContainsByPredicate([](const FText& Error)
	{
		return Error.ToString().Contains(TEXT("milestone"));
	}));
	SkillTrees->RegisterSkillTree(Tree);
	const FName LevelTwoId = Tree->GetMilestoneClaimId(2);
	const FName LevelFourId = Tree->GetMilestoneClaimId(4);
	TestEqual(TEXT("A milestone id names the tree and the level"), LevelTwoId.ToString(), FString(TEXT("SkillTree.Tree.AutomationReward/Mastery/2")));

	auto ReachLevel = [TradeSkills](const int32 Level)
	{
		for (int32 Guard = 0; Guard < 200 && TradeSkills->GetSkillLevelByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging) < Level; ++Guard)
		{
			TradeSkills->AddSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging, 50.0f);
		}
	};
	TestTrue(TEXT("Level 1 earns no milestone"), Claims->GetPendingClaims().IsEmpty());
	ReachLevel(2);
	TestNotNull(TEXT("Reaching level 2 grants its milestone"), Claims->FindPendingClaim(LevelTwoId));
	TestNull(TEXT("Level 4 is not reached yet"), Claims->FindPendingClaim(LevelFourId));
	ReachLevel(4);
	TestNotNull(TEXT("Reaching level 4 grants its milestone"), Claims->FindPendingClaim(LevelFourId));
	TestEqual(TEXT("Each milestone is granted once"), Claims->GetPendingClaims().Num(), 2);
	TestEqual(TEXT("Asking again grants nothing new"), SkillTrees->GrantReachedMasteryMilestones(), 0);

	// The progression view model lists the open rewards and claims them.
	URpgSkillProgressionViewModel* Progression = NewObject<URpgSkillProgressionViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->BindPlayerState(PlayerState);
	TestEqual(TEXT("The view lists both rewards"), Progression->GetRewardClaimCount(), 2);
	TestEqual(TEXT("Each reward offers both choices"), Progression->GetRewardChoiceCount(), 4);
	const FString RuneName =
		GetDefault<URpgInventoryItemDefinition>(URpgRuneAutomationTestRuneDefinition::StaticClass())->DisplayName.ToString();
	const FString FirstLabel = Progression->GetRewardChoiceLabel(0).ToString();
	TestTrue(TEXT("A choice names its item and source"), FirstLabel.StartsWith(RuneName + TEXT(" (")) && FirstLabel.EndsWith(TEXT(" 2)")));
	TestTrue(TEXT("The third choice is the rune of the second reward"), Progression->GetRewardChoiceLabel(2).ToString().EndsWith(TEXT(" 4)")));
	TestTrue(TEXT("No choice lies beyond the last"), Progression->GetRewardChoiceLabel(4).IsEmpty());
	Progression->ClaimRewardChoice(2);
	Progression->Refresh();
	TestEqual(TEXT("Claiming in the view resolves one reward"), Progression->GetRewardClaimCount(), 1);
	TestTrue(TEXT("The second reward was claimed"), Claims->HasResolvedClaim(LevelFourId) && !Claims->HasResolvedClaim(LevelTwoId));
	TestTrue(TEXT("A successful claim shows no feedback"), Progression->GetRewardFeedbackText().IsEmpty());
	TestEqual(TEXT("A claimed milestone is not granted again"), SkillTrees->GrantReachedMasteryMilestones(), 0);
	TestEqual(TEXT("The claimed rune is in the inventory"), CountOf(*Inventory, URpgRuneAutomationTestRuneDefinition::StaticClass()), 1);
	Progression->Unbind();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
