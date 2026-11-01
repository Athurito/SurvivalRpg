#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "GameFeatureData.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestBurn.h"
#include "Harvesting/RpgHarvestRuneStrike.h"
#include "Harvesting/RpgHarvestChainComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgFocusSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/Combat/RpgGameplayCueNotify_Impact.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddAbilities.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddComponents.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
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
	const TCHAR* WorkerRuneClassPath = TEXT("/GF_Harvesting_Magic/Items/Runes/ID_Rune_Totenarbeiter.ID_Rune_Totenarbeiter_C");
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
	const TCHAR* AxeActorClassPath = TEXT("/GF_Harvesting_Magic/Equipment/Tools/BP_Tool_AxeActor.BP_Tool_AxeActor_C");
	const TCHAR* PickaxeActorClassPath = TEXT("/GF_Harvesting_Magic/Equipment/Tools/BP_Tool_PickaxeActor.BP_Tool_PickaxeActor_C");
	const TCHAR* ToolHitCueClassPath = TEXT("/GF_Harvesting_Magic/GameplayCues/GCN_Harvest_ToolHit.GCN_Harvest_ToolHit_C");
	const TCHAR* StoneToolHitCueClassPath =
		TEXT("/GF_Harvesting_Magic/GameplayCues/GCN_Harvest_ToolHit_Stone.GCN_Harvest_ToolHit_Stone_C");
	const TCHAR* HarvestNodeClassPaths[] = {
		TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_Bush.BP_HarvestNode_Bush_C"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_IronVein.BP_HarvestNode_IronVein_C"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_Mining.BP_HarvestNode_Mining_C"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_StaticMeshBase.BP_HarvestNode_StaticMeshBase_C"),
		TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_Tree.BP_HarvestNode_Tree_C")};

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

	/** Spawns ClassPath in World for an inspection of its components; null when it does not load. */
	AActor* SpawnForInspection(UWorld* World, const TCHAR* ClassPath)
	{
		UClass* ActorClass = LoadClass<AActor>(nullptr, ClassPath);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World && ActorClass ? World->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParameters) : nullptr;
	}

	/**
	 * A tool swing sweeps a sphere of about 12 cm over the whole tool on the weapon channel, which harvestables overlap:
	 * from a grip point on the handle near the hands to the head's trace sockets, so a resource right in front of the
	 * player meets the handle. It plays a hit cue of the tool hit family that the swinging client can predict: an impact
	 * notify (CueClassPath) that absorbs the server's copy there.
	 */
	void ExpectToolSwingTrace(
		FAutomationTestBase& Test,
		UWorld* World,
		const URpgGameplayAbility_Harvest& Swing,
		const TCHAR* ToolActorClassPath,
		const TCHAR* CueClassPath)
	{
		const FString Name = GetNameSafe(Swing.GetClass());
		Test.TestTrue(*FString::Printf(TEXT("%s strikes with its tool's trace, not the view ray"), *Name), Swing.UsesToolSwing());
		// Holding the input keeps swinging, and a dodge, block or attack replaces a swing instead of waiting for it.
		Test.TestTrue(
			*FString::Printf(TEXT("%s keeps swinging while its input is held"), *Name),
			Swing.GetActivationPolicy() == ERpgAbilityActivationPolicy::WhileInputActive);
		Test.TestTrue(
			*FString::Printf(TEXT("%s yields to other exclusive abilities"), *Name),
			Swing.GetActivationGroup() == ERpgAbilityActivationGroup::Exclusive_Replaceable);
		const FRpgSwingTraceParams& Trace = Swing.GetSwingTrace();
		Test.TestTrue(*FString::Printf(TEXT("%s has a traceable swing"), *Name), Trace.HasValidTraceData());
		Test.TestTrue(
			*FString::Printf(TEXT("%s sweeps a sphere of 8 to 16 cm"), *Name),
			Trace.TraceMode == ERpgWeaponAttackTraceMode::SphereSweep && Trace.TraceRadius >= 8.0f && Trace.TraceRadius <= 16.0f);
		Test.TestTrue(
			*FString::Printf(TEXT("%s traces the weapon channel, which harvestables overlap"), *Name),
			Trace.TraceChannel == Rpg_TraceChannel_Weapon);
		const TArray<FName>& Points = Trace.TracePointSockets;
		Test.TestTrue(
			*FString::Printf(TEXT("%s traces from the grip on the handle to the head's base and tip"), *Name),
			Points.Num() >= 3 && Points[0] == TEXT("TracePoint_Grip") && Points.Last(1) == TEXT("TracePoint_Base") &&
				Points.Last() == TEXT("TracePoint_Tip"));

		// The trace points are sockets of the tool mesh the equipment spawns in the hand, and they span the tool.
		const AActor* ToolActor = SpawnForInspection(World, ToolActorClassPath);
		const UStaticMeshComponent* ToolMesh = nullptr;
		if (ToolActor)
		{
			TInlineComponentArray<UStaticMeshComponent*> Meshes(ToolActor);
			for (const UStaticMeshComponent* Mesh : Meshes)
			{
				if (Points.ContainsByPredicate([Mesh](const FName Socket) { return !Mesh->DoesSocketExist(Socket); }))
				{
					continue;
				}
				ToolMesh = Mesh;
			}
		}
		if (Test.TestNotNull(*FString::Printf(TEXT("The tool mesh of %s carries every trace socket"), *Name), ToolMesh) &&
			!Points.IsEmpty())
		{
			const double Span = FVector::Dist(
				ToolMesh->GetSocketTransform(Points[0], RTS_Component).GetLocation(),
				ToolMesh->GetSocketTransform(Points.Last(), RTS_Component).GetLocation());
			Test.TestTrue(
				*FString::Printf(TEXT("The trace of %s spans the tool from the grip to the tip (%.0f cm)"), *Name, Span),
				Span >= 40.0);

			// The mesh pivot is the right hand's grip with the handle along +Z and the edge or pick toward +X, so the
			// ToolMesh transform turns the tool about the hand. The grip point lies on the handle axis within a hand's
			// width above the pivot, and the head lies well up the handle.
			const FVector Grip = ToolMesh->GetSocketTransform(Points[0], RTS_Component).GetLocation();
			const FVector Tip = ToolMesh->GetSocketTransform(Points.Last(), RTS_Component).GetLocation();
			Test.TestTrue(
				*FString::Printf(
					TEXT("The mesh of %s has its pivot at the grip, with the grip point on +Z close above it (%s)"),
					*Name,
					*Grip.ToCompactString()),
				FVector2D(Grip.X, Grip.Y).Size() <= 1.0 && Grip.Z >= 0.0 && Grip.Z <= 10.0);
			bool bHeadUpTheHandle = true;
			for (int32 Index = 1; Index < Points.Num(); ++Index)
			{
				bHeadUpTheHandle &= ToolMesh->GetSocketTransform(Points[Index], RTS_Component).GetLocation().Z >= Grip.Z + 30.0;
			}
			Test.TestTrue(*FString::Printf(TEXT("The head of %s lies up the handle along +Z"), *Name), bHeadUpTheHandle);
			Test.TestTrue(
				*FString::Printf(TEXT("The edge or pick of %s leads toward +X (%s)"), *Name, *Tip.ToCompactString()),
				Tip.X > 0.0 && FMath::Abs(Tip.Y) <= 5.0);
		}

		const FStructProperty* SuccessCueProperty =
			FindFProperty<FStructProperty>(URpgGameplayAbility_Harvest::StaticClass(), TEXT("SuccessGameplayCue"));
		const FGameplayTag SuccessCue = SuccessCueProperty ? *SuccessCueProperty->ContainerPtrToValuePtr<FGameplayTag>(&Swing) : FGameplayTag();
		Test.TestTrue(
			*FString::Printf(TEXT("%s plays a tool hit cue"), *Name),
			SuccessCue.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.ToolHit"))));
		const UClass* CueClass = LoadClass<UGameplayCueNotify_Static>(nullptr, CueClassPath);
		const UGameplayCueNotify_Static* Cue = CueClass ? GetDefault<UGameplayCueNotify_Static>(CueClass) : nullptr;
		Test.TestTrue(
			*FString::Printf(
				TEXT("The hit cue of %s is an impact notify, so the swinging client's prediction absorbs the server's copy"),
				*Name),
			Cue && CueClass->IsChildOf<URpgGameplayCueNotify_Impact>() && Cue->GameplayCueTag == SuccessCue);
	}

	/** Region of the shared tree that holds harvesting spells; every other region holds passive talents. */
	const FName SpellsRegion(TEXT("Spells"));

	bool IsSpellNode(const FRpgSkillTreeNode& Node)
	{
		return Node.RegionId == SpellsRegion;
	}

	/**
	 * A spell node is an Active node whose set grants exactly one ability, with an Ability.Harvesting.Spell id and no
	 * input tag, so learning it fills the first free Q/E/R slot; it also grants Harvest.Spell.Known, which starts the
	 * focus meter. Returns the spell's ability class, or null.
	 */
	TSubclassOf<URpgGameplayAbility> ExpectSpellNode(FAutomationTestBase& Test, const FRpgSkillTreeNode& Node)
	{
		const FString Name = Node.NodeTag.ToString();
		Test.TestTrue(
			*FString::Printf(TEXT("%s is an Active node"), *Name),
			Node.KindTag == FGameplayTag::RequestGameplayTag(TEXT("SkillTree.NodeKind.Active")));
		Test.TestTrue(
			*FString::Printf(TEXT("%s grants Harvest.Spell.Known, which starts the focus meter"), *Name),
			Node.GrantedTags.HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Known"))));
		Test.TestTrue(*FString::Printf(TEXT("%s needs no rune"), *Name), Node.RequiredRunes.IsEmpty());
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s grants an ability set"), *Name), Node.AbilitySet.Get()))
		{
			return nullptr;
		}
		const TArray<FRpgAbilitySet_GameplayAbility>& Abilities = Node.AbilitySet->GetGrantedGameplayAbilities();
		Test.TestTrue(*FString::Printf(TEXT("%s grants no effect"), *Name), Node.AbilitySet->GetGrantedGameplayEffects().IsEmpty());
		if (!Test.TestEqual(*FString::Printf(TEXT("%s grants exactly one ability"), *Name), Abilities.Num(), 1))
		{
			return nullptr;
		}
		const FRpgAbilitySet_GameplayAbility& Spell = Abilities[0];
		const FGameplayTag SpellRoot = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.Spell"));
		Test.TestTrue(
			*FString::Printf(TEXT("%s's ability has an Ability.Harvesting.Spell id (%s)"), *Name, *Spell.AbilityIdTag.ToString()),
			Spell.AbilityIdTag.MatchesTag(SpellRoot) && Spell.AbilityIdTag != SpellRoot);
		Test.TestFalse(*FString::Printf(TEXT("%s's ability has no input tag; the free slot binds it"), *Name), Spell.InputTag.IsValid());
		Test.TestNotNull(*FString::Printf(TEXT("%s's ability class is set"), *Name), Spell.Ability.Get());
		return Spell.Ability;
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
	if (Strike)
	{
		// The pickaxe strikes stone: its hit cue is the stone variant of the tool hit.
		ExpectToolSwingTrace(*this, TestWorld.GetWorld(), *Strike, PickaxeActorClassPath, StoneToolHitCueClassPath);
	}

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

	// Talents are passive tunings that occupy no Q/E/R. Spells, in their own region, each grant one ability that
	// learning places on the first free slot. Every branch (region) has one entry node, and its other nodes build on it.
	TMap<FName, const FRpgSkillTreeNode*> Entries;
	TSet<FName> ChoiceGroups;
	int32 SpellNodes = 0;
	for (const FRpgSkillTreeNode& Node : Tree.Nodes)
	{
		const FString Name = Node.NodeTag.ToString();
		if (IsSpellNode(Node))
		{
			++SpellNodes;
			ExpectSpellNode(*this, Node);
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("%s grants no ability"), *Name), Node.AbilitySet == nullptr);
			TestFalse(*FString::Printf(TEXT("%s changes harvesting through tunings"), *Name), Node.AbilityTunings.IsEmpty());
		}
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
	TestTrue(TEXT("The tree frames a Spells region"), Tree.FindRegion(SpellsRegion) != nullptr);
	TestTrue(TEXT("The tree teaches at least one spell"), SpellNodes > 0);

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
	if (Chop)
	{
		ExpectToolSwingTrace(*this, TestWorld.GetWorld(), *Chop, AxeActorClassPath, ToolHitCueClassPath);
	}

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
	FRpgHarvestSwingMontageContractTest,
	"SurvivalRpg.Harvesting.Content.SwingMontageContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSwingMontageContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// The authored values are protected designer data; read them as the ability's own reflected properties.
	const FObjectPropertyBase* MontageProperty =
		FindFProperty<FObjectPropertyBase>(URpgGameplayAbility_Harvest::StaticClass(), TEXT("HarvestMontage"));
	const FStructProperty* CommitTagProperty =
		FindFProperty<FStructProperty>(URpgGameplayAbility_Harvest::StaticClass(), TEXT("CommitEventTag"));
	const FFloatProperty* PlayRateProperty =
		FindFProperty<FFloatProperty>(URpgGameplayAbility_Harvest::StaticClass(), TEXT("MontagePlayRate"));
	if (!TestNotNull(TEXT("HarvestMontage is reflected"), MontageProperty) ||
		!TestNotNull(TEXT("CommitEventTag is reflected"), CommitTagProperty) ||
		!TestNotNull(TEXT("MontagePlayRate is reflected"), PlayRateProperty))
	{
		return false;
	}

	for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
		const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), SwingPath), Swing))
		{
			continue;
		}
		const UAnimMontage* Montage = Cast<UAnimMontage>(MontageProperty->GetObjectPropertyValue_InContainer(Swing));
		const FGameplayTag CommitTag = *CommitTagProperty->ContainerPtrToValuePtr<FGameplayTag>(Swing);
		const float PlayRate = PlayRateProperty->GetFloatingPointPropertyValue(PlayRateProperty->ContainerPtrToValuePtr<void>(Swing));

		// A tool swing traces its tool through the montage's attack window, as a melee attack does: one queued window
		// pair that the server also receives, ending before the auto blend-out.
		float WindowStartTime = 0.0f;
		float WindowEndTime = 0.0f;
		FString Failure;
		if (!TestTrue(
				*FString::Printf(TEXT("%s traces inside one valid attack window"), SwingPath),
				FRpgSwingTraceWindow::ResolveWindowTiming(
					Montage,
					PlayRate * (Montage ? Montage->RateScale : 1.0f),
					WindowStartTime,
					WindowEndTime,
					Failure)))
		{
			AddInfo(Failure);
			continue;
		}

		// The impact notify gates nothing for a tool swing; it marks the authored impact, which the window must cover.
		float ImpactDelay = 0.0f;
		if (TestTrue(
				*FString::Printf(TEXT("%s marks its impact with one notify"), SwingPath),
				URpgGameplayAbility_Harvest::ResolveCommitDelay(Montage, CommitTag, PlayRate, ImpactDelay, Failure)))
		{
			const float ImpactTime = ImpactDelay * PlayRate * Montage->RateScale;
			TestTrue(
				*FString::Printf(TEXT("%s strikes inside its attack window"), SwingPath),
				WindowStartTime < ImpactTime && ImpactTime < WindowEndTime);

			// The swing ends shortly after the impact, without pulling the tool back out, so a held input blends into
			// the next wind-up (HRW-04b). Seconds of play at the ability's rate.
			const float CycleSeconds = URpgGameplayAbility_Harvest::ResolveSwingCycleSeconds(Montage, PlayRate * Montage->RateScale);
			const float ImpactSeconds = ImpactDelay;
			TestTrue(
				*FString::Printf(
					TEXT("%s blends out at most 0.25 s after its impact (%.3f s after it)"),
					SwingPath,
					CycleSeconds - ImpactSeconds),
				CycleSeconds > ImpactSeconds && CycleSeconds - ImpactSeconds <= 0.25f);
		}

		// The swinger's own montage freezes briefly at a hit. On a listen-server host that freeze shows in the replicated
		// montage position, which observers only correct beyond the ability system's threshold.
		const float HitstopSeconds = Swing->GetToolSwingHitstopSeconds();
		TestTrue(
			*FString::Printf(TEXT("%s freezes the swinger briefly at a hit (%.0f ms)"), SwingPath, HitstopSeconds * 1000.0f),
			HitstopSeconds > 0.0f &&
				HitstopSeconds * PlayRate * Montage->RateScale < URpgGameplayAbility_Harvest::SimulatedMontageCorrectionSeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestableCollisionContractTest,
	"SurvivalRpg.Harvesting.Content.HarvestableCollisionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestableCollisionContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	// Resources stop the view ray, interaction traces and walking pawns, and let weapon traces pass: melee traces stop on
	// no trunk, while the tool swing's multi trace reports them as overlaps.
	const FName ProfileName = FRpgHarvestTargeting::GetHarvestableCollisionProfileName();
	FCollisionResponseTemplate Profile;
	if (!TestTrue(TEXT("The harvestable collision profile exists"), UCollisionProfile::Get()->GetProfileTemplate(ProfileName, Profile)))
	{
		return false;
	}
	TestEqual(TEXT("Harvestables collide for queries and physics"), Profile.CollisionEnabled.GetValue(), ECollisionEnabled::QueryAndPhysics);
	TestEqual(TEXT("Harvestables block the view ray"), Profile.ResponseToChannels.GetResponse(ECC_Visibility), ECR_Block);
	TestEqual(TEXT("Harvestables block interaction traces"), Profile.ResponseToChannels.GetResponse(Rpg_TraceChannel_Interaction), ECR_Block);
	TestEqual(TEXT("Harvestables block walking pawns"), Profile.ResponseToChannels.GetResponse(ECC_Pawn), ECR_Block);
	TestEqual(TEXT("Harvestables overlap weapon traces"), Profile.ResponseToChannels.GetResponse(Rpg_TraceChannel_Weapon), ECR_Overlap);

	auto ExpectHarvestableCollision = [this, ProfileName](const UPrimitiveComponent* Component, const FString& Name)
	{
		TestTrue(
			*FString::Printf(TEXT("%s uses the harvestable collision profile"), *Name),
			Component && Component->GetCollisionProfileName() == ProfileName &&
				Component->GetCollisionResponseToChannel(Rpg_TraceChannel_Weapon) == ECR_Overlap &&
				Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
	};
	ExpectHarvestableCollision(GetDefault<URpgHarvestableInstancesComponent>(), TEXT("The native PCG instances component"));
	ExpectHarvestableCollision(GetDefault<URpgHarvestableInstancedMeshComponent>(), TEXT("The native instanced mesh resource"));
	for (const TCHAR* InstancesPath : {DeadPineInstancesClassPath, IronVeinInstancesClassPath})
	{
		UClass* InstancesClass = LoadClass<URpgHarvestableInstancesComponent>(nullptr, InstancesPath);
		ExpectHarvestableCollision(InstancesClass ? GetDefault<URpgHarvestableInstancesComponent>(InstancesClass) : nullptr, InstancesPath);
	}

	// Actor nodes: every part that stops the view ray is the resource's collision and lets weapon traces pass.
	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	for (const TCHAR* NodePath : HarvestNodeClassPaths)
	{
		const AActor* Node = SpawnForInspection(TestWorld.GetWorld(), NodePath);
		if (!TestNotNull(*FString::Printf(TEXT("%s spawns"), NodePath), Node))
		{
			continue;
		}
		int32 ResourceParts = 0;
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Node);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->IsQueryCollisionEnabled() && Primitive->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block)
			{
				++ResourceParts;
				ExpectHarvestableCollision(Primitive, FString::Printf(TEXT("%s.%s"), NodePath, *Primitive->GetName()));
			}
		}
		TestTrue(*FString::Printf(TEXT("%s has a collision a swing can reach"), NodePath), ResourceParts > 0);
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestHelperContractTest,
	"SurvivalRpg.Harvesting.Content.HelperContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestHelperContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Totenarbeiter is a rank I to III rune, found, forged, upgraded and salvaged like Brand.
	TestHarvestingRune(*this, WorkerRuneClassPath, TEXT("Rune.Totenarbeiter"), Ability_Tuning_Harvest_WorkerWork);

	// Both swings raise workers whose work names the rune; the workers rise as one worker, for a limited time.
	const FGameplayTag WorkerTag = FGameplayTag::RequestGameplayTag(TEXT("Rune.Totenarbeiter"), false);
	for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
		const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
		const TSubclassOf<ARpgHarvestSwarm> WorkerClass = Swing ? Swing->GetWorkerSwarmClass() : nullptr;
		TestTrue(*FString::Printf(TEXT("%s raises workers as Totenarbeiter"), SwingPath),
			WorkerClass && GetDefault<ARpgHarvestSwarm>(WorkerClass)->GetProcTag() == WorkerTag);
	}

	// The Helper branch needs Totenarbeiter in the tool; its two choices exclude each other.
	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(
		*this,
		PickaxeItemClassPath,
		FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting")),
		RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting);
	if (!Tree)
	{
		return false;
	}
	FGameplayTagContainer HelperRunes;
	HelperRunes.AddTag(WorkerTag);
	int32 HelperNodes = 0;
	TSet<FName> HelperChoices;
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (Node.RegionId != TEXT("Helpers"))
		{
			continue;
		}
		++HelperNodes;
		TestTrue(*FString::Printf(TEXT("%s needs Totenarbeiter"), *Node.NodeTag.ToString()), Node.RequiredRunes == HelperRunes);
		if (!Node.ExclusiveGroup.IsNone())
		{
			HelperChoices.Add(Node.ExclusiveGroup);
		}
	}
	TestEqual(TEXT("Helpers has an entry and two choices"), HelperNodes, 3);
	TestEqual(TEXT("The two Helper choices share one group"), HelperChoices.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestCatalogContractTest,
	"SurvivalRpg.Harvesting.Content.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestCatalogContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Axe and pickaxe share the one harvesting tree.
	const FGameplayTag TreeTag = FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting"));
	const FGameplayTag Mastery = RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting;
	const URpgSkillTreeDefinition* Tree = LoadToolSkillTree(*this, PickaxeItemClassPath, TreeTag, Mastery);
	if (!Tree)
	{
		return false;
	}
	TestTrue(TEXT("The axe carries the same tree"), LoadToolSkillTree(*this, AxeItemClassPath, TreeTag, Mastery) == Tree);

	// The plan's catalog: five talent branches of a one-point entry and two exclusive three-point choices, fifteen
	// talents, and exactly enough points for one path through every branch plus every spell.
	TMap<FName, TArray<const FRpgSkillTreeNode*>> Branches;
	int32 PathCost = 0;
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (IsSpellNode(Node))
		{
			PathCost += Node.Cost;
			continue;
		}
		Branches.FindOrAdd(Node.RegionId).Add(&Node);
	}
	int32 Talents = 0;
	for (const TPair<FName, TArray<const FRpgSkillTreeNode*>>& Branch : Branches)
	{
		Talents += Branch.Value.Num();
	}
	TestEqual(TEXT("The tree has fifteen talents"), Talents, 15);
	TestEqual(TEXT("The tree has five talent branches"), Branches.Num(), 5);
	for (const TPair<FName, TArray<const FRpgSkillTreeNode*>>& Branch : Branches)
	{
		const FString Name = Branch.Key.ToString();
		const TArray<const FRpgSkillTreeNode*> Entries = Branch.Value.FilterByPredicate([](const FRpgSkillTreeNode* Node)
		{
			return Node->Prerequisites.IsEmpty();
		});
		if (!TestEqual(*FString::Printf(TEXT("%s has one entry"), *Name), Entries.Num(), 1) ||
			!TestEqual(*FString::Printf(TEXT("%s has three talents"), *Name), Branch.Value.Num(), 3))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s's entry costs one point"), *Name), Entries[0]->Cost, 1);
		FName Group = NAME_None;
		for (const FRpgSkillTreeNode* Node : Branch.Value)
		{
			if (Node == Entries[0])
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s costs three points"), *Node->NodeTag.ToString()), Node->Cost, 3);
			TestTrue(*FString::Printf(TEXT("%s builds on its entry"), *Node->NodeTag.ToString()),
				Node->Prerequisites.Num() == 1 && Node->Prerequisites[0] == Entries[0]->NodeTag);
			TestFalse(*FString::Printf(TEXT("%s is a choice"), *Node->NodeTag.ToString()), Node->ExclusiveGroup.IsNone());
			TestTrue(*FString::Printf(TEXT("%s shares its branch's choice group"), *Node->NodeTag.ToString()),
				Group.IsNone() || Group == Node->ExclusiveGroup);
			Group = Node->ExclusiveGroup;
			TestTrue(*FString::Printf(TEXT("%s needs the runes of its entry"), *Node->NodeTag.ToString()),
				Entries[0]->RequiredRunes.IsEmpty() || Node->RequiredRunes.HasAny(Entries[0]->RequiredRunes));
		}
		PathCost += 1 + 3;
	}
	TestEqual(TEXT("One path through every branch and every spell spend exactly the tree's points"), PathCost, Tree->MaxPoints);

	// Every talent has gameplay: it tunes at least one value the harvest ability reads.
	const TSet<FGameplayTag> ReadTunings = {
		Ability_Tuning_Harvest_AreaRadius, Ability_Tuning_Harvest_Reach, Ability_Tuning_Harvest_MaxTargets,
		Ability_Tuning_Harvest_Sections, Ability_Tuning_Harvest_Creatures, Ability_Tuning_Harvest_StrikeInterval,
		Ability_Tuning_Harvest_StrikeRadius, Ability_Tuning_Harvest_Cooldown, Ability_Tuning_Harvest_Work,
		Ability_Tuning_Harvest_WeakPointWork, Ability_Tuning_Harvest_FollowUpWork, Ability_Tuning_Harvest_FollowUpWindow,
		Ability_Tuning_Harvest_BurnWork, Ability_Tuning_Harvest_BurnDuration, Ability_Tuning_Harvest_BurnRefresh,
		Ability_Tuning_Harvest_BurnDischarge, Ability_Tuning_Harvest_BurnWander, Ability_Tuning_Harvest_EchoWork,
		Ability_Tuning_Harvest_EchoHits, Ability_Tuning_Harvest_TransferWork, Ability_Tuning_Harvest_RuneFlowSplit,
		Ability_Tuning_Harvest_LeapWork, Ability_Tuning_Harvest_PierceWork, Ability_Tuning_Harvest_SpreadReach,
		Ability_Tuning_Harvest_SpreadSplit, Ability_Tuning_Harvest_WorkerWork, Ability_Tuning_Harvest_WorkerCount,
		Ability_Tuning_Harvest_WorkerHits, Ability_Tuning_Harvest_WorkerLifetime};
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		if (IsSpellNode(Node))
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("%s tunes something"), *Node.NodeTag.ToString()), Node.AbilityTunings.IsEmpty());
		for (const FRpgSkillTreeAbilityTuning& Tuning : Node.AbilityTunings)
		{
			TestTrue(*FString::Printf(TEXT("%s tunes %s, which the harvest ability reads"), *Node.NodeTag.ToString(), *Tuning.TuningTag.ToString()),
				ReadTunings.Contains(Tuning.TuningTag));
		}
	}

	// The six runes: each drives a branch, reaches the player the same ways, and every swing carries its effect.
	struct FCatalogRune
	{
		const TCHAR* ClassPath;
		const TCHAR* RuneTag;
	};
	const FCatalogRune Runes[] = {
		{BrandRuneClassPath, TEXT("Rune.Brand")},
		{EchoRuneClassPath, TEXT("Rune.Echo")},
		{TransferRuneClassPath, TEXT("Rune.Kraftuebertrag")},
		{LeapRuneClassPath, TEXT("Rune.Erntesprung")},
		{PierceRuneClassPath, TEXT("Rune.Durchschlag")},
		{WorkerRuneClassPath, TEXT("Rune.Totenarbeiter")}};
	FGameplayTagContainer CatalogRunes;
	const URpgCraftingRecipeSet* KilnRecipes = LoadObject<URpgCraftingRecipeSet>(nullptr, KilnRecipeSetPath);
	const URpgRewardClaimDefinition* RuneChoice = LoadObject<URpgRewardClaimDefinition>(nullptr, RuneChoiceRewardPath);
	for (const FCatalogRune& Entry : Runes)
	{
		const FGameplayTag RuneTag = FGameplayTag::RequestGameplayTag(Entry.RuneTag, false);
		CatalogRunes.AddTag(RuneTag);
		UClass* RuneClass = LoadClass<URpgInventoryItemDefinition>(nullptr, Entry.ClassPath);
		const URpgInventoryFragment_Rune* Rune = RuneClass
			? Cast<URpgInventoryFragment_Rune>(GetDefault<URpgInventoryItemDefinition>(RuneClass)->FindFragmentByClass(
				URpgInventoryFragment_Rune::StaticClass()))
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s is a rune item"), Entry.RuneTag), Rune))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s drives a branch"), Entry.RuneTag),
			Tree->Nodes.ContainsByPredicate([RuneTag](const FRpgSkillTreeNode& Node) { return Node.RequiredRunes.HasTagExact(RuneTag); }));
		for (int32 Rank = 1; Rank <= Rune->GetMaxRank(); ++Rank)
		{
			for (const FRpgSkillTreeAbilityTuning& Tuning : Rune->FindRank(Rank)->AbilityTunings)
			{
				TestTrue(*FString::Printf(TEXT("%s rank %d tunes a value the harvest ability reads"), Entry.RuneTag, Rank),
					ReadTunings.Contains(Tuning.TuningTag));
			}
		}
		TestTrue(*FString::Printf(TEXT("The Kiln forges %s"), Entry.RuneTag), KilnRecipes && KilnRecipes->Recipes.Contains(Rune->BaseRecipe));
		TestTrue(*FString::Printf(TEXT("The rune choice offers %s"), Entry.RuneTag),
			RuneChoice && RuneChoice->Options.ContainsByPredicate([RuneClass](const FRpgRewardClaimOption& Option)
			{
				return Option.ItemDefinition == RuneClass;
			}));
	}
	for (const FRpgSkillTreeNode& Node : Tree->Nodes)
	{
		TestTrue(*FString::Printf(TEXT("%s needs only catalog runes"), *Node.NodeTag.ToString()), CatalogRunes.HasAllExact(Node.RequiredRunes));
	}
	for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
		const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
		TestTrue(*FString::Printf(TEXT("%s carries every rune's effect"), SwingPath),
			Swing && Swing->GetBurnClass() && Swing->GetEchoStrikeClass() && Swing->GetTransferStrikeClass() &&
				Swing->GetLeapStrikeClass() && Swing->GetPierceStrikeClass() && Swing->GetWorkerSwarmClass());
	}
	return true;
}

