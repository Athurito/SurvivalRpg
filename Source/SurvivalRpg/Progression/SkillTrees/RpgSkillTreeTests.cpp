#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "RpgSkillTreeAutomationTestTypes.h"
#include "RpgSkillTreeComponent.h"
#include "RpgSkillTreeDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Game/RpgPlayerSaveData.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/Progression/Skills/Data/RpgTradeSkillConfigData.h"
#include "TimerManager.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include <limits>

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Tree, "SkillTree.Tree.AutomationTest");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_UnregisteredTree, "SkillTree.Tree.AutomationTestUnregistered");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_SecondTree, "SkillTree.Tree.AutomationTestSecond");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Root, "SkillTree.Node.AutomationTest.Root");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Second, "SkillTree.Node.AutomationTest.Second");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormA, "SkillTree.Node.AutomationTest.FormA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormB, "SkillTree.Node.AutomationTest.FormB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Wide, "SkillTree.Node.AutomationTest.Wide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Passive, "SkillTree.Node.AutomationTest.Passive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Removed, "SkillTree.Node.AutomationTest.Removed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_Granted, "SkillTree.AutomationTest.Granted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormAGranted, "SkillTree.AutomationTest.FormAGranted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SkillTreeTest_FormBGranted, "SkillTree.AutomationTest.FormBGranted");
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

