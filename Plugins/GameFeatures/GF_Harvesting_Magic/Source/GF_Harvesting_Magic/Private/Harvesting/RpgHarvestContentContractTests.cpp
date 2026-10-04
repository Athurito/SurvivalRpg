#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameFramework/Actor.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"

namespace RpgHarvestContentContractTests
{
	const TCHAR* PickaxeAbilitySetPath = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Pickaxe.AS_Tool_Pickaxe");
	const TCHAR* IronVeinClassPath = TEXT("/GF_Harvesting_Magic/Harvesting/Nodes/BP_HarvestNode_IronVein.BP_HarvestNode_IronVein_C");

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

#endif // WITH_DEV_AUTOMATION_TESTS