namespace RpgHarvestFocusContractTests
{
	const TCHAR* HudBottomBarClassPath = TEXT("/Game/SurvivalRpg/UI/Hud/CUI_HudBottomBar.CUI_HudBottomBar_C");

	/** Every entry of the Add Rpg Widgets actions of FeatureData as (widget class path, slot tag), read by reflection. */
	TArray<TPair<FSoftObjectPath, FGameplayTag>> CollectWidgetRegistrations(const UGameFeatureData& FeatureData)
	{
		TArray<TPair<FSoftObjectPath, FGameplayTag>> Registrations;
		for (const UGameFeatureAction* Action : FeatureData.GetActions())
		{
			const FArrayProperty* WidgetsProperty = Action && Action->GetClass()->GetName() == TEXT("RpgGameFeatureAction_AddWidgets")
				? FindFProperty<FArrayProperty>(Action->GetClass(), TEXT("Widgets"))
				: nullptr;
			const FStructProperty* EntryProperty = WidgetsProperty ? CastField<FStructProperty>(WidgetsProperty->Inner) : nullptr;
			const FSoftClassProperty* ClassProperty =
				EntryProperty ? FindFProperty<FSoftClassProperty>(EntryProperty->Struct, TEXT("WidgetClass")) : nullptr;
			const FStructProperty* SlotProperty = EntryProperty ? FindFProperty<FStructProperty>(EntryProperty->Struct, TEXT("SlotTag")) : nullptr;
			if (!ClassProperty || !SlotProperty)
			{
				continue;
			}
			FScriptArrayHelper Entries(WidgetsProperty, WidgetsProperty->ContainerPtrToValuePtr<void>(Action));
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const uint8* Entry = Entries.GetRawPtr(Index);
				Registrations.Emplace(
					ClassProperty->GetPropertyValue_InContainer(Entry).ToSoftObjectPath(),
					*SlotProperty->ContainerPtrToValuePtr<FGameplayTag>(Entry));
			}
		}
		return Registrations;
	}

	/** Extension point tags the widget tree of WidgetClass exposes, read by reflection. */
	TArray<FGameplayTag> CollectExtensionPointTags(const UClass* WidgetClass)
	{
		TArray<FGameplayTag> Tags;
		const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		const UWidgetTree* WidgetTree = GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
		if (WidgetTree)
		{
			WidgetTree->ForEachWidget([&Tags](const UWidget* Widget)
			{
				const FStructProperty* TagProperty = Widget && Widget->GetClass()->GetName() == TEXT("UIExtensionPointWidget")
					? FindFProperty<FStructProperty>(Widget->GetClass(), TEXT("ExtensionPointTag"))
					: nullptr;
				if (TagProperty)
				{
					Tags.Add(*TagProperty->ContainerPtrToValuePtr<FGameplayTag>(Widget));
				}
			});
		}
		return Tags;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestFocusContractTest,
	"SurvivalRpg.Harvesting.Content.FocusFillContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestFocusContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestFocusContractTests;

	// Both tool swings fill focus through a gain effect: an instant SetByCaller add on Focus that needs a known spell and
	// that a running spell blocks.
	const FGameplayTag SpellActive = FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Active"));
	const FGameplayTag SpellKnown = FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Known"));
	for (const TCHAR* SwingPath : {AxeChopAbilityClassPath, PickaxeStrikeAbilityClassPath})
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingPath);
		const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), SwingPath), Swing))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s fills focus per landed swing"), SwingPath), Swing->GetFocusGainPerSwing() > 0.0f);
		const UGameplayEffect* Gain = Swing->GetFocusGainEffect() ? Swing->GetFocusGainEffect()->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s names a focus gain effect"), SwingPath), Gain))
		{
			continue;
		}
		TestTrue(
			*FString::Printf(TEXT("%s's gain is instant"), SwingPath),
			Gain->DurationPolicy == EGameplayEffectDurationType::Instant);
		TestTrue(
			*FString::Printf(TEXT("%s's gain adds its SetByCaller.Harvest.FocusGain magnitude to Focus"), SwingPath),
			Gain->Modifiers.ContainsByPredicate([](const FGameplayModifierInfo& Modifier)
			{
				return Modifier.Attribute == URpgFocusSet::GetFocusAttribute() && Modifier.ModifierOp == EGameplayModOp::Additive &&
					Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller &&
					Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag ==
						RpgHarvestingMagicGameplayTags::SetByCaller_Harvest_FocusGain.GetTag();
			}));
		const UTargetTagRequirementsGameplayEffectComponent* Requirements =
			Gain->FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
		TestTrue(
			*FString::Printf(TEXT("%s's gain ignores a running spell, so the spell never refills its meter"), SwingPath),
			Requirements && Requirements->ApplicationTagRequirements.IgnoreTags.HasTagExact(SpellActive));
		TestTrue(
			*FString::Printf(TEXT("%s's gain requires a known spell, so the meter starts with the first spell"), SwingPath),
			Requirements && Requirements->ApplicationTagRequirements.RequireTags.HasTagExact(SpellKnown));
	}

	// The harvesting GameFeature gives every player state the focus set and puts the meter into an authored HUD slot.
	const UGameFeatureData* FeatureData = LoadObject<UGameFeatureData>(nullptr, HarvestingFeatureDataPath);
	if (!TestNotNull(TEXT("The harvesting GameFeature data loads"), FeatureData))
	{
		return false;
	}
	int32 FocusSetGrants = 0;
	for (const UGameFeatureAction* Action : FeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddAbilities* AddAbilities = Cast<URpgGameFeatureAction_AddAbilities>(Action);
		for (const FRpgGameFeatureAbilitiesEntry& Entry : AddAbilities ? AddAbilities->AbilitiesList : TArray<FRpgGameFeatureAbilitiesEntry>())
		{
			for (const FRpgGameFeatureAttributeSetGrant& Grant : Entry.GrantedAttributes)
			{
				if (Grant.AttributeSetType.ToSoftObjectPath() != FSoftObjectPath(URpgFocusSet::StaticClass()))
				{
					continue;
				}
				++FocusSetGrants;
				const UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
				TestTrue(TEXT("The focus set lands on the player state"), ActorClass && ActorClass->IsChildOf<ARpgBasePlayerState>());
			}
		}
	}
	TestEqual(TEXT("The GameFeature grants the focus set exactly once"), FocusSetGrants, 1);

	const TArray<FGameplayTag> HudSlots = CollectExtensionPointTags(LoadClass<UUserWidget>(nullptr, HudBottomBarClassPath));
	int32 MeterRegistrations = 0;
	for (const TPair<FSoftObjectPath, FGameplayTag>& Registration : CollectWidgetRegistrations(*FeatureData))
	{
		const UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *Registration.Key.ToString());
		if (!WidgetClass || !WidgetClass->GetName().Contains(TEXT("FocusMeter")))
		{
			continue;
		}
		++MeterRegistrations;
		TestTrue(
			*FString::Printf(TEXT("The HUD bottom bar exposes the meter's slot %s"), *Registration.Value.ToString()),
			HudSlots.Contains(Registration.Value));
	}
	TestEqual(TEXT("The GameFeature registers one focus meter widget"), MeterRegistrations, 1);
	return true;
}