namespace RpgSkillTreeTests
{
	/**
	 * The reference tree on a FreePosition map: every node keeps cell 0/0, which only a Grid tree would reject. Root
	 * and Second form region Core, the two forms region Forms, which the map frames first.
	 */
	URpgSkillTreeDefinition* MakeMapTree()
	{
		URpgSkillTreeDefinition* Tree = MakeTree();
		Tree->LayoutMode = ERpgSkillTreeLayoutMode::FreePosition;
		Tree->DefaultRegionId = TEXT("Forms");
		for (const TCHAR* RegionId : {TEXT("Core"), TEXT("Forms")})
		{
			FRpgSkillTreeMapRegion& Region = Tree->Regions.AddDefaulted_GetRef();
			Region.RegionId = RegionId;
			Region.Padding = FVector2D(50.0, 40.0);
		}

		const FVector2D Positions[] = {
			FVector2D(0.0, 0.0),
			FVector2D(250.0, 0.0),
			FVector2D(-120.0, 220.0),
			FVector2D(120.0, 220.0),
			FVector2D(500.0, 300.0),
			FVector2D(500.0, 450.0)};
		const TCHAR* Regions[] = {TEXT("Core"), TEXT("Core"), TEXT("Forms"), TEXT("Forms"), nullptr, nullptr};
		for (int32 Index = 0; Index < Tree->Nodes.Num(); ++Index)
		{
			Tree->Nodes[Index].Row = 0;
			Tree->Nodes[Index].Column = 0;
			Tree->Nodes[Index].MapPosition = Positions[Index];
			Tree->Nodes[Index].RegionId = Regions[Index] ? FName(Regions[Index]) : NAME_None;
		}
		return Tree;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeDefinitionMapLayoutTest,
	"SurvivalRpg.Progression.SkillTrees.Definition.MapLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeDefinitionMapLayoutTest::RunTest(const FString& Parameters)
{
	using namespace RpgSkillTreeTests;

	// A Grid tree turns its cells into map positions.
	const URpgSkillTreeDefinition* GridTree = MakeTree();
	TestEqual(TEXT("Existing trees use the grid layout"), GridTree->LayoutMode, ERpgSkillTreeLayoutMode::Grid);
	TestTrue(TEXT("Cell 0/0 sits at the map origin"), GridTree->GetNodeMapPosition(GridTree->Nodes[0]).Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("Cell row 1, column 1 sits one spacing right and down"),
		GridTree->GetNodeMapPosition(GridTree->Nodes[3]).Equals(FVector2D(160.0, 160.0)));
	const FBox2D GridBounds = GridTree->GetMapBounds();
	TestTrue(TEXT("Grid bounds span every cell center"),
		GridBounds.bIsValid && GridBounds.Min.Equals(FVector2D::ZeroVector) && GridBounds.Max.Equals(FVector2D(160.0, 320.0)));

	// A FreePosition tree uses authored positions and frames its regions around their nodes.
	URpgSkillTreeDefinition* MapTree = MakeMapTree();
	TArray<FText> Errors;
	TestTrue(TEXT("The map tree is valid"), MapTree->ValidateTree(Errors));
	for (const FText& Error : Errors)
	{
		AddError(Error.ToString());
	}
	TestTrue(TEXT("A free node sits at its MapPosition"), MapTree->GetNodeMapPosition(MapTree->Nodes[2]).Equals(FVector2D(-120.0, 220.0)));
	const FBox2D Core = MapTree->GetRegionBounds(TEXT("Core"));
	TestTrue(TEXT("A region frames its nodes with its padding"),
		Core.bIsValid && Core.Min.Equals(FVector2D(-50.0, -40.0)) && Core.Max.Equals(FVector2D(300.0, 40.0)));
	TestFalse(TEXT("An unknown region has no bounds"), MapTree->GetRegionBounds(TEXT("Missing")).bIsValid);
	const FBox2D MapBounds = MapTree->GetMapBounds();
	TestTrue(TEXT("Map bounds include the regions and loose nodes"),
		MapBounds.Min.Equals(FVector2D(-170.0, -40.0)) && MapBounds.Max.Equals(FVector2D(500.0, 450.0)));
	TestNotNull(TEXT("FindRegion finds an authored region"), MapTree->FindRegion(TEXT("Forms")));

	// MapOrigin places the whole cluster on the shared map: positions, regions and bounds move together.
	const FVector2D Origin(2000.0, -500.0);
	URpgSkillTreeDefinition* MovedTree = MakeMapTree();
	MovedTree->MapOrigin = Origin;
	TestTrue(TEXT("The map origin moves a free node"),
		MovedTree->GetNodeMapPosition(MovedTree->Nodes[2]).Equals(FVector2D(-120.0, 220.0) + Origin));
	const FBox2D MovedCore = MovedTree->GetRegionBounds(TEXT("Core"));
	TestTrue(TEXT("The map origin moves a region"),
		MovedCore.bIsValid && MovedCore.Min.Equals(Core.Min + Origin) && MovedCore.Max.Equals(Core.Max + Origin));
	const FBox2D MovedBounds = MovedTree->GetMapBounds();
	TestTrue(TEXT("The map origin moves the tree's bounds"),
		MovedBounds.Min.Equals(MapBounds.Min + Origin) && MovedBounds.Max.Equals(MapBounds.Max + Origin));
	URpgSkillTreeDefinition* MovedGrid = MakeTree();
	MovedGrid->MapOrigin = Origin;
	TestTrue(TEXT("The map origin moves grid cells"),
		MovedGrid->GetNodeMapPosition(MovedGrid->Nodes[3]).Equals(FVector2D(160.0, 160.0) + Origin));

	auto ExpectInvalid = [this](const TCHAR* What, URpgSkillTreeDefinition* Tree)
	{
		TArray<FText> CaseErrors;
		TestFalse(What, Tree->ValidateTree(CaseErrors));
		TestTrue(FString::Printf(TEXT("%s reports a reason"), What), !CaseErrors.IsEmpty());
	};

	URpgSkillTreeDefinition* SharedMapPosition = MakeMapTree();
	SharedMapPosition->Nodes[1].MapPosition = SharedMapPosition->Nodes[0].MapPosition;
	ExpectInvalid(TEXT("Two nodes at one map position are rejected"), SharedMapPosition);

	URpgSkillTreeDefinition* NonFinitePosition = MakeMapTree();
	NonFinitePosition->Nodes[4].MapPosition.X = std::numeric_limits<double>::quiet_NaN();
	ExpectInvalid(TEXT("A non-finite map position is rejected"), NonFinitePosition);

	URpgSkillTreeDefinition* UnknownNodeRegion = MakeMapTree();
	UnknownNodeRegion->Nodes[4].RegionId = TEXT("Missing");
	ExpectInvalid(TEXT("A node in an unknown region is rejected"), UnknownNodeRegion);

	URpgSkillTreeDefinition* EmptyRegion = MakeMapTree();
	EmptyRegion->Regions.AddDefaulted_GetRef().RegionId = TEXT("Empty");
	ExpectInvalid(TEXT("A region without nodes is rejected"), EmptyRegion);

	URpgSkillTreeDefinition* DuplicateRegion = MakeMapTree();
	const FRpgSkillTreeMapRegion RegionCopy = DuplicateRegion->Regions[0];
	DuplicateRegion->Regions.Add(RegionCopy);
	ExpectInvalid(TEXT("A duplicated region id is rejected"), DuplicateRegion);

	URpgSkillTreeDefinition* UnnamedRegion = MakeMapTree();
	UnnamedRegion->Regions.AddDefaulted();
	ExpectInvalid(TEXT("A region without id is rejected"), UnnamedRegion);

	URpgSkillTreeDefinition* UnknownDefaultRegion = MakeMapTree();
	UnknownDefaultRegion->DefaultRegionId = TEXT("Missing");
	ExpectInvalid(TEXT("An unknown default region is rejected"), UnknownDefaultRegion);

	URpgSkillTreeDefinition* NegativePadding = MakeMapTree();
	NegativePadding->Regions[0].Padding.Y = -1.0;
	ExpectInvalid(TEXT("A negative region padding is rejected"), NegativePadding);

	URpgSkillTreeDefinition* SharedCellOnGrid = MakeMapTree();
	SharedCellOnGrid->LayoutMode = ERpgSkillTreeLayoutMode::Grid;
	ExpectInvalid(TEXT("The same nodes on a Grid tree share a cell"), SharedCellOnGrid);

	URpgSkillTreeDefinition* FlatGrid = MakeTree();
	FlatGrid->GridSpacing.Y = 0.0;
	ExpectInvalid(TEXT("A grid without vertical spacing is rejected"), FlatGrid);

	URpgSkillTreeDefinition* NonFiniteOrigin = MakeMapTree();
	NonFiniteOrigin->MapOrigin.Y = std::numeric_limits<double>::infinity();
	ExpectInvalid(TEXT("A non-finite map origin is rejected"), NonFiniteOrigin);
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
	FRpgSkillTreeSwapTest,
	"SurvivalRpg.Progression.SkillTrees.SwapExclusiveChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeSwapTest::RunTest(const FString& Parameters)
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
	URpgSkillTreeDefinition* Tree = MakeTree();
	SkillTrees->RegisterSkillTree(Tree);
	RaiseLogging(*PlayerState, 4);
	for (const FGameplayTag& NodeTag : TArray<FGameplayTag>{TAG_SkillTreeTest_Root, TAG_SkillTreeTest_Second, TAG_SkillTreeTest_FormA})
	{
		SkillTrees->UnlockNode(TreeTag, NodeTag);
	}
	TestEqual(TEXT("Every point is spent"), SkillTrees->GetAvailablePoints(TreeTag), 0);
	TestEqual(TEXT("The other form is excluded"),
		SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeUnlockResult::ExclusiveConflict);

	// A swap at zero free points: the replaced form's point pays for the new one.
	FGameplayTag Replaced;
	TestEqual(TEXT("Swapping forms needs no free point"),
		SkillTrees->EvaluateSwap(TreeTag, TAG_SkillTreeTest_FormB, Replaced), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("The swap names the learned form it replaces"), Replaced, FGameplayTag(TAG_SkillTreeTest_FormA));
	TestEqual(TEXT("The swap succeeds"), SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeUnlockResult::Unlockable);
	TestTrue(TEXT("Only the new form is learned"),
		!SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormA) && SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormB));
	TestTrue(TEXT("The points moved over"), SkillTrees->GetSpentPoints(TreeTag) == 3 && SkillTrees->GetAvailablePoints(TreeTag) == 0);
	const TArray<FGameplayTag> ExpectedOrder = {TAG_SkillTreeTest_Root, TAG_SkillTreeTest_Second, TAG_SkillTreeTest_FormB};
	TestTrue(TEXT("The new form joins the end of the purchase order"), SkillTrees->GetUnlockedNodes(TreeTag) == ExpectedOrder);

