#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameFeatureData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestBurn.h"
#include "Harvesting/RpgHarvestRuneStrike.h"
#include "Harvesting/RpgHarvestChainComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddComponents.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Runes/RpgInventoryFragment_Rune.h"

namespace RpgHarvestContentContractTests
{
	const TCHAR* PickaxeAbilitySetPath = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Pickaxe.AS_Tool_Pickaxe");
	const TCHAR* PickaxeItemClassPath = TEXT("/GF_Harvesting_Magic/Items/Tools/ID_Tool_Pickaxe.ID_Tool_Pickaxe_C");
	const TCHAR* IronVeinClassPath = TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_IronVein.BP_HarvestNode_IronVein_C");
	const TCHAR* IronVeinInstancesClassPath =
		TEXT("/GF_Harvesting_Magic/Harvesting/Instances/BPC_HarvestInstances_IronVein.BPC_HarvestInstances_IronVein_C");
	const TCHAR* IronVeinProfilePath = TEXT("/GF_Harvesting_Magic/Harvesting/Profiles/HP_IronVein.HP_IronVein");
	const TCHAR* HarvestingFeatureDataPath = TEXT("/GF_Harvesting_Magic/GF_Harvesting_Magic.GF_Harvesting_Magic");
	const TCHAR* AxeAbilitySetPath = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Axe.AS_Tool_Axe");
	const TCHAR* AxeItemClassPath = TEXT("/GF_Harvesting_Magic/Items/Tools/ID_Tool_Axe.ID_Tool_Axe_C");
	const TCHAR* DeadPineProfilePath = TEXT("/GF_Harvesting_Magic/Harvesting/Profiles/HP_DeadPine.HP_DeadPine");
	const TCHAR* RootGroveClassPath = TEXT("/GF_Harvesting_Magic/Harvesting/Chains/BP_HarvestRootGrove.BP_HarvestRootGrove_C");
	const TCHAR* DeadPineInstancesClassPath =
		TEXT("/GF_Harvesting_Magic/Harvesting/Instances/BPC_HarvestInstances_DeadPine.BPC_HarvestInstances_DeadPine_C");
	const TCHAR* BrandRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Brand.ID_Rune_Brand_C");
	const TCHAR* EchoRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Echo.ID_Rune_Echo_C");
	const TCHAR* TransferRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Kraftuebertrag.ID_Rune_Kraftuebertrag_C");
	const TCHAR* LeapRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Erntesprung.ID_Rune_Erntesprung_C");
	const TCHAR* PierceRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Durchschlag.ID_Rune_Durchschlag_C");
	const TCHAR* RuneChoiceRewardPath = TEXT("/GF_Harvesting_Magic/Items/Runes/DA_Reward_HarvestingRuneChoice.DA_Reward_HarvestingRuneChoice");
	const TCHAR* IronVeinLootPath = TEXT("/GF_Harvesting_Magic/Loot/LT_IronVein.LT_IronVein");
	const TCHAR* DeadPineLootPath = TEXT("/GF_Harvesting_Magic/Loot/LT_DeadPine.LT_DeadPine");
	const TCHAR* KilnRecipeSetPath = TEXT("/GF_Harvesting_Magic/Crafting/DA_RecipeSet_Kiln.DA_RecipeSet_Kiln");
	const TCHAR* KilnStationClassPath = TEXT("/GF_Harvesting_Magic/Crafting/BP_CraftingStation_Kiln.BP_CraftingStation_Kiln_C");
	const TCHAR* RuneShrineClassPath = TEXT("/GF_Harvesting_Magic/Crafting/BP_CraftingStation_RuneShrine.BP_CraftingStation_RuneShrine_C");
	const TCHAR* RuneEssenceClassPath = TEXT("/GF_Harvesting_Magic/Items/Materials/ID_RuneEssence.ID_RuneEssence_C");
	const TCHAR* AxeChopAbilityClassPath = TEXT("/GF_Harvesting_Magic/GAS/Abilities/GA_Harvest_AxeChop.GA_Harvest_AxeChop_C");
	const TCHAR* PickaxeStrikeAbilityClassPath =
		TEXT("/GF_Harvesting_Magic/GAS/Abilities/GA_Harvest_PickaxeStrike.GA_Harvest_PickaxeStrike_C");

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

