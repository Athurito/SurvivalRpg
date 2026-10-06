#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NativeGameplayTags.h"
#include "RpgSkillTreeAutomationTestTypes.h"
#include "RpgSkillTreeComponent.h"
#include "RpgSkillTreeDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Game/RpgPlayerSaveData.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/Progression/Skills/Data/RpgTradeSkillConfigData.h"
#include "TimerManager.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Tree, "SkillTree.Tree.AutomationTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_UnregisteredTree, "SkillTree.Tree.AutomationTestUnregistered");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Root, "SkillTree.Node.AutomationTest.Root");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Second, "SkillTree.Node.AutomationTest.Second");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormA, "SkillTree.Node.AutomationTest.FormA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormB, "SkillTree.Node.AutomationTest.FormB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Wide, "SkillTree.Node.AutomationTest.Wide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Passive, "SkillTree.Node.AutomationTest.Passive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Removed, "SkillTree.Node.AutomationTest.Removed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Granted, "SkillTree.AutomationTest.Granted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Alpha, "Ability.AutomationSkillTree.Alpha");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Beta, "Ability.AutomationSkillTree.Beta");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Gamma, "Ability.AutomationSkillTree.Gamma");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Radius, "Ability.Tuning.AutomationTest.Radius");

namespace RpgSkillTreeTests
{
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

	/** Restores the class-default fragment of the test tool when a test ends. */
	struct FScopedTestToolTree
	{
		explicit FScopedTestToolTree(const URpgSkillTreeDefinition* Tree)
		{
			URpgSkillTreeAutomationTestToolDefinition::SetTestSkillTree(Tree);
		}

		~FScopedTestToolTree()
		{
			URpgSkillTreeAutomationTestToolDefinition::SetTestSkillTree(nullptr);
		}
	};

	URpgAbilitySet* MakeAbilitySet(const FGameplayTag AbilityId)
	{
		URpgAbilitySet* AbilitySet = NewObject<URpgAbilitySet>(GetTransientPackage(), NAME_None, RF_Transient);
		AbilitySet->AddGrantedGameplayAbility(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, FGameplayTag(), AbilityId);
		return AbilitySet;
	}

	FRpgSkillTreeAbilityTuning MakeTuning(
		const FGameplayTag AbilityId,
		const ERpgSkillTreeTuningOperation Operation,
		const float Value)
	{
		FRpgSkillTreeAbilityTuning Tuning;
		Tuning.AbilityIdTag = AbilityId;
		Tuning.TuningTag = TAG_SkillTreeTest_Radius;
		Tuning.Operation = Operation;
		Tuning.Value = Value;
		return Tuning;
	}

	/**
	 * Root and Second grant the abilities Alpha and Beta. FormA and FormB need Root and two spent points and exclude
	 * each other. Wide adds to every ability, Passive (two points) multiplies Alpha. Points come from Logging.
	 */
	URpgSkillTreeDefinition* MakeTree()
	{
		URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Tree->TreeTag = TAG_SkillTreeTest_Tree;
		Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
		Tree->MaxPoints = 10;

		auto AddNode = [Tree](const FGameplayTag NodeTag, const int32 Row, const int32 Column) -> FRpgSkillTreeNode&
		{
			FRpgSkillTreeNode& Node = Tree->Nodes.AddDefaulted_GetRef();
			Node.NodeTag = NodeTag;
			Node.Row = Row;
			Node.Column = Column;
			return Node;
		};

		FRpgSkillTreeNode& Root = AddNode(TAG_SkillTreeTest_Root, 0, 0);
		Root.AbilitySet = MakeAbilitySet(TAG_SkillTreeTest_Alpha);
		Root.GrantedTags.AddTag(TAG_SkillTreeTest_Granted);

		AddNode(TAG_SkillTreeTest_Second, 0, 1).AbilitySet = MakeAbilitySet(TAG_SkillTreeTest_Beta);

		FRpgSkillTreeNode& FormA = AddNode(TAG_SkillTreeTest_FormA, 1, 0);
		FormA.Prerequisites.Add(TAG_SkillTreeTest_Root);
		FormA.RequiredPointsInTree = 2;
		FormA.ExclusiveGroup = TEXT("Form");
		FormA.AbilityTunings.Add(MakeTuning(TAG_SkillTreeTest_Alpha, ERpgSkillTreeTuningOperation::Set, 900.0f));

		FRpgSkillTreeNode& FormB = AddNode(TAG_SkillTreeTest_FormB, 1, 1);
		FormB.Prerequisites.Add(TAG_SkillTreeTest_Root);
		FormB.RequiredPointsInTree = 2;
		FormB.ExclusiveGroup = TEXT("Form");
		FormB.AbilityTunings.Add(MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Set, 700.0f));

