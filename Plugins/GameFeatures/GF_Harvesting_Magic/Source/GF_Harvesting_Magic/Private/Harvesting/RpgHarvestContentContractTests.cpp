#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameFeatureData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddComponents.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"

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
	const TCHAR* DeadPineInstancesClassPath =
		TEXT("/GF_Harvesting_Magic/Harvesting/Instances/BPC_HarvestInstances_DeadPine.BPC_HarvestInstances_DeadPine_C");

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

	/** Returns the node of Tree whose ability set grants AbilityId, or null. */
	const FRpgSkillTreeNode* FindNodeTeaching(const URpgSkillTreeDefinition& Tree, const FGameplayTag AbilityId)
	{
		return Tree.Nodes.FindByPredicate([AbilityId](const FRpgSkillTreeNode& Candidate)
		{
			TArray<FGameplayTag> AbilityIds;
			Candidate.GetGrantedAbilityIds(AbilityIds);
			return AbilityIds.Contains(AbilityId);
		});
	}

	/**
	 * Expects Node's own ability set to grant AbilityId without an input tag, so the tree places it on Q/E/R, from the
	 * harvest ability base with its own id, a cooldown and no skill-level gate of its own.
	 */
	const URpgGameplayAbility_Harvest* ExpectNodeGrantsAbility(
		FAutomationTestBase& Test,
		const FRpgSkillTreeNode& Node,
		URpgAbilitySystemComponent& AbilitySystem,
		const FGameplayTag AbilityId)
	{
		const FString Name = AbilityId.ToString();
		for (const FRpgAbilitySet_GameplayAbility& Entry : Node.AbilitySet->GetGrantedGameplayAbilities())
		{
			if (Entry.AbilityIdTag == AbilityId)
			{
				Test.TestFalse(*FString::Printf(TEXT("%s has no input tag; the tree places it"), *Name), Entry.InputTag.IsValid());
			}
		}

		FRpgAbilitySet_GrantedHandles Granted;
		Node.AbilitySet->GiveToAbilitySystem(&AbilitySystem, &Granted);
		const FGameplayAbilitySpec* Spec = FindSpecWithId(AbilitySystem, AbilityId);
		const URpgGameplayAbility_Harvest* Ability = Spec ? Cast<URpgGameplayAbility_Harvest>(Spec->Ability) : nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("The node grants %s from the harvest ability base"), *Name), Ability))
		{
			return nullptr;
		}
		Test.TestTrue(
			*FString::Printf(TEXT("The %s id resolves to exactly one granted spec"), *Name),
			FRpgAbilityBindingResolver::ResolveUniqueAbilityId(&AbilitySystem, AbilityId).Result ==
				ERpgAbilityBindingResolveResult::Unique);
		Test.TestTrue(*FString::Printf(TEXT("%s sends its own ability id"), *Name), Ability->GetHarvestAbilityId() == AbilityId);
		Test.TestTrue(*FString::Printf(TEXT("%s has a cooldown"), *Name), Ability->GetCooldownGameplayEffect() != nullptr);
		const FStructProperty* GateProperty =
			FindFProperty<FStructProperty>(URpgGameplayAbility_Harvest::StaticClass(), TEXT("RequiredSkillTag"));
		const FGameplayTag* Gate = GateProperty ? GateProperty->ContainerPtrToValuePtr<FGameplayTag>(Ability) : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("The tree, not a skill level, unlocks %s"), *Name), Gate && !Gate->IsValid());
		return Ability;
	}

	/**
	 * Expects a node of Tree that teaches AbilityId with the first point (skill level 2): its own ability set grants the
	 * ability without an input tag, so the tree places it on Q/E/R, and the ability has no skill-level gate of its own.
	 */
	const URpgGameplayAbility_Harvest* ExpectFirstPointActive(
		FAutomationTestBase& Test,
		const URpgSkillTreeDefinition& Tree,
		URpgAbilitySystemComponent& AbilitySystem,
		const FGameplayTag AbilityId)
	{
		const FRpgSkillTreeNode* Node = FindNodeTeaching(Tree, AbilityId);
		const FString Name = AbilityId.ToString();
		if (!Test.TestNotNull(*FString::Printf(TEXT("A node teaches %s"), *Name), Node))
		{
			return nullptr;
		}
		Test.TestTrue(
			*FString::Printf(TEXT("%s is learnable with the first point"), *Name),
			Node->Prerequisites.IsEmpty() && Node->RequiredPointsInTree == 0 && Node->Cost <= Tree.GetEarnedPointsForLevel(2));
		Test.TestTrue(
			*FString::Printf(TEXT("%s presents as an active node"), *Name),
			Node->KindTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("SkillTree.NodeKind.Active"))));
		return ExpectNodeGrantsAbility(Test, *Node, AbilitySystem, AbilityId);
	}

	/**
	 * Expects the ultimate node of Tree that teaches AbilityId: it presents as an ultimate and keeps a point gate, which
	 * the tree's other nodes can fill, and its ability set grants the ability like any active node.
	 */
	const URpgGameplayAbility_Harvest* ExpectUltimate(
		FAutomationTestBase& Test,
		const URpgSkillTreeDefinition& Tree,
		URpgAbilitySystemComponent& AbilitySystem,
		const FGameplayTag AbilityId)
	{
		const FRpgSkillTreeNode* Node = FindNodeTeaching(Tree, AbilityId);
		const FString Name = AbilityId.ToString();
		if (!Test.TestNotNull(*FString::Printf(TEXT("A node teaches %s"), *Name), Node))
		{
			return nullptr;
		}
		Test.TestTrue(
			*FString::Printf(TEXT("%s presents as an ultimate"), *Name),
			Node->KindTag.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("SkillTree.NodeKind.Ultimate"))));
		int32 OtherNodeCosts = 0;
		for (const FRpgSkillTreeNode& Other : Tree.Nodes)
		{
			OtherNodeCosts += &Other != Node ? Other.Cost : 0;
		}
		Test.TestTrue(
			*FString::Printf(TEXT("%s keeps a point gate the other nodes can fill"), *Name),
			Node->RequiredPointsInTree > 0 && Node->RequiredPointsInTree <= OtherNodeCosts &&
				Node->RequiredPointsInTree + Node->Cost <= Tree.MaxPoints);
		return ExpectNodeGrantsAbility(Test, *Node, AbilitySystem, AbilityId);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPickaxeSkillTreeContractTest,
	"SurvivalRpg.Harvesting.Content.PickaxeSkillTreeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPickaxeSkillTreeContractTest::RunTest(const FString& Parameters)
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

	// Rift Grip: the awakened hold-to-aim ability, learned in the pickaxe tree with Mining points.
	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(
		*this,
		PickaxeItemClassPath,
		FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Pickaxe")),
		RpgTradeSkillGameplayTags::Skill_Gathering_Mining);
	if (!Tree)
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* RiftGrip = ExpectFirstPointActive(
		*this, *Tree, *AbilitySystem, FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.RiftGrip")));
	TestTrue(TEXT("Rift Grip aims while its input is held"), RiftGrip && RiftGrip->IsAimWhileInputHeld());
	TestFalse(TEXT("Rift Grip never strikes weak points"), RiftGrip && RiftGrip->CanHitWeakPoints());
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
	FRpgHarvestAxeSkillTreeContractTest,
	"SurvivalRpg.Harvesting.Content.AxeSkillTreeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAxeSkillTreeContractTest::RunTest(const FString& Parameters)
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

	// Death Wave and Grave Swarm: the awakened powers, learned in the axe tree with Logging points.
	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(
		*this,
		AxeItemClassPath,
		FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Axe")),
		RpgTradeSkillGameplayTags::Skill_Gathering_Logging);
	if (!Tree)
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* DeathWave = ExpectFirstPointActive(
		*this, *Tree, *AbilitySystem, FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.DeathWave")));
	TestTrue(TEXT("Death Wave aims while its input is held"), DeathWave && DeathWave->IsAimWhileInputHeld());
	TestTrue(TEXT("Death Wave harvests an area"), DeathWave && DeathWave->HarvestsArea());
	const URpgGameplayAbility_Harvest* GraveSwarm = ExpectFirstPointActive(
		*this, *Tree, *AbilitySystem, FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.GraveSwarm")));
	TestTrue(TEXT("Grave Swarm aims while its input is held"), GraveSwarm && GraveSwarm->IsAimWhileInputHeld());
	TestTrue(TEXT("Grave Swarm summons a swarm"), GraveSwarm && GraveSwarm->SummonsSwarm());

	// Striding Wave: the ultimate, which harvests around the walking player for a while without aiming.
	const URpgGameplayAbility_Harvest* StridingWave = ExpectUltimate(
		*this, *Tree, *AbilitySystem, FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.StridingWave")));
	TestTrue(TEXT("Striding Wave executes on press"), StridingWave && !StridingWave->IsAimWhileInputHeld());
	TestTrue(TEXT("Striding Wave strides with the player"), StridingWave && StridingWave->HasStride());

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

#endif // WITH_DEV_AUTOMATION_TESTS