namespace RpgHarvestSpellContractTests
{
	const TCHAR* WideSwingEffectClassPath = TEXT("/GF_Harvesting_Magic/GAS/Effects/GE_HarvestSpell_WideSwing.GE_HarvestSpell_WideSwing_C");
	const TCHAR* WideSwingCueClassPath =
		TEXT("/GF_Harvesting_Magic/GameplayCues/GCN_Harvest_Spell_WideSwing.GCN_Harvest_Spell_WideSwing_C");
	const TCHAR* SpectralToolHitCueClassPath =
		TEXT("/GF_Harvesting_Magic/GameplayCues/GCN_Harvest_ToolHit_Spectral.GCN_Harvest_ToolHit_Spectral_C");
	const TCHAR* SpectralStoneToolHitCueClassPath =
		TEXT("/GF_Harvesting_Magic/GameplayCues/GCN_Harvest_ToolHit_Stone_Spectral.GCN_Harvest_ToolHit_Stone_Spectral_C");

	/** Reads a protected property of Object by name; null when it does not exist. */
	template <typename TValue>
	const TValue* ReadProperty(const UObject& Object, const UClass* DeclaringClass, const TCHAR* PropertyName)
	{
		const FProperty* Property = FindFProperty<FProperty>(DeclaringClass, PropertyName);
		return Property ? Property->ContainerPtrToValuePtr<TValue>(&Object) : nullptr;
	}