		AddNode(TAG_SkillTreeTest_Wide, 2, 0).AbilityTunings.Add(
			MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 100.0f));

		FRpgSkillTreeNode& Passive = AddNode(TAG_SkillTreeTest_Passive, 2, 1);
		Passive.Cost = 2;
		Passive.AbilityTunings.Add(MakeTuning(TAG_SkillTreeTest_Alpha, ERpgSkillTreeTuningOperation::Multiply, 1.5f));
		return Tree;
	}

	void RaiseSkillLevel(URpgTradeSkillProgressionComponent& TradeSkills, const FGameplayTag SkillTag, const int32 Level)
	{
		for (int32 Guard = 0; Guard < 200 && TradeSkills.GetSkillLevelByTag(SkillTag) < Level; ++Guard)
		{
			const float MissingXP = TradeSkills.GetXPToNextLevelByTag(SkillTag) - TradeSkills.GetSkillXPByTag(SkillTag);
			TradeSkills.AddSkillXPByTag(SkillTag, FMath::Max(1.0f, MissingXP));
		}
	}

	void RaiseLogging(ARpgPlayerState& PlayerState, const int32 Level)
	{
		RaiseSkillLevel(
			*PlayerState.GetTradeSkillProgressionComponent(),
			RpgTradeSkillGameplayTags::Skill_Gathering_Logging,
			Level);
	}

	URpgInventoryItemInstance* MakeToolItem(UObject* Outer)
	{
		URpgInventoryItemInstance* Item = NewObject<URpgInventoryItemInstance>(Outer);
		FObjectPropertyBase* ItemDefinitionProperty = FindFProperty<FObjectPropertyBase>(
			URpgInventoryItemInstance::StaticClass(),
			TEXT("ItemDef"));
		if (!Item || !ItemDefinitionProperty)
		{
			return nullptr;
		}

		ItemDefinitionProperty->SetObjectPropertyValue_InContainer(
			Item,
			URpgSkillTreeAutomationTestToolDefinition::StaticClass());
		return Item;
	}

	const FGameplayAbilitySpec* FindSpecWithId(const URpgAbilitySystemComponent& AbilitySystem, const FGameplayTag AbilityId)
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityId))
			{
				return &Spec;
			}
		}
		return nullptr;
	}

	/** Player state with real trade skills and skill trees, and a GAS pawn with the real equipment manager. */
	struct FEquipmentFixture
	{
		ARpgInventoryAutomationTestPlayerState* PlayerState = nullptr;
		URpgSkillTreeComponent* SkillTrees = nullptr;
		ARpgEquipmentAutomationTestPawn* Pawn = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		URpgEquipmentManagerComponent* Equipment = nullptr;
		URpgInventoryItemInstance* Tool = nullptr;
	};

	bool MakeEquipmentFixture(FAutomationTestBase& Test, UWorld* World, FEquipmentFixture& Out)
	{
		Out.PlayerState = World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
		Out.Pawn = World ? World->SpawnActor<ARpgEquipmentAutomationTestPawn>() : nullptr;
		if (!Test.TestNotNull(TEXT("Player state spawns"), Out.PlayerState) ||
			!Test.TestNotNull(TEXT("Pawn spawns"), Out.Pawn))
		{
			return false;
		}

		Out.SkillTrees = Out.PlayerState->GetSkillTreeComponent();
		Out.AbilitySystem = Out.Pawn->GetRpgAbilitySystemComponent();
		Out.Equipment = Out.Pawn->GetEquipmentManagerComponent();
		Out.Tool = MakeToolItem(Out.PlayerState);
		if (!Test.TestNotNull(TEXT("Player state owns skill trees"), Out.SkillTrees) ||
			!Test.TestNotNull(TEXT("Pawn owns an ability system"), Out.AbilitySystem) ||
			!Test.TestNotNull(TEXT("Pawn owns an equipment manager"), Out.Equipment) ||
			!Test.TestNotNull(TEXT("Tool item exists"), Out.Tool))
		{
			return false;
		}

		// The standalone automation world does not run the pawn/PlayerState lifecycle; mirror it explicitly.
		Out.Pawn->SetPlayerState(Out.PlayerState);
		Out.AbilitySystem->InitAbilityActorInfo(Out.Pawn, Out.Pawn);
		Out.AbilitySystem->SetForceGrantAuthorityForTests(true);
		return Test.TestTrue(TEXT("Player state knows its pawn"), Out.PlayerState->GetPawn() == Out.Pawn);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeDefinitionValidationTest,
	"SurvivalRpg.Progression.SkillTrees.Definition.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeDefinitionValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	TArray<FText> Errors;
	TestTrue(TEXT("The reference tree is valid"), MakeTree()->ValidateTree(Errors));
	for (const FText& Error : Errors)
	{
		AddError(Error.ToString());
	}

	auto ExpectInvalid = [this](const TCHAR* What, URpgSkillTreeDefinition* Tree)
	{
		TArray<FText> CaseErrors;
		TestFalse(What, Tree->ValidateTree(CaseErrors));
		TestTrue(FString::Printf(TEXT("%s reports a reason"), What), !CaseErrors.IsEmpty());
	};

	URpgSkillTreeDefinition* MissingTreeTag = MakeTree();
	MissingTreeTag->TreeTag = FGameplayTag();
	ExpectInvalid(TEXT("A tree without TreeTag is rejected"), MissingTreeTag);

	URpgSkillTreeDefinition* DuplicateNode = MakeTree();
	FRpgSkillTreeNode Copy = DuplicateNode->Nodes[0];
	Copy.Row = 5;
	DuplicateNode->Nodes.Add(Copy);
	ExpectInvalid(TEXT("A duplicated node tag is rejected"), DuplicateNode);

	URpgSkillTreeDefinition* SharedPosition = MakeTree();
	SharedPosition->Nodes[1].Column = 0;
	ExpectInvalid(TEXT("Two nodes in one grid cell are rejected"), SharedPosition);

	URpgSkillTreeDefinition* MissingPrerequisite = MakeTree();
	MissingPrerequisite->Nodes[4].Prerequisites.Add(TAG_SkillTreeTest_Removed);
	ExpectInvalid(TEXT("A prerequisite outside the tree is rejected"), MissingPrerequisite);

	URpgSkillTreeDefinition* Cycle = MakeTree();
	Cycle->Nodes[0].Prerequisites.Add(TAG_SkillTreeTest_FormA);
	ExpectInvalid(TEXT("A prerequisite cycle is rejected"), Cycle);

	URpgSkillTreeDefinition* ExclusivePrerequisite = MakeTree();
	ExclusivePrerequisite->Nodes[3].Prerequisites.Add(TAG_SkillTreeTest_FormA);
	ExpectInvalid(TEXT("A prerequisite from the own exclusive group is rejected"), ExclusivePrerequisite);

	URpgSkillTreeDefinition* UnreachableGate = MakeTree();
	UnreachableGate->Nodes[5].RequiredPointsInTree = 7;
	ExpectInvalid(TEXT("A point gate the other nodes cannot reach is rejected"), UnreachableGate);

	URpgSkillTreeDefinition* InvalidTuning = MakeTree();
	InvalidTuning->Nodes[4].AbilityTunings[0].TuningTag = FGameplayTag();
	ExpectInvalid(TEXT("A tuning without TuningTag is rejected"), InvalidTuning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreePointsTest,
	"SurvivalRpg.Progression.SkillTrees.PointsFollowMasteryLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreePointsTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	URpgSkillTreeDefinition* Tree = MakeTree();
	TestEqual(TEXT("An unregistered tree earns nothing"), SkillTrees->GetEarnedPoints(TAG_SkillTreeTest_Tree), 0);
	SkillTrees->RegisterSkillTree(Tree);
	TestEqual(TEXT("Level 1 earns no point"), SkillTrees->GetEarnedPoints(TAG_SkillTreeTest_Tree), 0);

	RaiseLogging(*PlayerState, 4);
	TestEqual(TEXT("Level 4 earns one point per level above 1"), SkillTrees->GetEarnedPoints(TAG_SkillTreeTest_Tree), 3);
	TestEqual(TEXT("Nothing is spent yet"), SkillTrees->GetAvailablePoints(TAG_SkillTreeTest_Tree), 3);

	Tree->MaxPoints = 2;
	TestEqual(TEXT("MaxPoints caps the earned points"), SkillTrees->GetEarnedPoints(TAG_SkillTreeTest_Tree), 2);

	Tree->MaxPoints = 10;
	UCurveFloat* Curve = NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient);
	Curve->FloatCurve.AddKey(1.0f, 2.0f);
	Curve->FloatCurve.AddKey(4.0f, 8.0f);
	Tree->PointsByLevel = Curve;
	TestEqual(TEXT("An authored curve replaces the default points"), SkillTrees->GetEarnedPoints(TAG_SkillTreeTest_Tree), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeUnlockRulesTest,
	"SurvivalRpg.Progression.SkillTrees.UnlockRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeUnlockRulesTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	TestEqual(TEXT("An unregistered tree is unknown"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeUnlockResult::UnknownTree);

	SkillTrees->RegisterSkillTree(MakeTree());
	TestEqual(TEXT("Level 1 has no point for the root"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeUnlockResult::NotEnoughPoints);
	SkillTrees->RequestUnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	TestFalse(TEXT("A rejected request changes nothing"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Root));

	RaiseLogging(*PlayerState, 2);
	TestEqual(TEXT("Level 2 affords the root"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("The root is learned"),
		SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("The root reports as learned"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Root));
	TestEqual(TEXT("The root spent the only point"), SkillTrees->GetAvailablePoints(TreeTag), 0);
	TestEqual(TEXT("A learned node cannot be learned again"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeUnlockResult::AlreadyUnlocked);
	TestEqual(TEXT("A node outside the tree is unknown"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Removed), ERpgSkillTreeUnlockResult::UnknownNode);
	TestEqual(TEXT("A form needs two points spent in the tree"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_FormA), ERpgSkillTreeUnlockResult::TreePointsRequired);

	RaiseLogging(*PlayerState, 4);
	SkillTrees->RequestUnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	TestTrue(TEXT("A valid request learns the node"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Second));
	TestEqual(TEXT("Two spent points open the form row"),
		SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_FormA), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("The other form of the group is excluded"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeUnlockResult::ExclusiveConflict);
	TestEqual(TEXT("No point is left for the passive"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Passive), ERpgSkillTreeUnlockResult::NotEnoughPoints);

	TestTrue(TEXT("The tree resets"), SkillTrees->ResetTree(TreeTag));
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Wide);
	TestEqual(TEXT("A form still needs its learned prerequisite"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_FormA), ERpgSkillTreeUnlockResult::MissingPrerequisite);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeResetAndSlotsTest,
	"SurvivalRpg.Progression.SkillTrees.ResetAndSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeResetAndSlotsTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	SkillTrees->RegisterSkillTree(MakeTree());
	RaiseLogging(*PlayerState, 4);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	TestEqual(TEXT("The first learned ability takes Q"), SkillTrees->GetSlotAbilityId(TreeTag, 0), TAG_SkillTreeTest_Alpha.GetTag());
	TestEqual(TEXT("The second learned ability takes E"), SkillTrees->GetSlotAbilityId(TreeTag, 1), TAG_SkillTreeTest_Beta.GetTag());
	TestFalse(TEXT("R stays free"), SkillTrees->GetSlotAbilityId(TreeTag, 2).IsValid());

	TestTrue(TEXT("Alpha moves to R"), SkillTrees->AssignSlot(TreeTag, 2, TAG_SkillTreeTest_Alpha));
	TestFalse(TEXT("Q takes R's previous, empty occupant"), SkillTrees->GetSlotAbilityId(TreeTag, 0).IsValid());
	TestTrue(TEXT("Alpha swaps with Beta"), SkillTrees->AssignSlot(TreeTag, 1, TAG_SkillTreeTest_Alpha));
	TestEqual(TEXT("E holds Alpha"), SkillTrees->GetSlotAbilityId(TreeTag, 1), TAG_SkillTreeTest_Alpha.GetTag());
	TestEqual(TEXT("R holds Beta"), SkillTrees->GetSlotAbilityId(TreeTag, 2), TAG_SkillTreeTest_Beta.GetTag());
	TestFalse(TEXT("An ability of no learned node is rejected"), SkillTrees->AssignSlot(TreeTag, 0, TAG_SkillTreeTest_Gamma));
	TestFalse(TEXT("A slot outside Q/E/R is rejected"), SkillTrees->AssignSlot(TreeTag, 3, TAG_SkillTreeTest_Alpha));
	TestTrue(TEXT("An empty id clears a slot"), SkillTrees->AssignSlot(TreeTag, 2, FGameplayTag()));
	TestFalse(TEXT("R is empty"), SkillTrees->GetSlotAbilityId(TreeTag, 2).IsValid());

	TestTrue(TEXT("The tree resets for free"), SkillTrees->ResetTree(TreeTag));
	TestTrue(TEXT("Reset forgets every node"), SkillTrees->GetUnlockedNodes(TreeTag).IsEmpty());
	TestFalse(TEXT("Reset clears the slots"), SkillTrees->GetSlotAbilityId(TreeTag, 1).IsValid());
	TestEqual(TEXT("Reset refunds every point"), SkillTrees->GetAvailablePoints(TreeTag), 3);
	TestFalse(TEXT("Resetting an empty tree reports no change"), SkillTrees->ResetTree(TreeTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeRefundNodeTest,
	"SurvivalRpg.Progression.SkillTrees.RefundNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeRefundNodeTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	FEquipmentFixture Fixture;
	if (!MakeEquipmentFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	FScopedTestToolTree ToolTree(MakeTree());
	URpgEquipmentInstance* MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	if (!TestNotNull(TEXT("The tool equips"), MainHand))
	{
		return false;
	}

	URpgSkillTreeComponent* SkillTrees = Fixture.SkillTrees;
	RaiseLogging(*Fixture.PlayerState, 6);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_FormA);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Wide);
	TestEqual(TEXT("One point is left"), SkillTrees->GetAvailablePoints(TreeTag), 1);

	TestEqual(TEXT("A node another learned node requires cannot be refunded"),
		SkillTrees->EvaluateRefund(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeRefundResult::RequiredByNode);
	TestEqual(TEXT("A node whose point keeps a row gate met cannot be refunded"),
		SkillTrees->EvaluateRefund(TreeTag, TAG_SkillTreeTest_Second), ERpgSkillTreeRefundResult::PointsStillNeeded);
	TestEqual(TEXT("A node that is not learned cannot be refunded"),
		SkillTrees->EvaluateRefund(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeRefundResult::NotUnlocked);
	TestEqual(TEXT("An unknown node cannot be refunded"),
		SkillTrees->EvaluateRefund(TreeTag, TAG_SkillTreeTest_Removed), ERpgSkillTreeRefundResult::UnknownNode);
	TestEqual(TEXT("A rejected refund changes nothing"),
		SkillTrees->RefundNode(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeRefundResult::RequiredByNode);
	TestTrue(TEXT("Root stays learned"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Root));

	TestEqual(TEXT("A node nothing depends on is refunded"),
		SkillTrees->RefundNode(TreeTag, TAG_SkillTreeTest_FormA), ERpgSkillTreeRefundResult::Refundable);
	TestFalse(TEXT("The refunded node is forgotten"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormA));
	TestEqual(TEXT("Its point returns"), SkillTrees->GetAvailablePoints(TreeTag), 2);
	TestTrue(TEXT("The other purchases keep their order"),
		SkillTrees->GetUnlockedNodes(TreeTag) ==
			TArray<FGameplayTag>({TAG_SkillTreeTest_Root, TAG_SkillTreeTest_Second, TAG_SkillTreeTest_Wide}));
	TestTrue(TEXT("The exclusive alternative is no longer blocked"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_FormB) != ERpgSkillTreeUnlockResult::ExclusiveConflict);

	TestEqual(TEXT("Without FormA, Second is free to go"),
		SkillTrees->RefundNode(TreeTag, TAG_SkillTreeTest_Second), ERpgSkillTreeRefundResult::Refundable);
	TestFalse(TEXT("The refunded ability leaves its slot"), SkillTrees->GetSlotAbilityId(TreeTag, 1).IsValid());
	TestNull(TEXT("The refunded ability is no longer granted"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Beta));

	TestEqual(TEXT("Without dependents, Root is refunded too"),
		SkillTrees->RefundNode(TreeTag, TAG_SkillTreeTest_Root), ERpgSkillTreeRefundResult::Refundable);
	TestNull(TEXT("Root's ability is removed at once"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));
	TestFalse(TEXT("Root's tag is removed"), Fixture.AbilitySystem->HasMatchingGameplayTag(TAG_SkillTreeTest_Granted));
	TestFalse(TEXT("Q is cleared"), SkillTrees->GetSlotAbilityId(TreeTag, 0).IsValid());
	TestTrue(TEXT("The loose node stays learned"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Wide));
	TestEqual(TEXT("Every refunded point is back"), SkillTrees->GetAvailablePoints(TreeTag), 4);
	Fixture.Equipment->UnequipItem(MainHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeEquipmentGrantsTest,
	"SurvivalRpg.Progression.SkillTrees.WeaponGrantsLearnedNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeEquipmentGrantsTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	FEquipmentFixture Fixture;
	if (!MakeEquipmentFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	URpgSkillTreeDefinition* Tree = MakeTree();
	FScopedTestToolTree ToolTree(Tree);
	URpgEquipmentInstance* MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	if (!TestNotNull(TEXT("The tool equips in the main hand"), MainHand))
	{
		return false;
	}
	TestTrue(TEXT("Equipping the tool registers its tree"), Fixture.SkillTrees->FindSkillTree(TreeTag) == Tree);
	TestNull(TEXT("Nothing is granted before learning"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));

	RaiseLogging(*Fixture.PlayerState, 4);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	const FGameplayAbilitySpec* AlphaSpec = FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha);
	TestNotNull(TEXT("Learning grants the node's ability at once"), AlphaSpec);
	TestTrue(TEXT("The ability's source is the tool's equipment"), AlphaSpec && AlphaSpec->SourceObject.Get() == MainHand);
	TestTrue(TEXT("The node's tag is owned"), Fixture.AbilitySystem->HasMatchingGameplayTag(TAG_SkillTreeTest_Granted));

	Fixture.Equipment->UnequipItem(MainHand);
	TestNull(TEXT("Unequipping removes the ability"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));
	TestFalse(TEXT("Unequipping removes the tag"), Fixture.AbilitySystem->HasMatchingGameplayTag(TAG_SkillTreeTest_Granted));

	URpgEquipmentInstance* OffHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestOffHandDefinition::StaticClass(),
		ERpgEquipmentSlot::OffHand,
		Fixture.Tool);
	TestNotNull(TEXT("The tool equips in the off hand"), OffHand);
	TestNull(TEXT("A tree active only in the main hand grants nothing in the off hand"),
		FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));
	Fixture.Equipment->UnequipItem(OffHand);

	MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	TestNotNull(TEXT("Learned nodes survive a tool switch"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));

	Fixture.SkillTrees->ResetTree(TreeTag);
	TestNull(TEXT("Resetting the tree removes its ability"), FindSpecWithId(*Fixture.AbilitySystem, TAG_SkillTreeTest_Alpha));
	TestFalse(TEXT("Resetting the tree removes its tag"), Fixture.AbilitySystem->HasMatchingGameplayTag(TAG_SkillTreeTest_Granted));
	Fixture.Equipment->UnequipItem(MainHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeTuningTest,
	"SurvivalRpg.Progression.SkillTrees.TuningsChangeAbilityValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeTuningTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	FEquipmentFixture Fixture;
	if (!MakeEquipmentFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	FScopedTestToolTree ToolTree(MakeTree());
	URpgEquipmentInstance* MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	if (!TestNotNull(TEXT("The tool equips"), MainHand))
	{
		return false;
	}

	RaiseLogging(*Fixture.PlayerState, 7);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);

	const FGameplayAbilityActorInfo& ActorInfo = *Fixture.AbilitySystem->AbilityActorInfo;
	auto Radius = [&](const FGameplayTag AbilityId) -> float
	{
		const FGameplayAbilitySpec* Spec = FindSpecWithId(*Fixture.AbilitySystem, AbilityId);
		return Spec ? URpgGameplayAbility::GetTunedValueForSpec(*Spec, ActorInfo, TAG_SkillTreeTest_Radius, 500.0f) : -1.0f;
	};

	TestEqual(TEXT("Without upgrades Alpha keeps its base value"), Radius(TAG_SkillTreeTest_Alpha), 500.0f);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_FormA);
	TestEqual(TEXT("A Set entry for Alpha replaces its base value"), Radius(TAG_SkillTreeTest_Alpha), 900.0f);
	TestEqual(TEXT("Beta is not targeted by Alpha's entry"), Radius(TAG_SkillTreeTest_Beta), 500.0f);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Wide);
	TestEqual(TEXT("An Add entry for every ability follows the Set entry"), Radius(TAG_SkillTreeTest_Alpha), 1000.0f);
	TestEqual(TEXT("The Add entry reaches Beta too"), Radius(TAG_SkillTreeTest_Beta), 600.0f);
	TestEqual(TEXT("The passive is learned"),
		Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Passive), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("A Multiply entry applies last"), Radius(TAG_SkillTreeTest_Alpha), 1500.0f);
	TestEqual(TEXT("Beta ignores Alpha's multiplier"), Radius(TAG_SkillTreeTest_Beta), 600.0f);

	FGameplayAbilitySpec Unsourced(URpgInventoryAutomationTestUseAbility::StaticClass()->GetDefaultObject<UGameplayAbility>(), 1);
	Unsourced.GetDynamicSpecSourceTags().AddTag(TAG_SkillTreeTest_Alpha);
	TestEqual(TEXT("An ability without a weapon tree keeps its base value"),
		URpgGameplayAbility::GetTunedValueForSpec(Unsourced, ActorInfo, TAG_SkillTreeTest_Radius, 500.0f), 500.0f);
	Fixture.Equipment->UnequipItem(MainHand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeLoadoutTest,
	"SurvivalRpg.Progression.SkillTrees.WeaponAbilitySlotsFollowTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeLoadoutTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	FEquipmentFixture Fixture;
	if (!MakeEquipmentFixture(*this, TestWorld.GetWorld(), Fixture))
	{
		return false;
	}

	ARpgInventoryAutomationTestPlayerController* Controller =
		TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	URpgWeaponAbilityLoadoutComponent* Loadout = Controller ? Controller->GetWeaponAbilityLoadoutComponent() : nullptr;
	if (!TestNotNull(TEXT("Controller owns a weapon ability loadout"), Loadout))
	{
		return false;
	}
	Controller->PlayerState = Fixture.PlayerState;
	Controller->SetPawn(Fixture.Pawn);

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	FScopedTestToolTree ToolTree(MakeTree());
	URpgEquipmentInstance* MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	RaiseLogging(*Fixture.PlayerState, 4);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	Fixture.SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);

	Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	FRpgWeaponAbilityLoadoutSlot Q = Loadout->GetSlot(0);
	TestEqual(TEXT("Q follows the tree's assignment"), Q.AbilityIdTag, TAG_SkillTreeTest_Alpha.GetTag());
	TestTrue(TEXT("Q is bound"), Q.bAvailable);
	TestTrue(TEXT("A tree assignment is not a player selection"), Q.bDefaultSelection);
	TestEqual(TEXT("E follows the tree's assignment"), Loadout->GetSlot(1).AbilityIdTag, TAG_SkillTreeTest_Beta.GetTag());

	Fixture.SkillTrees->AssignSlot(TreeTag, 0, TAG_SkillTreeTest_Beta);
	Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	TestEqual(TEXT("Q follows a changed assignment"), Loadout->GetSlot(0).AbilityIdTag, TAG_SkillTreeTest_Beta.GetTag());
	TestEqual(TEXT("E received the swapped ability"), Loadout->GetSlot(1).AbilityIdTag, TAG_SkillTreeTest_Alpha.GetTag());

	Fixture.Equipment->UnequipItem(MainHand);
	Loadout->ApplyAbilityBindings(*Fixture.AbilitySystem);
	TestFalse(TEXT("Without the weapon Q is empty"), Loadout->GetSlot(0).AbilityIdTag.IsValid());
	TestFalse(TEXT("Without the weapon Q is unbound"), Loadout->GetSlot(0).bAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeRestoreTest,
	"SurvivalRpg.Progression.SkillTrees.SaveRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeRestoreTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	auto SpawnPlayer = [World](const int32 Level) -> ARpgInventoryAutomationTestPlayerState*
	{
		ARpgInventoryAutomationTestPlayerState* PlayerState =
			World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
		if (PlayerState)
		{
			RaiseLogging(*PlayerState, Level);
		}
		return PlayerState;
	};

	URpgSkillTreeDefinition* Tree = MakeTree();
	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	ARpgInventoryAutomationTestPlayerState* Source = SpawnPlayer(4);
	ARpgInventoryAutomationTestPlayerState* Target = SpawnPlayer(4);
	ARpgInventoryAutomationTestPlayerState* LowLevel = SpawnPlayer(2);
	if (!TestNotNull(TEXT("Source spawns"), Source) || !TestNotNull(TEXT("Target spawns"), Target) ||
		!TestNotNull(TEXT("Low-level player spawns"), LowLevel))
	{
		return false;
	}

	URpgSkillTreeComponent* SourceTrees = Source->GetSkillTreeComponent();
	URpgSkillTreeComponent* TargetTrees = Target->GetSkillTreeComponent();
	URpgSkillTreeComponent* LowLevelTrees = LowLevel->GetSkillTreeComponent();
	SourceTrees->RegisterSkillTree(Tree);
	TargetTrees->RegisterSkillTree(Tree);
	LowLevelTrees->RegisterSkillTree(Tree);
	SourceTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	SourceTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	SourceTrees->AssignSlot(TreeTag, 2, TAG_SkillTreeTest_Alpha);

	const TArray<FRpgSkillTreeState> Saved = SourceTrees->ExportSkillTreeStates();
	TestTrue(TEXT("The export is valid save data"), URpgSkillTreeComponent::ValidateSkillTreeStates(Saved));
	TestTrue(TEXT("The snapshot restores"), TargetTrees->RestoreSkillTreeStates(Saved));
	TestEqual(TEXT("Learned nodes survive"), TargetTrees->GetUnlockedNodes(TreeTag), SourceTrees->GetUnlockedNodes(TreeTag));
	TestEqual(TEXT("Slot assignments survive"), TargetTrees->GetSlotAbilityId(TreeTag, 2), TAG_SkillTreeTest_Alpha.GetTag());
	TestEqual(TEXT("Restored points match"), TargetTrees->GetAvailablePoints(TreeTag), SourceTrees->GetAvailablePoints(TreeTag));

	AddExpectedMessage(TEXT("no longer has saved node"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	AddExpectedMessage(TEXT("no longer fits"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	TArray<FRpgSkillTreeState> WithRemovedNode = Saved;
	WithRemovedNode[0].UnlockedNodes.Insert(TAG_SkillTreeTest_Removed, 1);
	FRpgSkillTreeState& Unregistered = WithRemovedNode.AddDefaulted_GetRef();
	Unregistered.TreeTag = TAG_SkillTreeTest_UnregisteredTree;
	Unregistered.UnlockedNodes.Add(TAG_SkillTreeTest_Removed);
	TestTrue(TEXT("A snapshot with a removed node restores"), TargetTrees->RestoreSkillTreeStates(WithRemovedNode));
	TestEqual(TEXT("The removed node is dropped and the rest kept"), TargetTrees->GetUnlockedNodes(TreeTag).Num(), 2);
	TestEqual(TEXT("An unregistered tree keeps its saved state"),
		TargetTrees->GetUnlockedNodes(TAG_SkillTreeTest_UnregisteredTree).Num(), 1);

	TestTrue(TEXT("A snapshot beyond the player's points restores"), LowLevelTrees->RestoreSkillTreeStates(Saved));
	TestTrue(TEXT("A tree whose purchases no longer fit is reset"), LowLevelTrees->GetUnlockedNodes(TreeTag).IsEmpty());
	TestFalse(TEXT("Its slots are cleared"), LowLevelTrees->GetSlotAbilityId(TreeTag, 2).IsValid());
	TestEqual(TEXT("Its points are refunded"), LowLevelTrees->GetAvailablePoints(TreeTag), 1);

	TArray<FRpgSkillTreeState> Duplicated = Saved;
	Duplicated.Add(Saved[0]);
	AddExpectedError(TEXT("Skill tree restore rejected"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A tree saved twice is rejected"), TargetTrees->RestoreSkillTreeStates(Duplicated));

	FRpgPlayerSaveData SaveData;
	SaveData.bHasSkillTreeProgression = true;
	SaveData.SkillTreeStates = Saved;
	TestTrue(TEXT("Schema 4 accepts skill trees"), SaveData.IsSchemaSupported());
	SaveData.SchemaVersion = FRpgPlayerSaveData::ProgressionSchemaVersion;
	TestFalse(TEXT("Schema 3 cannot carry skill trees"), SaveData.IsSchemaSupported());
	SaveData.bHasSkillTreeProgression = false;
	TestFalse(TEXT("Skill tree states need their presence flag"), SaveData.IsSchemaSupported());
	SaveData.SkillTreeStates.Reset();
	TestTrue(TEXT("Schema 3 profiles without skill trees still load"), SaveData.IsSchemaSupported());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeContentTest,
	"SurvivalRpg.Progression.SkillTrees.Content.AllTreesValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeContentTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	TArray<FAssetData> TreeAssets;
	AssetRegistry.GetAssetsByClass(URpgSkillTreeDefinition::StaticClass()->GetClassPathName(), TreeAssets, true);

	TMap<FGameplayTag, FString> TreesByTag;
	for (const FAssetData& TreeAsset : TreeAssets)
	{
		const URpgSkillTreeDefinition* Tree = Cast<URpgSkillTreeDefinition>(TreeAsset.GetAsset());
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *TreeAsset.GetObjectPathString()), Tree))
		{
			continue;
		}

		TArray<FText> Errors;
		TestTrue(*FString::Printf(TEXT("%s is a valid skill tree"), *GetNameSafe(Tree)), Tree->ValidateTree(Errors));
		for (const FText& Error : Errors)
		{
			AddError(FString::Printf(TEXT("%s: %s"), *GetNameSafe(Tree), *Error.ToString()));
		}

		if (const FString* Existing = TreesByTag.Find(Tree->TreeTag))
		{
			AddError(FString::Printf(TEXT("%s and %s share tree tag %s."), **Existing, *GetNameSafe(Tree), *Tree->TreeTag.ToString()));
		}
		TreesByTag.Add(Tree->TreeTag, GetNameSafe(Tree));
	}

	AddInfo(FString::Printf(TEXT("Validated %d skill tree assets."), TreeAssets.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeViewModelProjectionTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.TreeProjectsProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeViewModelProjectionTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	SkillTrees->RegisterSkillTree(MakeTree());
	URpgSkillTreeViewModel* Tree = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->BindSkillTree(SkillTrees, TreeTag);

	// Level 1 earns nothing: open nodes wait for points, gated ones stay locked.
	TestEqual(TEXT("Every node is projected"), Tree->GetNodes().Num(), 6);
	TestEqual(TEXT("The grid has three rows"), Tree->GetRowCount(), 3);
	TestEqual(TEXT("The grid has two columns"), Tree->GetColumnCount(), 2);
	TestEqual(TEXT("No points are earned at level 1"), Tree->GetEarnedPoints(), 0);
	URpgSkillTreeNodeViewModel* Root = Tree->FindNode(TAG_SkillTreeTest_Root);
	URpgSkillTreeNodeViewModel* FormA = Tree->FindNode(TAG_SkillTreeTest_FormA);
	URpgSkillTreeNodeViewModel* FormB = Tree->FindNode(TAG_SkillTreeTest_FormB);
	if (!TestNotNull(TEXT("Root is projected"), Root) || !TestNotNull(TEXT("FormA is projected"), FormA) ||
		!TestNotNull(TEXT("FormB is projected"), FormB))
	{
		return false;
	}
	TestEqual(TEXT("Root waits for a point"), Root->GetState(), ERpgSkillTreeNodeState::Unaffordable);
	TestEqual(TEXT("FormA is locked"), FormA->GetState(), ERpgSkillTreeNodeState::Locked);
	TestEqual(TEXT("Root sits in its authored cell"), Root->GetCell(), FIntPoint(0, 0));
	TestEqual(TEXT("FormB sits in its authored cell"), FormB->GetCell(), FIntPoint(1, 1));
	TestTrue(TEXT("Root offers its ability for Q/E/R"), Root->GetAbilityIdTag() == TAG_SkillTreeTest_Alpha);
	TestFalse(TEXT("A tuning-only node offers no ability"), FormA->GetAbilityIdTag().IsValid());
	if (TestEqual(TEXT("Every row has a gate"), Tree->GetRows().Num(), 3))
	{
		TestEqual(TEXT("Row 1 needs two spent points"), Tree->GetRows()[1].RequiredPoints, 2);
		TestFalse(TEXT("Row 1 is locked"), Tree->GetRows()[1].bIsUnlocked);
		TestTrue(TEXT("Row 0 is open"), Tree->GetRows()[0].bIsUnlocked);
	}
	TestEqual(TEXT("Both forms link to Root"), Tree->GetLinks().Num(), 2);
	TestEqual(TEXT("The tree has three empty slots"), Tree->GetSlots().Num(), URpgSkillTreeComponent::SlotCount);
	TestFalse(TEXT("Nothing to reset yet"), Tree->CanResetTree());

	RaiseLogging(*PlayerState, 4);
	Tree->Refresh();
	TestEqual(TEXT("Level 4 earns three points"), Tree->GetAvailablePoints(), 3);
	TestEqual(TEXT("Root can be learned"), Root->GetState(), ERpgSkillTreeNodeState::Unlockable);

	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Second);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_FormA);
	Tree->Refresh();
	TestTrue(TEXT("Node view models stay the same objects"), Tree->FindNode(TAG_SkillTreeTest_Root) == Root);
	TestEqual(TEXT("Root is learned"), Root->GetState(), ERpgSkillTreeNodeState::Unlocked);
	TestEqual(TEXT("FormB is excluded by FormA"), FormB->GetState(), ERpgSkillTreeNodeState::Excluded);
	TestTrue(TEXT("Row 1 opened with two spent points"), Tree->GetRows()[1].bIsUnlocked);
	TestEqual(TEXT("Root holds Q"), Root->GetAssignedSlotIndex(), 0);
	TestTrue(TEXT("The Q slot names Root"), Tree->GetSlots()[0].NodeTag == TAG_SkillTreeTest_Root);
	TestEqual(TEXT("Three points are spent"), Tree->GetSpentPoints(), 3);
	TestTrue(TEXT("A spent tree can be reset"), Tree->CanResetTree());
	const FRpgSkillTreeLinkView* RootToFormA = Tree->GetLinks().FindByPredicate([](const FRpgSkillTreeLinkView& Link)
	{
		return Link.ToCell == FIntPoint(0, 1);
	});
	TestTrue(TEXT("The Root-FormA link is learned at both ends"),
		RootToFormA && RootToFormA->bFromUnlocked && RootToFormA->bToUnlocked);

	// Commands: selection is local; assignment and reset go through the validated requests.
	Tree->SelectNode(TAG_SkillTreeTest_FormA);
	TestTrue(TEXT("FormA is selected"), FormA->IsSelected());
	Tree->AssignSelectedNodeToSlot(2);
	TestFalse(TEXT("A node without an ability cannot take a slot"), SkillTrees->GetSlotAbilityId(TreeTag, 2).IsValid());
	Tree->SelectNode(TAG_SkillTreeTest_Second);
	TestFalse(TEXT("Selecting another node clears the old selection"), FormA->IsSelected());
	Tree->AssignSelectedNodeToSlot(2);
	TestEqual(TEXT("The selected ability moved to R"), SkillTrees->GetSlotAbilityId(TreeTag, 2), TAG_SkillTreeTest_Beta.GetTag());
	Tree->Refresh();
	TestEqual(TEXT("Second shows R"), Tree->FindNode(TAG_SkillTreeTest_Second)->GetAssignedSlotIndex(), 2);
	Tree->ClearSlot(2);
	TestFalse(TEXT("Clearing a slot empties it"), SkillTrees->GetSlotAbilityId(TreeTag, 2).IsValid());

	TestFalse(TEXT("Root cannot be refunded while FormA needs it"), Root->CanRefund());
	TestFalse(TEXT("Second keeps FormA's row gate met"), Tree->FindNode(TAG_SkillTreeTest_Second)->CanRefund());
	TestTrue(TEXT("FormA can be refunded"), FormA->CanRefund());
	TestFalse(TEXT("A node that is not learned cannot be refunded"), FormB->CanRefund());
	FormA->RequestRefund();
	Tree->Refresh();
	TestFalse(TEXT("A refund request forgets the node"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormA));
	TestEqual(TEXT("The refund returns its point"), Tree->GetAvailablePoints(), 1);
	TestTrue(TEXT("Without FormA, Root can be refunded"), Root->CanRefund());

	Tree->RequestResetTree();
	Tree->Refresh();
	TestEqual(TEXT("Reset refunds every point"), Tree->GetAvailablePoints(), 3);
	TestEqual(TEXT("Root is learnable again"), Root->GetState(), ERpgSkillTreeNodeState::Unlockable);
	TestFalse(TEXT("Reset clears the Q slot"), Tree->GetSlots()[0].NodeTag.IsValid());

	Tree->RequestUnlockNode(TAG_SkillTreeTest_Root);
	TestTrue(TEXT("A request learns an affordable node"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Root));

	Tree->UnbindSkillTree();
	TestTrue(TEXT("Unbinding clears the nodes"), Tree->GetNodes().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeViewModelInvalidationTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.ChangesRefreshOncePerFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeViewModelInvalidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState = World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	const FGameplayTag TreeTag = TAG_SkillTreeTest_Tree;
	SkillTrees->RegisterSkillTree(MakeTree());
	URpgSkillTreeViewModel* Tree = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->BindSkillTree(SkillTrees, TreeTag);

	// Mastery XP and purchases arrive as separate events; the view rebuilds on the next tick.
	RaiseLogging(*PlayerState, 4);
	SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Root);
	TestEqual(TEXT("Changes do not rebuild synchronously"), Tree->GetSpentPoints(), 0);
	// The timer manager ticks once per engine frame.
	++GFrameCounter;
	World->GetTimerManager().Tick(0.016f);
	TestEqual(TEXT("The next tick shows the purchase"), Tree->GetSpentPoints(), 1);
	TestEqual(TEXT("The next tick shows the new points"), Tree->GetAvailablePoints(), 2);

	// Another skill's XP does not touch this tree.
	URpgSkillTreeNodeViewModel* Root = Tree->FindNode(TAG_SkillTreeTest_Root);
	RaiseSkillLevel(*PlayerState->GetTradeSkillProgressionComponent(), RpgTradeSkillGameplayTags::Skill_Gathering_Mining, 3);
	SkillTrees->ResetTree(TreeTag);
	TestTrue(TEXT("Root still shows learned before the tick"), Root && Root->GetState() == ERpgSkillTreeNodeState::Unlocked);
	++GFrameCounter;
	World->GetTimerManager().Tick(0.016f);
	TestTrue(TEXT("The reset arrives on the next tick"), Root && Root->GetState() == ERpgSkillTreeNodeState::Unlockable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillProgressionViewModelTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.ProgressionOverview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillProgressionViewModelTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	FScopedTestWorld TestWorld;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetWorld() ? TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgSkillTreeComponent* SkillTrees = PlayerState ? PlayerState->GetSkillTreeComponent() : nullptr;
	if (!TestNotNull(TEXT("Player state with skill trees spawns"), SkillTrees))
	{
		return false;
	}

	SkillTrees->RegisterSkillTree(MakeTree());
	RaiseLogging(*PlayerState, 3);
	URpgSkillProgressionViewModel* Progression =
		NewObject<URpgSkillProgressionViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->BindPlayerState(PlayerState);

	const TArray<URpgTradeSkillViewModel*> Skills = Progression->GetSkills();
	URpgTradeSkillViewModel* const* Logging = Skills.FindByPredicate([](const URpgTradeSkillViewModel* Skill)
	{
		return Skill && Skill->GetSkillTag() == RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	});
	if (TestNotNull(TEXT("The overview lists Logging"), Logging))
	{
		TestEqual(TEXT("Logging shows its level"), (*Logging)->GetLevel(), 3);
	}

	const TArray<URpgSkillTreeViewModel*> Trees = Progression->GetSkillTrees();
	URpgSkillTreeViewModel* const* TestTree = Trees.FindByPredicate([](const URpgSkillTreeViewModel* Tree)
	{
		return Tree && Tree->GetTreeTag() == TAG_SkillTreeTest_Tree;
	});
	if (!TestNotNull(TEXT("The overview lists the registered tree"), TestTree))
	{
		return false;
	}
	URpgSkillTreeViewModel* TestTreeViewModel = *TestTree;
	Progression->SelectSkillTree(TAG_SkillTreeTest_Tree);
	TestTrue(TEXT("Selecting a tree shows it"), Progression->GetSelectedSkillTree() == TestTreeViewModel);
	TestEqual(TEXT("The tree view shows the tree's points"), TestTreeViewModel->GetAvailablePoints(), 2);
	const int32 PointsBefore = Progression->GetTotalAvailablePoints();
	TestTrue(TEXT("Free points include the test tree"), PointsBefore >= 2);

	SkillTrees->UnlockNode(TAG_SkillTreeTest_Tree, TAG_SkillTreeTest_Root);
	Progression->Refresh();
	TestEqual(TEXT("Learning spends a free point"), Progression->GetTotalAvailablePoints(), PointsBefore - 1);
	TestTrue(TEXT("The selected tree survives a refresh"), Progression->GetSelectedSkillTree() == TestTreeViewModel);

	// A screen refreshes from one event, also when only the selected tree changed.
	URpgSkillTreeAutomationTestListener* Listener =
		NewObject<URpgSkillTreeAutomationTestListener>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->OnProgressionChanged.AddDynamic(Listener, &URpgSkillTreeAutomationTestListener::HandleChanged);
	TestTreeViewModel->SelectNode(TAG_SkillTreeTest_Root);
	TestEqual(TEXT("Selecting a node of the selected tree notifies the overview"), Listener->Broadcasts, 1);
	if (URpgSkillTreeViewModel* const* OtherTree = Trees.FindByPredicate([TestTreeViewModel](const URpgSkillTreeViewModel* Tree)
	{
		return Tree && Tree != TestTreeViewModel;
	}))
	{
		const int32 BroadcastsBefore = Listener->Broadcasts;
		(*OtherTree)->Refresh();
		TestEqual(TEXT("Trees that are not selected do not notify the overview"), Listener->Broadcasts, BroadcastsBefore);
	}

	Progression->Unbind();
	TestTrue(TEXT("Unbinding clears the trees"), Progression->GetSkillTrees().IsEmpty());
	TestTrue(TEXT("Unbinding clears the skills"), Progression->GetSkills().IsEmpty());
	TestTrue(TEXT("Unbinding clears the selection"), Progression->GetSelectedSkillTree() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillDisplayNameTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.SkillDisplayNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillDisplayNameTest::RunTest(const FString& Parameters)
{
	URpgTradeSkillProgressionComponent* TradeSkills =
		NewObject<URpgTradeSkillProgressionComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	const FGameplayTag Logging = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	TestEqual(TEXT("Without config the tag leaf names the skill"), TradeSkills->GetSkillDisplayName(Logging).ToString(), FString(TEXT("Logging")));

	URpgTradeSkillConfigData* Config = NewObject<URpgTradeSkillConfigData>(GetTransientPackage(), NAME_None, RF_Transient);
	Config->TaggedSkillConfigs.Add(Logging).DisplayName = FText::FromString(TEXT("Woodcutting"));
	TradeSkills->ConfigData = Config;
	TestEqual(TEXT("An authored name wins"), TradeSkills->GetSkillDisplayName(Logging).ToString(), FString(TEXT("Woodcutting")));
	TestEqual(TEXT("Skills without a name still use the tag leaf"),
		TradeSkills->GetSkillDisplayName(RpgTradeSkillGameplayTags::Skill_Gathering_Mining).ToString(), FString(TEXT("Mining")));
	return true;
}

#endif