	URpgAbilitySystemComponent* MakeTestAbilitySystem(FAutomationTestBase& Test, UWorld* World)
	{
		AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Test.TestNotNull(TEXT("Grant owner spawns"), Owner))
		{
			return nullptr;
		}
		URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
		Owner->AddInstanceComponent(AbilitySystem);
		AbilitySystem->RegisterComponent();
		AbilitySystem->InitAbilityActorInfo(Owner, Owner);
		AbilitySystem->SetForceGrantAuthorityForTests(true);
		return AbilitySystem;
	}

	/** The tool's own set grants only its swing; Q/E/R come from the tool's skill tree. */
	const URpgGameplayAbility_Harvest* ExpectToolSwing(
		FAutomationTestBase& Test,
		URpgAbilitySystemComponent& AbilitySystem,
		const TCHAR* AbilitySetPath,
		const FGameplayTag SwingId)
	{
		const URpgAbilitySet* AbilitySet = LoadObject<URpgAbilitySet>(nullptr, AbilitySetPath);
		if (!Test.TestNotNull(TEXT("The tool ability set loads"), AbilitySet))
		{
			return nullptr;
		}
		FRpgAbilitySet_GrantedHandles Granted;
		AbilitySet->GiveToAbilitySystem(&AbilitySystem, &Granted);

		const FGameplayAbilitySpec* Swing = FindSpecWithId(AbilitySystem, SwingId);
		if (!Test.TestNotNull(*FString::Printf(TEXT("The tool set grants %s"), *SwingId.ToString()), Swing))
		{
			return nullptr;
		}
		Test.TestTrue(
			TEXT("The swing is bound to the primary input"),
			Swing->GetDynamicSpecSourceTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"))));
		for (int32 SlotIndex = 0; SlotIndex < URpgSkillTreeComponent::SlotCount; ++SlotIndex)
		{
			FGameplayTag DefaultId;
			Test.TestFalse(
				*FString::Printf(TEXT("The tool set declares no default for weapon ability slot %d"), SlotIndex + 1),
				URpgWeaponAbilityLoadoutComponent::ResolveDefaultAbilityId(AbilitySystem, SlotIndex, DefaultId) ==
					ERpgAbilityBindingResolveResult::Unique);
		}
		return Cast<URpgGameplayAbility_Harvest>(Swing->Ability);
	}

	/** Returns the tree the tool grants from the main hand. */
	const URpgSkillTreeDefinition* LoadToolSkillTree(
		FAutomationTestBase& Test,
		const TCHAR* ItemClassPath,
		const FGameplayTag ExpectedTreeTag,
		const FGameplayTag ExpectedMasterySkill)
	{
		const UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ItemClassPath);
		const URpgInventoryItemDefinition* Item = ItemClass ? GetDefault<URpgInventoryItemDefinition>(ItemClass) : nullptr;
		const URpgInventoryFragment_SkillTree* Fragment = Item
			? Cast<URpgInventoryFragment_SkillTree>(Item->FindFragmentByClass(URpgInventoryFragment_SkillTree::StaticClass()))
			: nullptr;
		if (!Test.TestNotNull(TEXT("The tool carries a skill tree fragment"), Fragment) ||
			!Test.TestNotNull(TEXT("The fragment names a tree"), Fragment->SkillTree.Get()))
		{
			return nullptr;
		}
		const URpgSkillTreeDefinition* Tree = Fragment->SkillTree;
		Test.TestTrue(TEXT("The tree is in use from the main hand"), Fragment->IsActiveInSlot(ERpgEquipmentSlot::MainHand));
		Test.TestTrue(TEXT("The tree keeps its stable save identity"), Tree->TreeTag == ExpectedTreeTag);
		Test.TestTrue(TEXT("The tree earns points from the tool's gathering skill"), Tree->MasterySkillTag == ExpectedMasterySkill);
		TArray<FText> Errors;
		Test.TestTrue(TEXT("The tree is valid"), Tree->ValidateTree(Errors));
		return Tree;
	}

	/** Paths of the profiles of the resources harvesting tools take; each advances the shared harvesting mastery. */
	const TCHAR* ToolResourceProfilePaths[] = {
		TEXT("/GF_Harvesting_Magic/Harvesting/Profiles/HP_IronVein.HP_IronVein"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Profiles/HP_MiningNode.HP_MiningNode"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Profiles/HP_DeadPine.HP_DeadPine")};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPickaxeToolContractTest,
	"SurvivalRpg.Harvesting.Content.PickaxeToolContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPickaxeToolContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	URpgAbilitySystemComponent* AbilitySystem = MakeTestAbilitySystem(*this, TestWorld.GetWorld());
	if (!AbilitySystem)
	{
		return false;
	}

	// Pickaxe Strike: the swing on the main-hand primary input.
	const URpgGameplayAbility_Harvest* Strike = ExpectToolSwing(
		*this, *AbilitySystem, PickaxeAbilitySetPath, FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.PickaxeStrike")));
	TestTrue(TEXT("Pickaxe Strike executes on press"), Strike && !Strike->IsAimWhileInputHeld());
	TestTrue(TEXT("Pickaxe Strike can strike weak points"), Strike && Strike->CanHitWeakPoints());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSharedTreeContractTest,
	"SurvivalRpg.Harvesting.Content.SharedHarvestingTreeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSharedTreeContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// Axe and pickaxe carry one tree, so they share its learned talents and its budget of shared mastery points.
	const FGameplayTag TreeTag = FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting"));
	const FGameplayTag Mastery = RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting;
	const URpgSkillTreeDefinition* PickaxeTree = LoadToolSkillTree(*this, PickaxeItemClassPath, TreeTag, Mastery);
	const URpgSkillTreeDefinition* AxeTree = LoadToolSkillTree(*this, AxeItemClassPath, TreeTag, Mastery);
	if (!PickaxeTree || !AxeTree)
	{
		return false;
	}
	TestTrue(TEXT("Axe and pickaxe carry the same tree asset"), PickaxeTree == AxeTree);
	const URpgSkillTreeDefinition& Tree = *PickaxeTree;
	TestTrue(TEXT("The shared tree is a free-position map"), Tree.LayoutMode == ERpgSkillTreeLayoutMode::FreePosition);
	TestTrue(TEXT("The shared tree frames a default region"), Tree.FindRegion(Tree.DefaultRegionId) != nullptr);

	// Talents are passive tunings; nothing occupies the tool's Q/E/R anymore. Every branch (region) has one entry
	// talent, and its other talents build on it.
	TMap<FName, const FRpgSkillTreeNode*> Entries;
	TSet<FName> ChoiceGroups;
	for (const FRpgSkillTreeNode& Node : Tree.Nodes)
	{
		const FString Name = Node.NodeTag.ToString();
		TestTrue(*FString::Printf(TEXT("%s grants no ability"), *Name), Node.AbilitySet == nullptr);
		TestFalse(*FString::Printf(TEXT("%s changes harvesting through tunings"), *Name), Node.AbilityTunings.IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s belongs to a branch"), *Name), Tree.FindRegion(Node.RegionId) != nullptr);
		if (Node.Prerequisites.IsEmpty())
		{
			TestFalse(*FString::Printf(TEXT("%s is the only entry of its branch"), *Name), Entries.Contains(Node.RegionId));
			Entries.Add(Node.RegionId, &Node);
		}
		else if (!Node.ExclusiveGroup.IsNone())
		{
			ChoiceGroups.Add(Node.ExclusiveGroup);
		}
	}
	for (const TPair<FName, const FRpgSkillTreeNode*>& Entry : Entries)
	{
		TestTrue(
			*FString::Printf(TEXT("The entry of %s is learnable with the first point"), *Entry.Key.ToString()),
			Entry.Value->Cost <= Tree.GetEarnedPointsForLevel(2));
	}
	for (const FRpgSkillTreeNode& Node : Tree.Nodes)
	{
		const FRpgSkillTreeNode* const* Entry = Entries.Find(Node.RegionId);
		if (!Node.Prerequisites.IsEmpty() && TestNotNull(TEXT("Every branch has an entry talent"), Entry))
		{
			TestTrue(
				*FString::Printf(TEXT("%s builds on its branch's entry"), *Node.NodeTag.ToString()),
				Node.Prerequisites.Contains((*Entry)->NodeTag));
		}
	}
	for (const FName Group : ChoiceGroups)
	{
		int32 Members = 0;
		for (const FRpgSkillTreeNode& Node : Tree.Nodes)
		{
			Members += Node.ExclusiveGroup == Group ? 1 : 0;
		}
		TestTrue(*FString::Printf(TEXT("Choice %s offers alternatives"), *Group.ToString()), Members >= 2);
	}

	// The Aftermath branch works with Brand: its talents sleep without it in the tool.
	const FGameplayTag Brand = FGameplayTag::RequestGameplayTag(TEXT("Rune.Brand"));
	bool bHasAftermath = false;
	for (const FRpgSkillTreeNode& Node : Tree.Nodes)
	{
		if (Node.RegionId == TEXT("Aftermath"))
		{
			bHasAftermath = true;
			TestTrue(*FString::Printf(TEXT("%s needs Brand"), *Node.NodeTag.ToString()), Node.RequiredRunes.HasTagExact(Brand));
		}
	}
	TestTrue(TEXT("The tree has the Aftermath branch"), bHasAftermath);
	TestFalse(TEXT("The tree offers an exclusive choice"), ChoiceGroups.IsEmpty());

	// Every resource of a harvesting tool advances the shared mastery once per section.
	for (const TCHAR* ProfilePath : ToolResourceProfilePaths)
	{
		const URpgHarvestProfile* Profile = LoadObject<URpgHarvestProfile>(nullptr, ProfilePath);
		if (TestNotNull(*FString::Printf(TEXT("%s loads"), ProfilePath), Profile))
		{
			TestTrue(
				*FString::Printf(TEXT("%s requires a harvesting tool"), *GetNameSafe(Profile)),
				Profile->RequiredToolTag.IsValid());
			TestTrue(
				*FString::Printf(TEXT("%s awards shared mastery XP"), *GetNameSafe(Profile)),
				Profile->MasterySkillTag == Mastery && Profile->MasteryExperience > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestIronVeinWeakPointContractTest,
	"SurvivalRpg.Harvesting.Content.IronVeinWeakPointContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestIronVeinWeakPointContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	UClass* VeinClass = LoadClass<AActor>(nullptr, IronVeinClassPath);
	if (!TestNotNull(TEXT("The iron vein class loads"), VeinClass))
	{
		return false;
	}

	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Vein = World ? World->SpawnActor<AActor>(VeinClass, FTransform(FVector(500.0, 0.0, 0.0)), SpawnParameters) : nullptr;
	if (Vein && !Vein->HasActorBegunPlay())
	{
		Vein->DispatchBeginPlay();
	}
	const URpgHarvestableComponent* Node = Vein ? Vein->FindComponentByClass<URpgHarvestableComponent>() : nullptr;
	if (!TestNotNull(TEXT("The iron vein has a harvestable node"), Node) ||
		!TestNotNull(TEXT("The iron vein has a harvest profile"), Node->GetHarvestProfile()))
	{
		return false;
	}

	if (Node->GetHarvestProfile()->GetClampedWeakPointBonusSections() <= 0)
	{
		AddInfo(TEXT("The iron vein profile awards no weak point bonus."));
		return true;
	}
	FVector WeakPoint;
	float Radius = 0.0f;
	if (!TestTrue(TEXT("A stocked vein with a weak point bonus has an active weak point"), Node->GetActiveWeakPoint(WeakPoint, Radius)))
	{
		return false;
	}
	// Colliding components only: the swing has to reach the weak point on the resource, not on a cosmetic marker.
	const FBox Bounds = Vein->GetComponentsBoundingBox(false).ExpandBy(Radius);
	TestTrue(TEXT("The active weak point lies on the vein's collision"), Bounds.IsValid && Bounds.IsInside(WeakPoint));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPcgIronVeinContractTest,
	"SurvivalRpg.Harvesting.Content.PcgIronVeinInstancesContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPcgIronVeinContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// The PCG spawner's component class: every spawned instance is a harvestable iron vein.
	UClass* InstancesClass = LoadClass<URpgHarvestableInstancesComponent>(nullptr, IronVeinInstancesClassPath);
	if (!TestNotNull(TEXT("The iron vein instances class loads"), InstancesClass))
	{
		return false;
	}
	const URpgHarvestableInstancesComponent* Instances = GetDefault<URpgHarvestableInstancesComponent>(InstancesClass);
	const URpgHarvestProfile* Profile = Instances ? Instances->GetHarvestProfile() : nullptr;
	if (!TestNotNull(TEXT("The iron vein instances have a harvest profile"), Profile))
	{
		return false;
	}
	TestTrue(
		TEXT("The instances share the actor veins' profile"),
		Profile->GetPathName() == FString(IronVeinProfilePath));
	TestTrue(
		TEXT("The instances require the pickaxe"),
		Profile->RequiredToolTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Tool.Harvesting.Pickaxe"))));

	// The harvesting GameFeature adds the replicated instance stock to the GameState on server and clients.
	const UGameFeatureData* FeatureData = LoadObject<UGameFeatureData>(nullptr, HarvestingFeatureDataPath);
	if (!TestNotNull(TEXT("The harvesting GameFeature data loads"), FeatureData))
	{
		return false;
	}
	int32 StockRegistrations = 0;
	for (const UGameFeatureAction* Action : FeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddComponents* AddComponents = Cast<URpgGameFeatureAction_AddComponents>(Action);
		if (!AddComponents)
		{
			continue;
		}
		for (const FRpgGameFeatureComponentEntry& Entry : AddComponents->ComponentList)
		{
			if (Entry.ComponentClass.ToSoftObjectPath() != FSoftObjectPath(URpgHarvestInstanceStockComponent::StaticClass()))
			{
				continue;
			}
			++StockRegistrations;
			const UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
			TestTrue(TEXT("The instance stock targets a GameState"), ActorClass && ActorClass->IsChildOf<AGameStateBase>());
			TestTrue(TEXT("The server creates the instance stock"), Entry.bServerComponent);
		}
	}
	TestEqual(TEXT("The GameFeature registers the instance stock exactly once"), StockRegistrations, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestAxeToolContractTest,
	"SurvivalRpg.Harvesting.Content.AxeToolContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAxeToolContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	URpgAbilitySystemComponent* AbilitySystem = MakeTestAbilitySystem(*this, TestWorld.GetWorld());
	if (!AbilitySystem)
	{
		return false;
	}

	// Axe Chop: the swing on the main-hand primary input.
	const FGameplayTag ChopId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.AxeChop"));
	const URpgGameplayAbility_Harvest* Chop = ExpectToolSwing(*this, *AbilitySystem, AxeAbilitySetPath, ChopId);
	TestTrue(TEXT("Axe Chop executes on press"), Chop && !Chop->IsAimWhileInputHeld());
	TestTrue(TEXT("Axe Chop sends its own ability id"), Chop && Chop->GetHarvestAbilityId() == ChopId);

	// The axe item supplies the tool category that dead pines require.
	const UClass* AxeClass = LoadClass<URpgInventoryItemDefinition>(nullptr, AxeItemClassPath);
	const URpgInventoryItemDefinition* Axe = AxeClass ? GetDefault<URpgInventoryItemDefinition>(AxeClass) : nullptr;
	const URpgInventoryFragment_HarvestingTool* AxeTool = Axe
		? Cast<URpgInventoryFragment_HarvestingTool>(Axe->FindFragmentByClass(URpgInventoryFragment_HarvestingTool::StaticClass()))
		: nullptr;
	const URpgHarvestProfile* DeadPine = LoadObject<URpgHarvestProfile>(nullptr, DeadPineProfilePath);
	if (!TestNotNull(TEXT("The axe is a harvesting tool"), AxeTool) || !TestNotNull(TEXT("The dead pine profile loads"), DeadPine))
	{
		return false;
	}
	TestTrue(TEXT("Dead pines require a tool"), DeadPine->RequiredToolTag.IsValid());
	TestTrue(TEXT("The axe meets the dead pine's tool requirement"), AxeTool->ToolTag.MatchesTag(DeadPine->RequiredToolTag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPcgDeadPineContractTest,
	"SurvivalRpg.Harvesting.Content.PcgDeadPineInstancesContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPcgDeadPineContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// The PCG trunk proxies: every instance is a harvestable dead pine that presents a linked visible tree.
	UClass* InstancesClass = LoadClass<URpgHarvestableInstancesComponent>(nullptr, DeadPineInstancesClassPath);
	if (!TestNotNull(TEXT("The dead pine instances class loads"), InstancesClass))
	{
		return false;
	}
	const URpgHarvestableInstancesComponent* Instances = GetDefault<URpgHarvestableInstancesComponent>(InstancesClass);
	const URpgHarvestProfile* Profile = Instances ? Instances->GetHarvestProfile() : nullptr;
	if (!TestNotNull(TEXT("The dead pine instances have a harvest profile"), Profile))
	{
		return false;
	}
	TestTrue(TEXT("The instances use the dead pine profile"), Profile->GetPathName() == FString(DeadPineProfilePath));
	TestFalse(TEXT("The trunk proxies link visible trees"), Instances->GetLinkedPresentationTag().IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRootGroveContractTest,
	"SurvivalRpg.Harvesting.Content.RootGroveChainContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRootGroveContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// The root grove that portal realms place over tree groups: an actor with exactly one chain box.
	const UBlueprintGeneratedClass* GroveClass = Cast<UBlueprintGeneratedClass>(LoadClass<AActor>(nullptr, RootGroveClassPath));
	if (!TestNotNull(TEXT("The root grove class loads"), GroveClass) ||
		!TestNotNull(TEXT("The root grove has components"), GroveClass->SimpleConstructionScript.Get()))
	{
		return false;
	}
	TArray<const URpgHarvestChainComponent*> Chains;
	for (const USCS_Node* Node : GroveClass->SimpleConstructionScript->GetAllNodes())
	{
		if (const URpgHarvestChainComponent* Chain = Node ? Cast<URpgHarvestChainComponent>(Node->ComponentTemplate) : nullptr)
		{
			Chains.Add(Chain);
		}
	}
	if (!TestEqual(TEXT("The root grove has one chain box"), Chains.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("The chain takes resources"), Chains[0]->GetMaxChainedTargets() >= 1);
	TestTrue(TEXT("The grove falls tree by tree"), Chains[0]->GetChainSpeed() > 0.0f);
	TestTrue(TEXT("The chain box has a size"), !Chains[0]->GetUnscaledBoxExtent().IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRuneSocketContractTest,
	"SurvivalRpg.Harvesting.Content.RuneSocketContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRuneSocketContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// A harvesting rune is a rank I to III item instance; Brand is the first one.
	UClass* BrandClass = LoadClass<URpgInventoryItemDefinition>(nullptr, BrandRuneClassPath);
	const URpgInventoryFragment_Rune* Brand = BrandClass
		? Cast<URpgInventoryFragment_Rune>(GetDefault<URpgInventoryItemDefinition>(BrandClass)->FindFragmentByClass(
			URpgInventoryFragment_Rune::StaticClass()))
		: nullptr;
	if (!TestNotNull(TEXT("Brand is a rune item"), Brand))
	{
		return false;
	}
	TestTrue(TEXT("Brand has its rune identity"), Brand->RuneTag.IsValid());
	TestEqual(TEXT("Brand reaches rank III"), Brand->GetMaxRank(), RpgRuneMaxRank);

	// Every rank of Brand lights a burn, and a higher rank never burns weaker.
	float PreviousBurnWork = 0.0f;
	for (int32 Rank = 1; Rank <= Brand->GetMaxRank(); ++Rank)
	{
		FRpgAbilityTuningAccumulator BurnWork;
		FRpgAbilityTuningAccumulator BurnDuration;
		for (const FRpgSkillTreeAbilityTuning& Tuning : Brand->FindRank(Rank)->AbilityTunings)
		{
			BurnWork.Apply(Tuning, FGameplayTagContainer(), RpgHarvestingMagicGameplayTags::Ability_Tuning_Harvest_BurnWork);
			BurnDuration.Apply(Tuning, FGameplayTagContainer(), RpgHarvestingMagicGameplayTags::Ability_Tuning_Harvest_BurnDuration);
		}
		TestTrue(*FString::Printf(TEXT("Brand rank %d burns"), Rank), BurnWork.Resolve(0.0f) > 0.0f && BurnDuration.Resolve(0.0f) > 0.0f);
		TestTrue(*FString::Printf(TEXT("Brand rank %d burns at least as hard as the rank below"), Rank), BurnWork.Resolve(0.0f) >= PreviousBurnWork);
		PreviousBurnWork = BurnWork.Resolve(0.0f);
	}

	// The axe and pickaxe swings light a burn that names Brand as its effect.
	for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
		const TSubclassOf<ARpgHarvestBurn> BurnClass =
			SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass)->GetBurnClass() : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("%s lights a burn"), SwingPath), BurnClass.Get()))
		{
			TestTrue(
				*FString::Printf(TEXT("%s burns as Brand"), SwingPath),
				GetDefault<ARpgHarvestBurn>(BurnClass)->GetProcTag() == Brand->RuneTag);
		}
	}

	// Axe and pickaxe each carry three equal sockets that take harvesting runes and store nothing else.
	for (const TCHAR* ToolPath : {AxeItemClassPath, PickaxeItemClassPath})
	{
		UClass* ToolClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ToolPath);
		const URpgInventoryFragment_ItemContainer* Containers = ToolClass
			? Cast<URpgInventoryFragment_ItemContainer>(GetDefault<URpgInventoryItemDefinition>(ToolClass)->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s provides item-owned grids"), ToolPath), Containers))
		{
			continue;
		}
		TArray<FRpgInventoryItemContainerDefinition> Grids;
		Containers->GetProvidedContainers(Grids);
		TestTrue(*FString::Printf(TEXT("%s is no storage container"), ToolPath), !Containers->ProvidesStorageContainers());
		TestEqual(*FString::Printf(TEXT("%s has one socket grid"), ToolPath), Grids.Num(), 1);
		if (Grids.Num() == 1)
		{
			const FRpgInventoryItemContainerDefinition& Sockets = Grids[0];
			TestTrue(*FString::Printf(TEXT("%s sockets are a socket grid"), ToolPath), Sockets.bSocketGrid);
			TestEqual(*FString::Printf(TEXT("%s has three sockets"), ToolPath), Sockets.GridSize.Width * Sockets.GridSize.Height, 3);
			TestTrue(*FString::Printf(TEXT("%s sockets take Brand"), ToolPath), Sockets.AllowsItemDefinition(BrandClass, 1));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRuneRecipeContractTest,
	"SurvivalRpg.Harvesting.Content.RuneRecipeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRuneRecipeContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// Finding Brand teaches its locked base recipe, which crafts a new Brand at the Kiln.
	UClass* BrandClass = LoadClass<URpgInventoryItemDefinition>(nullptr, BrandRuneClassPath);
	const URpgInventoryFragment_Rune* Brand = BrandClass
		? Cast<URpgInventoryFragment_Rune>(GetDefault<URpgInventoryItemDefinition>(BrandClass)->FindFragmentByClass(
			URpgInventoryFragment_Rune::StaticClass()))
		: nullptr;
	const URpgCraftingRecipeDefinition* Recipe = Brand ? Brand->BaseRecipe.Get() : nullptr;
	if (!TestNotNull(TEXT("Brand names its base recipe"), Recipe))
	{
		return false;
	}
	TestFalse(TEXT("The base recipe is locked until Brand is found"), Recipe->bUnlockedByDefault);
	TestTrue(
		TEXT("The base recipe unlocks through a Recipe tag"),
		Recipe->RecipeUnlockTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Recipe"))));
	TestTrue(
		TEXT("The base recipe crafts one Brand"),
		Recipe->OutputItems.Num() == 1 && Recipe->OutputItems[0].ItemDefinition == BrandClass && Recipe->OutputItems[0].Count == 1);
	TestFalse(TEXT("A new Brand costs materials"), Recipe->RequiredResources.IsEmpty());

	const URpgCraftingRecipeSet* KilnRecipes = LoadObject<URpgCraftingRecipeSet>(nullptr, KilnRecipeSetPath);
	TestTrue(TEXT("The Kiln offers the base recipe"), KilnRecipes && KilnRecipes->Recipes.Contains(Recipe));
	UClass* KilnClass = LoadClass<ARpgCraftingStationActor>(nullptr, KilnStationClassPath);
	const URpgCraftingStationComponent* Kiln =
		KilnClass ? GetDefault<ARpgCraftingStationActor>(KilnClass)->GetCraftingStationComponent() : nullptr;
	TestTrue(
		TEXT("The Kiln meets the recipe's station tags"),
		Kiln && Kiln->GetStationTags().HasAllExact(Recipe->RequiredStationTags));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRuneRewardContractTest,
	"SurvivalRpg.Harvesting.Content.RuneRewardContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRuneRewardContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	UClass* BrandClass = LoadClass<URpgInventoryItemDefinition>(nullptr, BrandRuneClassPath);
	if (!TestNotNull(TEXT("Brand loads"), BrandClass))
	{
		return false;
	}

	// The shared harvesting tree guarantees a rune of the player's choice at a mastery milestone.
	const FGameplayTag TreeTag = FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting"));
	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(*this, PickaxeItemClassPath, TreeTag, RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting);
	if (!Tree)
	{
		return false;
	}
	const FRpgSkillTreeMasteryMilestone* RuneMilestone = Tree->MasteryMilestones.FindByPredicate(
		[BrandClass](const FRpgSkillTreeMasteryMilestone& Milestone)
		{
			return Milestone.Reward && Milestone.Reward->IsChoice() &&
				Milestone.Reward->Options.ContainsByPredicate([BrandClass](const FRpgRewardClaimOption& Option)
				{
					return Option.ItemDefinition == BrandClass;
				});
		});
	if (TestNotNull(TEXT("A harvesting milestone offers Brand as a choice"), RuneMilestone))
	{
		TestTrue(TEXT("The rune milestone reward is valid"), RuneMilestone->Reward->HasValidConfiguration());
		TestTrue(TEXT("The rune milestone lies above the first level"), RuneMilestone->MasteryLevel >= 2);
	}

	// Veins and pines can drop Brand as a rare find.
	for (const TCHAR* LootPath : {IronVeinLootPath, DeadPineLootPath})
	{
		const URpgLootTable* Loot = LoadObject<URpgLootTable>(nullptr, LootPath);
		bool bDropsBrandRarely = false;
		for (const FRpgLootGroup& Group : Loot ? Loot->Groups : TArray<FRpgLootGroup>())
		{
			for (const FRpgLootEntry& Entry : Group.Entries)
			{
				bDropsBrandRarely |= Entry.ItemDefinition == BrandClass && Group.Mode == ERpgLootGroupMode::Independent &&
					Entry.ChancePercent > 0.0f && Entry.ChancePercent < 100.0f;
			}
		}
		TestTrue(*FString::Printf(TEXT("%s drops Brand as a rare find"), LootPath), bDropsBrandRarely);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRuneWorkContractTest,
	"SurvivalRpg.Harvesting.Content.RuneWorkContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRuneWorkContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// Brand's higher ranks cost rune essence at the Runenschrein, and every rank salvages into essence.
	UClass* BrandClass = LoadClass<URpgInventoryItemDefinition>(nullptr, BrandRuneClassPath);
	UClass* EssenceClass = LoadClass<URpgInventoryItemDefinition>(nullptr, RuneEssenceClassPath);
	const URpgInventoryFragment_Rune* Brand = BrandClass
		? Cast<URpgInventoryFragment_Rune>(GetDefault<URpgInventoryItemDefinition>(BrandClass)->FindFragmentByClass(
			URpgInventoryFragment_Rune::StaticClass()))
		: nullptr;
	if (!TestNotNull(TEXT("Brand is a rune"), Brand) || !TestNotNull(TEXT("Rune essence loads"), EssenceClass))
	{
		return false;
	}
	auto CountOfEssence = [EssenceClass](const auto& Entries)
	{
		int32 Count = 0;
		for (const auto& Entry : Entries)
		{
			Count += Entry.ItemDefinition == EssenceClass ? Entry.Count : 0;
		}
		return Count;
	};
	int32 PreviousSalvage = 0;
	for (int32 Rank = 1; Rank <= Brand->GetMaxRank(); ++Rank)
	{
		const FRpgRuneRank* RankData = Brand->FindRank(Rank);
		if (Rank > 1)
		{
			TestTrue(*FString::Printf(TEXT("Brand rank %d costs rune essence"), Rank), CountOfEssence(RankData->UpgradeCosts) > 0);
		}
		const int32 Salvage = CountOfEssence(RankData->SalvageOutputs);
		TestTrue(*FString::Printf(TEXT("Brand rank %d salvages into more essence than the rank below"), Rank), Salvage > PreviousSalvage);
		PreviousSalvage = Salvage;
	}

	// The Runenschrein offers rune work and no recipes of its own.
	UClass* ShrineClass = LoadClass<ARpgCraftingStationActor>(nullptr, RuneShrineClassPath);
	const URpgCraftingStationComponent* Shrine =
		ShrineClass ? GetDefault<ARpgCraftingStationActor>(ShrineClass)->GetCraftingStationComponent() : nullptr;
	TestTrue(TEXT("The Runenschrein offers rune work"), Shrine && Shrine->OffersRuneWork());
	TestTrue(TEXT("The Runenschrein opens its rune screen"), Shrine &&
		Shrine->GetScreenTag() == FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.RuneShrine"), false));
	return true;
}

namespace RpgHarvestContentContractTests
{
	/**
	 * Checks one harvesting rune of the catalog: its identity, ranks I to III that each tune TuningTag and are never
	 * weaker than the rank below, Runenschrein costs and salvage, a locked Kiln recipe that forges it, the rune choice,
	 * the pickaxe sockets and rare vein and pine drops.
	 */
	void TestHarvestingRune(FAutomationTestBase& Test, const TCHAR* ClassPath, const TCHAR* RuneTag, const FGameplayTag TuningTag)
	{
		const URpgCraftingRecipeSet* KilnRecipes = LoadObject<URpgCraftingRecipeSet>(nullptr, KilnRecipeSetPath);
		const URpgRewardClaimDefinition* RuneChoice = LoadObject<URpgRewardClaimDefinition>(nullptr, RuneChoiceRewardPath);
		UClass* PickaxeClass = LoadClass<URpgInventoryItemDefinition>(nullptr, PickaxeItemClassPath);
		const URpgInventoryFragment_ItemContainer* PickaxeContainers = PickaxeClass
			? Cast<URpgInventoryFragment_ItemContainer>(GetDefault<URpgInventoryItemDefinition>(PickaxeClass)->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
			: nullptr;
		TArray<FRpgInventoryItemContainerDefinition> PickaxeGrids;
		if (PickaxeContainers)
		{
			PickaxeContainers->GetProvidedContainers(PickaxeGrids);
		}

		UClass* RuneClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ClassPath);
		const URpgInventoryFragment_Rune* Rune = RuneClass
			? Cast<URpgInventoryFragment_Rune>(GetDefault<URpgInventoryItemDefinition>(RuneClass)->FindFragmentByClass(
				URpgInventoryFragment_Rune::StaticClass()))
			: nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s is a rune item"), RuneTag), Rune))
		{
			return;
		}
		Test.TestTrue(*FString::Printf(TEXT("%s has its rune identity"), RuneTag),
			Rune->RuneTag == FGameplayTag::RequestGameplayTag(RuneTag, false));
		Test.TestEqual(*FString::Printf(TEXT("%s reaches rank III"), RuneTag), Rune->GetMaxRank(), RpgRuneMaxRank);
		float Previous = 0.0f;
		for (int32 Rank = 1; Rank <= Rune->GetMaxRank(); ++Rank)
		{
			const FRpgRuneRank* RankData = Rune->FindRank(Rank);
			FRpgAbilityTuningAccumulator Effect;
			for (const FRpgSkillTreeAbilityTuning& Tuning : RankData->AbilityTunings)
			{
				Effect.Apply(Tuning, FGameplayTagContainer(), TuningTag);
			}
			Test.TestTrue(*FString::Printf(TEXT("%s rank %d works"), RuneTag, Rank), Effect.Resolve(0.0f) > 0.0f);
			Test.TestTrue(*FString::Printf(TEXT("%s rank %d is at least as strong as the rank below"), RuneTag, Rank), Effect.Resolve(0.0f) >= Previous);
			Previous = Effect.Resolve(0.0f);
			Test.TestTrue(*FString::Printf(TEXT("%s rank %d salvages into something"), RuneTag, Rank), !RankData->SalvageOutputs.IsEmpty());
			if (Rank > 1)
			{
				Test.TestFalse(*FString::Printf(TEXT("%s rank %d has an upgrade cost"), RuneTag, Rank), RankData->UpgradeCosts.IsEmpty());
			}
		}
		const URpgCraftingRecipeDefinition* Recipe = Rune->BaseRecipe;
		Test.TestTrue(
			*FString::Printf(TEXT("%s has a locked base recipe that forges it"), RuneTag),
			Recipe && !Recipe->bUnlockedByDefault && Recipe->RecipeUnlockTag.IsValid() &&
				Recipe->OutputItems.ContainsByPredicate([RuneClass](const FRpgCraftingOutputItem& Output)
				{
					return Output.ItemDefinition == RuneClass;
				}));
		Test.TestTrue(*FString::Printf(TEXT("The Kiln offers the %s recipe"), RuneTag), KilnRecipes && KilnRecipes->Recipes.Contains(Recipe));
		Test.TestTrue(
			*FString::Printf(TEXT("The rune choice offers %s"), RuneTag),
			RuneChoice && RuneChoice->Options.ContainsByPredicate([RuneClass](const FRpgRewardClaimOption& Option)
			{
				return Option.ItemDefinition == RuneClass;
			}));
		Test.TestTrue(
			*FString::Printf(TEXT("The pickaxe sockets take %s"), RuneTag),
			PickaxeGrids.Num() == 1 && PickaxeGrids[0].AllowsItemDefinition(RuneClass, 1));
		for (const TCHAR* LootPath : {IronVeinLootPath, DeadPineLootPath})
		{
			const URpgLootTable* Loot = LoadObject<URpgLootTable>(nullptr, LootPath);
			bool bDropsRarely = false;
			for (const FRpgLootGroup& Group : Loot ? Loot->Groups : TArray<FRpgLootGroup>())
			{
				for (const FRpgLootEntry& Entry : Group.Entries)
				{
					bDropsRarely |= Entry.ItemDefinition == RuneClass && Group.Mode == ERpgLootGroupMode::Independent &&
						Entry.ChancePercent > 0.0f && Entry.ChancePercent < 100.0f;
				}
			}
			Test.TestTrue(*FString::Printf(TEXT("%s drops %s as a rare find"), LootPath, RuneTag), bDropsRarely);
		}
	}

	/** Checks that both harvesting swings send StrikeClass strikes named after RuneTag; Describe names the effect. */
	void TestSwingStrikes(
		FAutomationTestBase& Test,
		TSubclassOf<ARpgHarvestRuneStrike> (URpgGameplayAbility_Harvest::*GetStrikeClass)() const,
		const TCHAR* RuneTag,
		const TCHAR* Describe)
	{
		for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
		{
			UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
			const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
			const TSubclassOf<ARpgHarvestRuneStrike> StrikeClass = Swing ? (Swing->*GetStrikeClass)() : nullptr;
			Test.TestTrue(
				*FString::Printf(TEXT("%s %s as %s"), SwingPath, Describe, RuneTag),
				StrikeClass && GetDefault<ARpgHarvestRuneStrike>(StrikeClass)->GetProcTag() == FGameplayTag::RequestGameplayTag(RuneTag, false));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRuneFlowContractTest,
	"SurvivalRpg.Harvesting.Content.RuneFlowContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRuneFlowContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Echo and Kraftuebertrag are rank I to III runes, found, forged, upgraded and salvaged like Brand.
	TestHarvestingRune(*this, EchoRuneClassPath, TEXT("Rune.Echo"), Ability_Tuning_Harvest_EchoWork);
	TestHarvestingRune(*this, TransferRuneClassPath, TEXT("Rune.Kraftuebertrag"), Ability_Tuning_Harvest_TransferWork);

	// The axe and pickaxe swings echo and transfer with strikes that name their runes.
	TestSwingStrikes(*this, &URpgGameplayAbility_Harvest::GetEchoStrikeClass, TEXT("Rune.Echo"), TEXT("echoes"));
	TestSwingStrikes(*this, &URpgGameplayAbility_Harvest::GetTransferStrikeClass, TEXT("Rune.Kraftuebertrag"), TEXT("transfers"));

	// The Rune Flow branch needs Echo or Kraftuebertrag in the tool; its two choices exclude each other.
	const FGameplayTag Mastery = RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting;
	const URpgSkillTreeDefinition* Tree =
		LoadToolSkillTree(*this, PickaxeItemClassPath, FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting")), Mastery);
	if (!Tree)
	{
		return false;
	}
	FGameplayTagContainer FlowRunes;
	FlowRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Echo"), false));
	FlowRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Kraftuebertrag"), false));
	int32 FlowNodes = 0;
	TSet<FName> FlowChoices;
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (Node.RegionId != TEXT("RuneFlow"))
		{
			continue;
		}
		++FlowNodes;
		TestTrue(*FString::Printf(TEXT("%s needs Echo or Kraftuebertrag"), *Node.NodeTag.ToString()),
			Node.RequiredRunes == FlowRunes && Node.RuneMatch == ERpgSkillTreeRuneMatch::AnyRune);
		if (!Node.ExclusiveGroup.IsNone())
		{
			FlowChoices.Add(Node.ExclusiveGroup);
		}
	}
	TestEqual(TEXT("Rune Flow has an entry and two choices"), FlowNodes, 3);
	TestEqual(TEXT("The two Rune Flow choices share one group"), FlowChoices.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSpreadContractTest,
	"SurvivalRpg.Harvesting.Content.SpreadContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSpreadContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Erntesprung and Durchschlag are rank I to III runes, found, forged, upgraded and salvaged like Brand.
	TestHarvestingRune(*this, LeapRuneClassPath, TEXT("Rune.Erntesprung"), Ability_Tuning_Harvest_LeapWork);
	TestHarvestingRune(*this, PierceRuneClassPath, TEXT("Rune.Durchschlag"), Ability_Tuning_Harvest_PierceWork);
	TestSwingStrikes(*this, &URpgGameplayAbility_Harvest::GetLeapStrikeClass, TEXT("Rune.Erntesprung"), TEXT("leaps"));
	TestSwingStrikes(*this, &URpgGameplayAbility_Harvest::GetPierceStrikeClass, TEXT("Rune.Durchschlag"), TEXT("pierces"));

	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(
		*this,
		PickaxeItemClassPath,
		FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting")),
		RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting);
	if (!Tree)
	{
		return false;
	}

	// The Spread branch needs Erntesprung or Durchschlag in the tool; its two choices exclude each other.
	FGameplayTagContainer SpreadRunes;
	SpreadRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Erntesprung"), false));
	SpreadRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Durchschlag"), false));
	int32 SpreadNodes = 0;
	TSet<FName> SpreadChoices;
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (Node.RegionId != TEXT("Spread"))
		{
			continue;
		}
		++SpreadNodes;
		TestTrue(*FString::Printf(TEXT("%s needs Erntesprung or Durchschlag"), *Node.NodeTag.ToString()),
			Node.RequiredRunes == SpreadRunes && Node.RuneMatch == ERpgSkillTreeRuneMatch::AnyRune);
		if (!Node.ExclusiveGroup.IsNone())
		{
			SpreadChoices.Add(Node.ExclusiveGroup);
		}
	}
	TestEqual(TEXT("Spread has an entry and two choices"), SpreadNodes, 3);
	TestEqual(TEXT("The two Spread choices share one group"), SpreadChoices.Num(), 1);

	// Wandering Effect needs Brand and Erntesprung, and it is the alternative to Discharge.
	const FRpgSkillTreeNode* Discharge = Tree->Nodes.FindByPredicate([](const FRpgSkillTreeNode& Node)
	{
		return Node.NodeTag == FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.Harvesting.Aftermath.Discharge"), false);
	});
	const FRpgSkillTreeNode* Wander = Tree->Nodes.FindByPredicate([](const FRpgSkillTreeNode& Node)
	{
		return Node.NodeTag == FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Node.Harvesting.Aftermath.Wander"), false);
	});
	if (!TestNotNull(TEXT("Discharge exists"), Discharge) || !TestNotNull(TEXT("Wandering Effect exists"), Wander))
	{
		return false;
	}
	FGameplayTagContainer WanderRunes;
	WanderRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Brand"), false));
	WanderRunes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.Erntesprung"), false));
	TestTrue(TEXT("Wandering Effect needs Brand and Erntesprung"),
		Wander->RequiredRunes == WanderRunes && Wander->RuneMatch == ERpgSkillTreeRuneMatch::AllRunes);
	TestTrue(TEXT("Wandering Effect and Discharge exclude each other"),
		!Wander->ExclusiveGroup.IsNone() && Wander->ExclusiveGroup == Discharge->ExclusiveGroup);
	TestTrue(TEXT("Wandering Effect follows Lasting Power"), Wander->Prerequisites == Discharge->Prerequisites);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