	/** The first native class above Class, so a Blueprint family base between them does not matter. */
	const UClass* GetNativeBase(const UClass* Class)
	{
		while (Class && !Class->IsNative())
		{
			Class = Class->GetSuperClass();
		}
		return Class;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWideSwingContractTest,
	"SurvivalRpg.Harvesting.Content.WideSwingSpellContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWideSwingContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;
	using namespace RpgHarvestSpellContractTests;
	using namespace RpgHarvestingMagicGameplayTags;

	// Wide Swing is the Spells node of the shared tree whose ability carries the spell's id.
	const FGameplayTag TreeTag = FGameplayTag::RequestGameplayTag(TEXT("SkillTree.Tree.Harvesting"));
	const URpgSkillTreeDefinition* Tree =
		LoadToolSkillTree(*this, AxeItemClassPath, TreeTag, RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting);
	if (!Tree)
	{
		return false;
	}
	const FGameplayTag WideSwingId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.Spell.WideSwing"));
	const FRpgSkillTreeNode* Node = Tree->Nodes.FindByPredicate([WideSwingId](const FRpgSkillTreeNode& Candidate)
	{
		TArray<FGameplayTag> Ids;
		Candidate.GetGrantedAbilityIds(Ids);
		return Ids.Contains(WideSwingId);
	});
	if (!TestNotNull(TEXT("A node of the shared tree teaches Wide Swing"), Node))
	{
		return false;
	}
	TestTrue(TEXT("Wide Swing is a Spells node"), IsSpellNode(*Node));
	TestTrue(TEXT("Wide Swing needs no other node"), Node->Prerequisites.IsEmpty());
	const TSubclassOf<URpgGameplayAbility> SpellClass = ExpectSpellNode(*this, *Node);
	const URpgGameplayAbility* Spell = SpellClass ? GetDefault<URpgGameplayAbility>(SpellClass) : nullptr;
	if (!TestNotNull(TEXT("The Wide Swing ability loads"), Spell))
	{
		return false;
	}

	// A Blueprint on the native ability base that its caster predicts, beside the held swing and never cancelling it.
	TestFalse(TEXT("Wide Swing is a Blueprint ability, not a native leaf"), SpellClass->IsNative());
	TestTrue(TEXT("Wide Swing's native base is URpgGameplayAbility"), GetNativeBase(SpellClass) == URpgGameplayAbility::StaticClass());
	TestTrue(
		TEXT("Wide Swing is predicted by its caster"),
		Spell->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	TestTrue(
		TEXT("Wide Swing runs independently of the held swing"),
		Spell->GetActivationGroup() == ERpgAbilityActivationGroup::Independent);
	const FGameplayTagContainer* BlockedTags =
		ReadProperty<FGameplayTagContainer>(*Spell, UGameplayAbility::StaticClass(), TEXT("ActivationBlockedTags"));
	TestTrue(
		TEXT("A running harvesting spell blocks Wide Swing"),
		BlockedTags && BlockedTags->HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Active"))));

	// Its cost is an instant spend of Focus that passes only on a full meter.
	const UGameplayEffect* Cost = Spell->GetCostGameplayEffect();
	if (TestNotNull(TEXT("Wide Swing has a cost effect"), Cost))
	{
		TestTrue(TEXT("The cost is instant"), Cost->DurationPolicy == EGameplayEffectDurationType::Instant);
		TestTrue(
			TEXT("The cost spends Focus"),
			Cost->Modifiers.ContainsByPredicate([](const FGameplayModifierInfo& Modifier)
			{
				return Modifier.Attribute == URpgFocusSet::GetFocusAttribute();
			}));
	}
	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	URpgAbilitySystemComponent* AbilitySystem = MakeTestAbilitySystem(*this, TestWorld.GetWorld());
	if (AbilitySystem && Cost)
	{
		AbilitySystem->AddAttributeSetSubobject(NewObject<URpgFocusSet>(AbilitySystem->GetOwner(), NAME_None, RF_Transient));
		const FGameplayAbilitySpecHandle SpellHandle = AbilitySystem->GiveAbility(FGameplayAbilitySpec(SpellClass));
		const FGameplayAbilityActorInfo* ActorInfo = AbilitySystem->AbilityActorInfo.Get();
		// The engine's cost check is public; the project's override narrows its access.
		const UGameplayAbility* SpellBase = Spell;
		const float MaxFocus = AbilitySystem->GetNumericAttribute(URpgFocusSet::GetMaxFocusAttribute());
		AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetFocusAttribute(), MaxFocus - 1.0f);
		TestFalse(TEXT("A meter one point short of full cannot pay for Wide Swing"), SpellBase->CheckCost(SpellHandle, ActorInfo));
		AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetFocusAttribute(), MaxFocus);
		TestTrue(TEXT("A full meter pays for Wide Swing"), SpellBase->CheckCost(SpellHandle, ActorInfo));
	}

	// The spell's effect: timed, unstacked, cleared on respawn, and widening the tool swings inside the native clamps.
	const UClass* EffectClass = LoadClass<UGameplayEffect>(nullptr, WideSwingEffectClassPath);
	const UGameplayEffect* Effect = EffectClass ? GetDefault<UGameplayEffect>(EffectClass) : nullptr;
	if (!TestNotNull(TEXT("The Wide Swing effect loads"), Effect))
	{
		return false;
	}
	float DurationSeconds = 0.0f;
	TestTrue(
		TEXT("The Wide Swing effect lasts a fixed time above 0"),
		Effect->DurationPolicy == EGameplayEffectDurationType::HasDuration &&
			Effect->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, DurationSeconds) && DurationSeconds > 0.0f);
	TestTrue(TEXT("The Wide Swing effect does not stack"), Effect->GetStackingType() == EGameplayEffectStackingType::None);
	TestTrue(
		TEXT("The Wide Swing effect grants the running-spell tag"),
		Effect->GetGrantedTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Active"))));
	TestTrue(
		TEXT("The Wide Swing effect grants its own tag, which selects the spectral hits"),
		Effect->GetGrantedTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.WideSwing"))));
	TestTrue(
		TEXT("Respawn clears the Wide Swing effect"),
		Effect->GetAssetTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Effect.Behavior.ClearOnRespawn"))));
	const FGameplayTag LoopingCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.Spell.WideSwing"));
	TestTrue(
		TEXT("The Wide Swing effect plays its looping cue while active"),
		Effect->GameplayCues.ContainsByPredicate([LoopingCue](const FGameplayEffectCue& Cue)
		{
			return Cue.GameplayCueTags.HasTagExact(LoopingCue);
		}));
	const URpgAbilityTuningEffectComponent* Tunings = Effect->FindComponent<URpgAbilityTuningEffectComponent>();
	if (TestNotNull(TEXT("The Wide Swing effect carries Ability Tunings"), Tunings))
	{
		const FGameplayTag AxeChop = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.AxeChop"));
		const FGameplayTag PickaxeStrike = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.PickaxeStrike"));
		bool bAxeReach = false;
		bool bAxeTargets = false;
		for (const FRpgSkillTreeAbilityTuning& Tuning : Tunings->AbilityTunings)
		{
			const FString Entry = FString::Printf(TEXT("%s on %s"), *Tuning.TuningTag.ToString(), *Tuning.AbilityIdTag.ToString());
			TestTrue(
				*FString::Printf(TEXT("%s names a tool swing"), *Entry),
				Tuning.AbilityIdTag == AxeChop || Tuning.AbilityIdTag == PickaxeStrike);
			TestTrue(*FString::Printf(TEXT("%s adds, so talents can stack on it"), *Entry), Tuning.Operation == ERpgSkillTreeTuningOperation::Add);
			if (Tuning.TuningTag == Ability_Tuning_Harvest_SwingReach)
			{
				TestTrue(
					*FString::Printf(TEXT("%s reaches farther, at most %.0f cm"), *Entry, URpgGameplayAbility_Harvest::MaxSwingReach),
					Tuning.Value > 0.0f && Tuning.Value <= URpgGameplayAbility_Harvest::MaxSwingReach);
				bAxeReach |= Tuning.AbilityIdTag == AxeChop;
			}
			else if (Tuning.TuningTag == Ability_Tuning_Harvest_SwingTargets)
			{
				TestTrue(
					*FString::Printf(TEXT("%s strikes more resources, at most %d"), *Entry, URpgGameplayAbility_Harvest::MaxSwingTargets),
					Tuning.Value >= 1.0f && 1.0f + Tuning.Value <= URpgGameplayAbility_Harvest::MaxSwingTargets);
				bAxeTargets |= Tuning.AbilityIdTag == AxeChop;
			}
			else
			{
				AddError(FString::Printf(TEXT("%s is neither swing reach nor swing targets"), *Entry));
			}
		}
		TestTrue(TEXT("Wide Swing widens the axe's reach"), bAxeReach);
		TestTrue(TEXT("Wide Swing lets the axe strike more resources"), bAxeTargets);
	}

	// The caster wears a looping cue that every machine sees.
	const UClass* LoopingCueClass = LoadClass<AGameplayCueNotify_Actor>(nullptr, WideSwingCueClassPath);
	TestTrue(
		TEXT("The looping Wide Swing cue is an actor notify for its tag"),
		LoopingCueClass && GetDefault<AGameplayCueNotify_Actor>(LoopingCueClass)->GameplayCueTag == LoopingCue);

	// While it runs, both swings play a spectral variant of their hit: a form cue keyed on Harvest.Spell.WideSwing, an
	// impact notify the swinging client can predict.
	const FGameplayTag WideSwingOwned = FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.WideSwing"));
	const TPair<const TCHAR*, const TCHAR*> SwingCues[] = {
		{AxeChopAbilityClassPath, SpectralToolHitCueClassPath},
		{PickaxeStrikeAbilityClassPath, SpectralStoneToolHitCueClassPath}};
	for (const TPair<const TCHAR*, const TCHAR*>& SwingCue : SwingCues)
	{
		UClass* SwingClass = LoadClass<URpgGameplayAbility_Harvest>(nullptr, SwingCue.Key);
		const URpgGameplayAbility_Harvest* Swing = SwingClass ? GetDefault<URpgGameplayAbility_Harvest>(SwingClass) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), SwingCue.Key), Swing))
		{
			continue;
		}
		const FGameplayTag* SuccessCue =
			ReadProperty<FGameplayTag>(*Swing, URpgGameplayAbility_Harvest::StaticClass(), TEXT("SuccessGameplayCue"));
		const TArray<FRpgHarvestFormGameplayCue>* FormCues = ReadProperty<TArray<FRpgHarvestFormGameplayCue>>(
			*Swing, URpgGameplayAbility_Harvest::StaticClass(), TEXT("FormSuccessGameplayCues"));
		const FRpgHarvestFormGameplayCue* SpectralForm = FormCues
			? FormCues->FindByPredicate([WideSwingOwned](const FRpgHarvestFormGameplayCue& Form) { return Form.RequiredOwnerTag == WideSwingOwned; })
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s has a form cue for Wide Swing"), SwingCue.Key), SpectralForm) || !SuccessCue)
		{
			continue;
		}
		TestTrue(
			*FString::Printf(TEXT("%s's Wide Swing cue %s is a variant of its hit cue"), SwingCue.Key, *SpectralForm->GameplayCue.ToString()),
			SpectralForm->GameplayCue.MatchesTag(*SuccessCue) && SpectralForm->GameplayCue != *SuccessCue);
		const UClass* CueClass = LoadClass<UGameplayCueNotify_Static>(nullptr, SwingCue.Value);
		TestTrue(
			*FString::Printf(TEXT("The Wide Swing hit of %s is an impact notify, so the swinging client predicts it"), SwingCue.Key),
			CueClass && CueClass->IsChildOf<URpgGameplayCueNotify_Impact>() &&
				GetDefault<UGameplayCueNotify_Static>(CueClass)->GameplayCueTag == SpectralForm->GameplayCue);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