	// Repeated and stale requests change nothing.
	TestEqual(TEXT("Repeating the swap is rejected"),
		SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeUnlockResult::AlreadyUnlocked);
	TestTrue(TEXT("A repeated swap changes nothing"), SkillTrees->GetUnlockedNodes(TreeTag) == ExpectedOrder);
	TestEqual(TEXT("A plain unlock still refuses the excluded form"),
		SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_FormA), ERpgSkillTreeUnlockResult::ExclusiveConflict);
	SkillTrees->RequestSwapNode(TreeTag, TAG_SkillTreeTest_FormA);
	TestTrue(TEXT("The swap request switches back"),
		SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormA) && !SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormB));
	TestEqual(TEXT("Outside a learned group a swap is an unlock"),
		SkillTrees->EvaluateSwap(TreeTag, TAG_SkillTreeTest_Wide, Replaced), SkillTrees->EvaluateUnlock(TreeTag, TAG_SkillTreeTest_Wide));
	TestFalse(TEXT("Outside a learned group nothing would be replaced"), Replaced.IsValid());

	// A choice another learned node needs stays, and the owner hears why.
	URpgSkillTreeAutomationTestRejectionListener* Listener =
		NewObject<URpgSkillTreeAutomationTestRejectionListener>(GetTransientPackage(), NAME_None, RF_Transient);
	SkillTrees->OnRequestRejected.AddDynamic(Listener, &URpgSkillTreeAutomationTestRejectionListener::HandleRejected);
	Tree->Nodes.FindByPredicate([](const FRpgSkillTreeNode& Node) { return Node.NodeTag == TAG_SkillTreeTest_Wide; })
		->Prerequisites.Add(TAG_SkillTreeTest_FormA);
	RaiseLogging(*PlayerState, 5);
	TestEqual(TEXT("Wide builds on FormA"), SkillTrees->UnlockNode(TreeTag, TAG_SkillTreeTest_Wide), ERpgSkillTreeUnlockResult::Unlockable);
	const TArray<FGameplayTag> NeededOrder = SkillTrees->GetUnlockedNodes(TreeTag);
	TestEqual(TEXT("A needed choice cannot be swapped out"),
		SkillTrees->EvaluateSwap(TreeTag, TAG_SkillTreeTest_FormB, Replaced), ERpgSkillTreeUnlockResult::ChoiceStillNeeded);
	SkillTrees->RequestSwapNode(TreeTag, TAG_SkillTreeTest_FormB);
	TestTrue(TEXT("A rejected swap changes nothing"), SkillTrees->GetUnlockedNodes(TreeTag) == NeededOrder);
	SkillTrees->RequestUnlockNode(TreeTag, TAG_SkillTreeTest_Passive);
	SkillTrees->RequestRefundNode(TreeTag, TAG_SkillTreeTest_FormA);
	const TArray<FGameplayTag> ExpectedRejectedNodes = {TAG_SkillTreeTest_FormB, TAG_SkillTreeTest_Passive, TAG_SkillTreeTest_FormA};
	const TArray<ERpgSkillTreeRequestRejection> ExpectedReasons = {
		ERpgSkillTreeRequestRejection::ChoiceStillNeeded,
		ERpgSkillTreeRequestRejection::NotEnoughPoints,
		ERpgSkillTreeRequestRejection::RequiredByNode};
	TestTrue(TEXT("Each rejected request reports its node and reason"),
		Listener->NodeTags == ExpectedRejectedNodes && Listener->Reasons == ExpectedReasons);

	// A form that costs more than the freed and free points together stays out of reach.
	SkillTrees->RefundNode(TreeTag, TAG_SkillTreeTest_Wide);
	Tree->Nodes.FindByPredicate([](const FRpgSkillTreeNode& Node) { return Node.NodeTag == TAG_SkillTreeTest_FormB; })->Cost = 3;
	TestEqual(TEXT("A swap still needs enough points"),
		SkillTrees->EvaluateSwap(TreeTag, TAG_SkillTreeTest_FormB, Replaced), ERpgSkillTreeUnlockResult::NotEnoughPoints);
	TestEqual(TEXT("An unaffordable swap is refused"),
		SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormB), ERpgSkillTreeUnlockResult::NotEnoughPoints);
	TestTrue(TEXT("An unaffordable swap keeps the learned form"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_FormA));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeSwapGrantsTest,
	"SurvivalRpg.Progression.SkillTrees.SwapReconcilesGrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeSwapGrantsTest::RunTest(const FString& Parameters)
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
	for (FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (Node.NodeTag == TAG_SkillTreeTest_FormA)
		{
			Node.GrantedTags.AddTag(TAG_SkillTreeTest_FormAGranted);
		}
		else if (Node.NodeTag == TAG_SkillTreeTest_FormB)
		{
			Node.GrantedTags.AddTag(TAG_SkillTreeTest_FormBGranted);
		}
	}
	FScopedTestToolTree ToolTree(Tree);
	URpgEquipmentInstance* MainHand = Fixture.Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand,
		Fixture.Tool);
	if (!TestNotNull(TEXT("The tool equips in the main hand"), MainHand))
	{
		return false;
	}

	RaiseLogging(*Fixture.PlayerState, 4);
	for (const FGameplayTag& NodeTag : TArray<FGameplayTag>{TAG_SkillTreeTest_Root, TAG_SkillTreeTest_Second, TAG_SkillTreeTest_FormA})
	{
		Fixture.SkillTrees->UnlockNode(TreeTag, NodeTag);
	}
	UAbilitySystemComponent& AbilitySystem = *Fixture.AbilitySystem;
	TestEqual(TEXT("FormA's grant is owned once"), AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormAGranted), 1);

	Fixture.SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormB);
	TestEqual(TEXT("The swap removes FormA's grant"), AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormAGranted), 0);
	TestEqual(TEXT("The swap grants FormB once"), AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormBGranted), 1);

	Fixture.SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormB);
	TestEqual(TEXT("A repeated swap does not grant twice"), AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormBGranted), 1);

	Fixture.SkillTrees->SwapNode(TreeTag, TAG_SkillTreeTest_FormA);
	TestTrue(TEXT("Swapping back restores FormA's grant alone"),
		AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormAGranted) == 1 && AbilitySystem.GetTagCount(TAG_SkillTreeTest_FormBGranted) == 0);
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
	FRpgCombatSkillTreeContentTest,
	"SurvivalRpg.Progression.SkillTrees.Content.CombatTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCombatSkillTreeContentTest::RunTest(const FString& Parameters)
{
	// The combat cluster: kills earn its points, every melee weapon carries it, and its talents change attack damage and
	// blocking in the intended direction. Exact values stay tuning.
	const URpgSkillTreeDefinition* Tree = LoadObject<URpgSkillTreeDefinition>(
		nullptr, TEXT("/GF_Combat_Core/Progression/SkillTrees/DA_SkillTree_Combat.DA_SkillTree_Combat"));
	if (!TestNotNull(TEXT("The combat tree loads"), Tree))
	{
		return false;
	}
	TestEqual(TEXT("The combat tree has its stable tag"), Tree->TreeTag, FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Combat")));
	TestEqual(TEXT("Combat mastery earns the points"), Tree->MasterySkillTag, RpgTradeSkillGameplayTags::Skill_Mastery_Combat.GetTag());

	for (const TCHAR* ItemPath : {
		TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicSword.ID_BasicSword_C"),
		TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicTwoHandedSword.ID_BasicTwoHandedSword_C")})
	{
		const UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ItemPath);
		const URpgInventoryItemDefinition* Item = ItemClass ? GetDefault<URpgInventoryItemDefinition>(ItemClass) : nullptr;
		const URpgInventoryFragment_SkillTree* Fragment = Item
			? Cast<URpgInventoryFragment_SkillTree>(Item->FindFragmentByClass(URpgInventoryFragment_SkillTree::StaticClass()))
			: nullptr;
		TestTrue(*FString::Printf(TEXT("%s carries the combat tree in the main hand"), ItemPath),
			Fragment && Fragment->SkillTree == Tree && Fragment->IsActiveInSlot(ERpgEquipmentSlot::MainHand));
	}

	// Raises: a multiplier above one, or a positive addition.
	auto RaisesBy = [](const EGameplayModOp::Type Op, const float Magnitude)
	{
		return Op == EGameplayModOp::MultiplyAdditive || Op == EGameplayModOp::MultiplyCompound
			? Magnitude > 1.0f
			: Magnitude > 0.0f;
	};
	auto ChangesAttribute = [&RaisesBy](const FRpgSkillTreeNode& Node, const FGameplayAttribute& Attribute, const bool bRaise)
	{
		for (const FRpgAbilitySet_GameplayEffect& Granted : Node.AbilitySet ? Node.AbilitySet->GetGrantedGameplayEffects() : TArray<FRpgAbilitySet_GameplayEffect>())
		{
			const UGameplayEffect* Effect = Granted.GameplayEffect ? Granted.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
			if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite)
			{
				continue;
			}
			for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
			{
				float Magnitude = 0.0f;
				if (Modifier.Attribute == Attribute && Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Magnitude) &&
					RaisesBy(Modifier.ModifierOp, Magnitude) == bRaise)
				{
					return true;
				}
			}
		}
		return false;
	};

	const FRpgSkillTreeNode* Rhythm = Tree->FindNode(FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.Combat.BattleRhythm")));
	const FRpgSkillTreeNode* Riposte = Tree->FindNode(FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.Combat.Riposte")));
	const FRpgSkillTreeNode* Bulwark = Tree->FindNode(FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.Combat.Bulwark")));
	if (!TestTrue(TEXT("The combat tree has Battle Rhythm, Riposte and Bulwark"), Rhythm && Riposte && Bulwark))
	{
		return false;
	}
	TestTrue(TEXT("Battle Rhythm is the entry and raises attack damage"),
		Rhythm->Prerequisites.IsEmpty() && Rhythm->AbilityTunings.ContainsByPredicate([](const FRpgSkillTreeAbilityTuning& Tuning)
		{
			return Tuning.TuningTag == RpgGameplayTags::Ability_Tuning_Attack_Damage &&
				(Tuning.Operation == ERpgSkillTreeTuningOperation::Multiply ? Tuning.Value > 1.0f
					: Tuning.Operation == ERpgSkillTreeTuningOperation::Add && Tuning.Value > 0.0f);
		}));
	TestTrue(TEXT("Riposte and Bulwark follow Battle Rhythm and exclude each other"),
		Riposte->Prerequisites.Contains(Rhythm->NodeTag) && Bulwark->Prerequisites.Contains(Rhythm->NodeTag) &&
			!Riposte->ExclusiveGroup.IsNone() && Riposte->ExclusiveGroup == Bulwark->ExclusiveGroup);
	TestTrue(TEXT("Riposte raises a perfect block's stagger damage and stamina"),
		ChangesAttribute(*Riposte, URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute(), true) &&
			ChangesAttribute(*Riposte, URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute(), true));
	TestTrue(TEXT("Bulwark lowers the block's stamina cost and raises its damage reduction"),
		ChangesAttribute(*Bulwark, URpgDefenseSet::GetBlockStaminaCostAttribute(), false) &&
			ChangesAttribute(*Bulwark, URpgDefenseSet::GetBlockDamageReductionAttribute(), true));
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
	FRpgSkillTreeViewModelMapLayoutTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.MapLayoutProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeViewModelMapLayoutTest::RunTest(const FString& Parameters)
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

	const URpgSkillTreeDefinition* Definition = MakeMapTree();
	SkillTrees->RegisterSkillTree(Definition);
	URpgSkillTreeViewModel* Tree = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->BindSkillTree(SkillTrees, TAG_SkillTreeTest_Tree);

	TestEqual(TEXT("The view model reports the layout mode"), Tree->GetLayoutMode(), ERpgSkillTreeLayoutMode::FreePosition);
	TestTrue(TEXT("The view model reports the map bounds"), Tree->GetMapBounds() == Definition->GetMapBounds() && Tree->GetMapBounds().bIsValid);
	TestEqual(TEXT("The view model names the default region"), Tree->GetDefaultRegionId(), FName(TEXT("Forms")));
	if (TestEqual(TEXT("Both regions are projected"), Tree->GetRegions().Num(), 2))
	{
		const FRpgSkillTreeRegionView* Core = Tree->FindRegion(TEXT("Core"));
		TestTrue(TEXT("A region view carries its framed bounds"), Core && Core->Bounds == Definition->GetRegionBounds(TEXT("Core")));
	}

	const URpgSkillTreeNodeViewModel* FormA = Tree->FindNode(TAG_SkillTreeTest_FormA);
	if (TestNotNull(TEXT("FormA is projected"), FormA))
	{
		TestTrue(TEXT("A node view model carries its map position"), FormA->GetMapPosition().Equals(FVector2D(-120.0, 220.0)));
		TestEqual(TEXT("A node view model carries its region"), FormA->GetRegionId(), FName(TEXT("Forms")));
	}

	// Links name both ends and carry their map positions; they exist only for authored prerequisites.
	const FRpgSkillTreeLinkView* RootToFormA = Tree->GetLinks().FindByPredicate([](const FRpgSkillTreeLinkView& Link)
	{
		return Link.ToNodeTag == TAG_SkillTreeTest_FormA;
	});
	TestEqual(TEXT("Only the two prerequisites are linked"), Tree->GetLinks().Num(), 2);
	TestTrue(TEXT("The link names its prerequisite"), RootToFormA && RootToFormA->FromNodeTag == TAG_SkillTreeTest_Root);
	TestTrue(TEXT("The link starts at the prerequisite's map position"), RootToFormA && RootToFormA->FromPosition.Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("The link ends at the dependent's map position"), RootToFormA && RootToFormA->ToPosition.Equals(FVector2D(-120.0, 220.0)));

	Tree->UnbindSkillTree();
	TestTrue(TEXT("Unbinding clears the regions"), Tree->GetRegions().IsEmpty() && !Tree->GetMapBounds().bIsValid);
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

	// A screen refreshes from one event; one map shows every tree, so every tree notifies it.
	URpgSkillTreeAutomationTestListener* Listener =
		NewObject<URpgSkillTreeAutomationTestListener>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->OnProgressionChanged.AddDynamic(Listener, &URpgSkillTreeAutomationTestListener::HandleChanged);
	TestTreeViewModel->SelectNode(TAG_SkillTreeTest_Root);
	TestEqual(TEXT("Selecting a node notifies the overview"), Listener->Broadcasts, 1);
	TestTrue(TEXT("The overview reports the selected node"),
		Progression->GetSelectedNode() && Progression->GetSelectedNode() == TestTreeViewModel->FindNode(TAG_SkillTreeTest_Root));

	URpgSkillTreeDefinition* SecondTree = MakeTree();
	SecondTree->TreeTag = TAG_SkillTreeTest_SecondTree;
	SecondTree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	SkillTrees->RegisterSkillTree(SecondTree);
	Progression->Refresh();
	const TArray<URpgSkillTreeViewModel*> TreesWithSecond = Progression->GetSkillTrees();
	URpgSkillTreeViewModel* const* OtherTree = TreesWithSecond.FindByPredicate([](const URpgSkillTreeViewModel* Tree)
	{
		return Tree && Tree->GetTreeTag() == TAG_SkillTreeTest_SecondTree;
	});
	if (TestNotNull(TEXT("The overview lists a second tree"), OtherTree))
	{
		const int32 BroadcastsBefore = Listener->Broadcasts;
		(*OtherTree)->Refresh();
		TestTrue(TEXT("Every tree notifies the overview"), Listener->Broadcasts > BroadcastsBefore);
		TestEqual(TEXT("The second tree has its own points"), (*OtherTree)->GetSpentPoints(), 0);

		(*OtherTree)->SelectNode(TAG_SkillTreeTest_Second);
		TestTrue(TEXT("A selection in another tree is the newer one"),
			Progression->GetSelectedNode() == (*OtherTree)->FindNode(TAG_SkillTreeTest_Second) &&
				Progression->GetSelectedSkillTree() == *OtherTree);
		(*OtherTree)->SelectNode(FGameplayTag());
		TestTrue(TEXT("Clearing it shows the selection that remains"),
			Progression->GetSelectedNode() == TestTreeViewModel->FindNode(TAG_SkillTreeTest_Root) &&
				Progression->GetSelectedSkillTree() == TestTreeViewModel);
	}
	TestTreeViewModel->SelectNode(FGameplayTag());
	TestNull(TEXT("Without a selection the overview reports no node"), Progression->GetSelectedNode());
	TestTrue(TEXT("Without a selection the picked tree stays selected"), Progression->GetSelectedSkillTree() == TestTreeViewModel);

	Progression->Unbind();
	TestTrue(TEXT("Unbinding clears the trees"), Progression->GetSkillTrees().IsEmpty());
	TestTrue(TEXT("Unbinding clears the skills"), Progression->GetSkills().IsEmpty());
	TestTrue(TEXT("Unbinding clears the selection"), Progression->GetSelectedSkillTree() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSkillTreeRequestFeedbackTest,
	"SurvivalRpg.Progression.SkillTrees.ViewModel.SwapAndRejectionFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSkillTreeRequestFeedbackTest::RunTest(const FString& Parameters)
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
	for (const FGameplayTag& NodeTag : TArray<FGameplayTag>{TAG_SkillTreeTest_Root, TAG_SkillTreeTest_Second, TAG_SkillTreeTest_FormA})
	{
		SkillTrees->UnlockNode(TreeTag, NodeTag);
	}

	URpgSkillTreeViewModel* View = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	View->BindSkillTree(SkillTrees, TreeTag);
	const URpgSkillTreeNodeViewModel* FormB = View->FindNode(TAG_SkillTreeTest_FormB);
	TestTrue(TEXT("The excluded form offers a swap that replaces the learned one"),
		FormB && FormB->GetState() == ERpgSkillTreeNodeState::Excluded && FormB->CanSwap() &&
			FormB->GetSwapReplacesNodeTag() == TAG_SkillTreeTest_FormA);
	const URpgSkillTreeNodeViewModel* Passive = View->FindNode(TAG_SkillTreeTest_Passive);
	TestTrue(TEXT("A node outside the group offers no swap"), Passive && !Passive->CanSwap());

	View->RequestSwapNode(TAG_SkillTreeTest_FormB);
	TestTrue(TEXT("On the server the swap lands at once and nothing waits"),
		FormB && FormB->GetState() == ERpgSkillTreeNodeState::Unlocked && FormB->GetRequestStatus() == ERpgSkillTreeRequestStatus::Idle);
	const URpgSkillTreeNodeViewModel* FormA = View->FindNode(TAG_SkillTreeTest_FormA);
	TestTrue(TEXT("The replaced form now offers the swap back"), FormA && FormA->CanSwap());

	// Passive costs two points and none are free: the server rejects it and the card can say why.
	View->RequestUnlockNode(TAG_SkillTreeTest_Passive);
	TestTrue(TEXT("A rejected request shows its reason"),
		Passive && Passive->GetRequestStatus() == ERpgSkillTreeRequestStatus::Rejected &&
			Passive->GetRejection() == ERpgSkillTreeRequestRejection::NotEnoughPoints);
	TestFalse(TEXT("A rejected request learns nothing"), SkillTrees->IsNodeUnlocked(TreeTag, TAG_SkillTreeTest_Passive));
	View->SelectNode(TAG_SkillTreeTest_Root);
	TestTrue(TEXT("Selecting another node clears the rejection"),
		Passive && Passive->GetRequestStatus() == ERpgSkillTreeRequestStatus::Idle &&
			Passive->GetRejection() == ERpgSkillTreeRequestRejection::None);
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

	const TSoftObjectPtr<UTexture2D> LoggingIcon(FSoftObjectPath(TEXT("/Game/Test/T_LoggingIcon.T_LoggingIcon")));
	Config->TaggedSkillConfigs.FindChecked(Logging).Icon = LoggingIcon;
	TestEqual(TEXT("An authored icon is returned"), TradeSkills->GetSkillIcon(Logging), LoggingIcon);
	TestTrue(TEXT("Skills without an icon return none"), TradeSkills->GetSkillIcon(RpgTradeSkillGameplayTags::Skill_Gathering_Mining).IsNull());
	return true;
}

#endif
