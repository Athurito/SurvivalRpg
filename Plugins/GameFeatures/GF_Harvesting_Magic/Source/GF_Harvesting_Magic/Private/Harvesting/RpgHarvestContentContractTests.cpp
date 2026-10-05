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

namespace RpgHarvestContentContractTests
{
	const TCHAR* PickaxeAbilitySetPath = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Pickaxe.AS_Tool_Pickaxe");
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestPickaxeAbilitySetContractTest,
	"SurvivalRpg.Harvesting.Content.PickaxeAbilitySetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestPickaxeAbilitySetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	const URpgAbilitySet* AbilitySet = LoadObject<URpgAbilitySet>(nullptr, PickaxeAbilitySetPath);
	if (!TestNotNull(TEXT("The pickaxe ability set loads"), AbilitySet))
	{
		return false;
	}

	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
	if (!TestNotNull(TEXT("Grant owner spawns"), Owner))
	{
		return false;
	}
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Owner, Owner);
	AbilitySystem->SetForceGrantAuthorityForTests(true);
	FRpgAbilitySet_GrantedHandles Granted;
	AbilitySet->GiveToAbilitySystem(AbilitySystem, &Granted);

	// Pickaxe Strike: the swing on the main-hand primary input.
	const FGameplayTag StrikeId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.PickaxeStrike"));
	const FGameplayAbilitySpec* Strike = FindSpecWithId(*AbilitySystem, StrikeId);
	if (!TestNotNull(TEXT("The set grants Pickaxe Strike"), Strike))
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* StrikeAbility = Cast<URpgGameplayAbility_Harvest>(Strike->Ability);
	TestNotNull(TEXT("Pickaxe Strike derives from the harvest ability base"), StrikeAbility);
	TestTrue(
		TEXT("Pickaxe Strike is bound to the primary input"),
		Strike->GetDynamicSpecSourceTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"))));
	TestTrue(TEXT("Pickaxe Strike executes on press"), StrikeAbility && !StrikeAbility->IsAimWhileInputHeld());
	TestTrue(TEXT("Pickaxe Strike can strike weak points"), StrikeAbility && StrikeAbility->CanHitWeakPoints());

	// Rift Grip: the awakened hold-to-aim ability, declared as the default of weapon ability slot 1.
	const FGameplayTag RiftGripId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.RiftGrip"));
	const FGameplayAbilitySpec* RiftGrip = FindSpecWithId(*AbilitySystem, RiftGripId);
	if (!TestNotNull(TEXT("The set grants Rift Grip"), RiftGrip))
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* RiftGripAbility = Cast<URpgGameplayAbility_Harvest>(RiftGrip->Ability);
	TestNotNull(TEXT("Rift Grip derives from the harvest ability base"), RiftGripAbility);
	TestTrue(TEXT("Rift Grip aims while its input is held"), RiftGripAbility && RiftGripAbility->IsAimWhileInputHeld());
	TestFalse(TEXT("Rift Grip never strikes weak points"), RiftGripAbility && RiftGripAbility->CanHitWeakPoints());
	TestTrue(
		TEXT("Rift Grip is the declared default of weapon ability slot 1"),
		RiftGrip->GetDynamicSpecSourceTags().HasTagExact(URpgWeaponAbilityLoadoutComponent::GetDefaultSelectionTagForSlotIndex(0)));
	TestTrue(
		TEXT("Rift Grip has a cooldown"),
		RiftGripAbility && RiftGripAbility->GetCooldownGameplayEffect() != nullptr);

	FGameplayTag DefaultId;
	TestEqual(
		TEXT("Slot 1 resolves its default to Rift Grip alone"),
		URpgWeaponAbilityLoadoutComponent::ResolveDefaultAbilityId(*AbilitySystem, 0, DefaultId),
		ERpgAbilityBindingResolveResult::Unique);
	TestEqual(TEXT("The slot 1 default is Rift Grip"), DefaultId, RiftGripId);
	TestEqual(
		TEXT("The Rift Grip id resolves to exactly one granted spec"),
		FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystem, RiftGripId).Result,
		ERpgAbilityBindingResolveResult::Unique);
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
	FRpgHarvestAxeAbilitySetContractTest,
	"SurvivalRpg.Harvesting.Content.AxeAbilitySetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestAxeAbilitySetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestContentContractTests;

	const URpgAbilitySet* AbilitySet = LoadObject<URpgAbilitySet>(nullptr, AxeAbilitySetPath);
	if (!TestNotNull(TEXT("The axe ability set loads"), AbilitySet))
	{
		return false;
	}

	RpgHarvestAutomation::FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
	if (!TestNotNull(TEXT("Grant owner spawns"), Owner))
	{
		return false;
	}
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Owner, Owner);
	AbilitySystem->SetForceGrantAuthorityForTests(true);
	FRpgAbilitySet_GrantedHandles Granted;
	AbilitySet->GiveToAbilitySystem(AbilitySystem, &Granted);

	// Axe Chop: the swing on the main-hand primary input.
	const FGameplayTag ChopId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.AxeChop"));
	const FGameplayAbilitySpec* Chop = FindSpecWithId(*AbilitySystem, ChopId);
	if (!TestNotNull(TEXT("The set grants Axe Chop"), Chop))
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* ChopAbility = Cast<URpgGameplayAbility_Harvest>(Chop->Ability);
	TestNotNull(TEXT("Axe Chop derives from the harvest ability base"), ChopAbility);
	TestTrue(
		TEXT("Axe Chop is bound to the primary input"),
		Chop->GetDynamicSpecSourceTags().HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"))));
	TestTrue(TEXT("Axe Chop executes on press"), ChopAbility && !ChopAbility->IsAimWhileInputHeld());
	TestTrue(TEXT("Axe Chop sends its own ability id"), ChopAbility && ChopAbility->GetHarvestAbilityId() == ChopId);

	// Death Wave: the awakened hold-to-aim area felling, declared as the default of weapon ability slot 1.
	const FGameplayTag DeathWaveId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.DeathWave"));
	const FGameplayAbilitySpec* DeathWave = FindSpecWithId(*AbilitySystem, DeathWaveId);
	if (!TestNotNull(TEXT("The set grants Death Wave"), DeathWave))
	{
		return false;
	}
	const URpgGameplayAbility_Harvest* DeathWaveAbility = Cast<URpgGameplayAbility_Harvest>(DeathWave->Ability);
	TestNotNull(TEXT("Death Wave derives from the harvest ability base"), DeathWaveAbility);
	TestTrue(TEXT("Death Wave aims while its input is held"), DeathWaveAbility && DeathWaveAbility->IsAimWhileInputHeld());
	TestTrue(TEXT("Death Wave harvests an area"), DeathWaveAbility && DeathWaveAbility->HarvestsArea());
	TestTrue(TEXT("Death Wave sends its own ability id"), DeathWaveAbility && DeathWaveAbility->GetHarvestAbilityId() == DeathWaveId);
	TestTrue(
		TEXT("Death Wave is the declared default of weapon ability slot 1"),
		DeathWave->GetDynamicSpecSourceTags().HasTagExact(URpgWeaponAbilityLoadoutComponent::GetDefaultSelectionTagForSlotIndex(0)));
	TestTrue(TEXT("Death Wave has a cooldown"), DeathWaveAbility && DeathWaveAbility->GetCooldownGameplayEffect() != nullptr);

	FGameplayTag DefaultId;
	TestEqual(
		TEXT("Slot 1 resolves its default to Death Wave alone"),
		URpgWeaponAbilityLoadoutComponent::ResolveDefaultAbilityId(*AbilitySystem, 0, DefaultId),
		ERpgAbilityBindingResolveResult::Unique);
	TestEqual(TEXT("The slot 1 default is Death Wave"), DefaultId, DeathWaveId);

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
