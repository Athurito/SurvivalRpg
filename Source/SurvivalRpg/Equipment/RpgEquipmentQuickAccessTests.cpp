#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgAbilityBindingResolver.h"
#include "RpgEquipmentAutomationTestTypes.h"
#include "RpgEquipmentLoadoutComponent.h"
#include "RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace RpgEquipmentAutomationTests
{
	class FScopedEquipmentWorld
	{
	public:
		FScopedEquipmentWorld()
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

		~FScopedEquipmentWorld()
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

		UWorld* GetWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUniqueAbilityBindingResolverTest,
	"SurvivalRpg.Equipment.AbilityBinding.RequiresUniqueSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUniqueAbilityBindingResolverTest::RunTest(const FString& Parameters)
{
	URpgAbilitySystemComponent* AbilitySystemComponent = NewObject<URpgAbilitySystemComponent>();
	const FGameplayTag AbilityId = RpgGameplayTags::Ability_Attack_Basic;

	FRpgUniqueAbilityBindingResolution Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
		AbilitySystemComponent,
		AbilityId);
	TestEqual(TEXT("An ungranted ability id is missing"), Resolution.Result, ERpgAbilityBindingResolveResult::Missing);

	FGameplayAbilitySpec FirstSpec(URpgGameplayAbility::StaticClass());
	FirstSpec.GetDynamicSpecSourceTags().AddTag(AbilityId);
	AbilitySystemComponent->GetActivatableAbilities().Add(FirstSpec);
	Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystemComponent, AbilityId);
	TestEqual(TEXT("One matching spec resolves uniquely"), Resolution.Result, ERpgAbilityBindingResolveResult::Unique);
	TestEqual(TEXT("Unique resolution reports one match"), Resolution.MatchCount, 1);
	TestTrue(TEXT("Unique resolution returns a concrete spec handle"), Resolution.SpecHandle.IsValid());

	FGameplayAbilitySpec DuplicateSpec(URpgGameplayAbility::StaticClass());
	DuplicateSpec.GetDynamicSpecSourceTags().AddTag(AbilityId);
	AbilitySystemComponent->GetActivatableAbilities().Add(DuplicateSpec);
	AddExpectedError(TEXT("Ability binding blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystemComponent, AbilityId);
	TestEqual(TEXT("Duplicate ids are blocked as ambiguous"), Resolution.Result, ERpgAbilityBindingResolveResult::Ambiguous);
	TestEqual(TEXT("Ambiguous resolution reports every match"), Resolution.MatchCount, 2);
	TestFalse(TEXT("Ambiguous resolution never returns an arbitrary handle"), Resolution.SpecHandle.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessBindingContractTest,
	"SurvivalRpg.Inventory.QuickAccess.FixedEightBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessBindingContractTest::RunTest(const FString& Parameters)
{
	const URpgActionBarComponent* ActionBar = NewObject<URpgActionBarComponent>();
	const TArray<FRpgQuickAccessBinding> Bindings = ActionBar->GetQuickAccessBindings();
	TestEqual(TEXT("Keyboard and radial share exactly eight entries"), Bindings.Num(), 8);
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("Binding %d defaults to Empty"), Index),
			Bindings[Index].SlotType,
			ERpgActionBarSlotType::Empty);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentLoadTierThresholdTest,
	"SurvivalRpg.Equipment.Load.ExactThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentLoadTierThresholdTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("12.99 kg is Light"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(12.99f),
		ERpgEquipmentLoadTier::Light);
	TestEqual(
		TEXT("13 kg enters Medium"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(13.0f),
		ERpgEquipmentLoadTier::Medium);
	TestEqual(
		TEXT("22.99 kg stays Medium"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(22.99f),
		ERpgEquipmentLoadTier::Medium);
	TestEqual(
		TEXT("23 kg enters Heavy"),
		URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(23.0f),
		ERpgEquipmentLoadTier::Heavy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentEmptyAllowedSlotsTest,
	"SurvivalRpg.Equipment.Definition.EmptyAllowedSlotsAreDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentEmptyAllowedSlotsTest::RunTest(const FString& Parameters)
{
	URpgEquipmentAutomationTestSwordDefinition* DisabledDefinition =
		NewObject<URpgEquipmentAutomationTestSwordDefinition>();
	if (!TestNotNull(TEXT("A mutable equipment-definition fixture exists"), DisabledDefinition))
	{
		return false;
	}

	DisabledDefinition->AllowedSlots.Reset();
	TestFalse(
		TEXT("An empty AllowedSlots array does not silently fall back to MainHand"),
		DisabledDefinition->CanEquipInSlot(ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("An empty AllowedSlots array has no default equipment destination"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);

	DisabledDefinition->AllowedSlots = { ERpgEquipmentSlot::OffHand };
	DisabledDefinition->HandOccupancy = ERpgEquipmentHandOccupancy::BothHands;
	TestFalse(
		TEXT("A BothHands definition cannot activate from OffHand even when stale data lists that slot"),
		DisabledDefinition->CanEquipInSlot(ERpgEquipmentSlot::OffHand));
	TestEqual(
		TEXT("An OffHand-only BothHands definition has no unsafe default destination"),
		DisabledDefinition->GetDefaultEquipSlot(),
		ERpgEquipmentSlot::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentPersistentHealthGrantTest,
	"SurvivalRpg.Equipment.Grants.UnchangedMaxHealthEffectSurvivesWeaponEquip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentPersistentHealthGrantTest::RunTest(const FString& Parameters)
{
	RpgEquipmentAutomationTests::FScopedEquipmentWorld TestWorld;
	if (!TestNotNull(TEXT("Standalone equipment test world is available"), TestWorld.GetWorld()))
	{
		return false;
	}

	ARpgEquipmentAutomationTestPawn* Pawn =
		TestWorld.GetWorld()->SpawnActor<ARpgEquipmentAutomationTestPawn>();
	if (!TestNotNull(TEXT("Authoritative GAS equipment pawn is spawned"), Pawn))
	{
		return false;
	}

	URpgAbilitySystemComponent* AbilitySystemComponent = Pawn->GetRpgAbilitySystemComponent();
	URpgEquipmentManagerComponent* EquipmentManager = Pawn->GetEquipmentManagerComponent();
	URpgHealthSet* HealthSet = Pawn->GetHealthSet();
	if (!TestNotNull(TEXT("Pawn owns an ability system"), AbilitySystemComponent) ||
		!TestNotNull(TEXT("Pawn owns an equipment manager"), EquipmentManager) ||
		!TestNotNull(TEXT("Pawn owns a health set"), HealthSet))
	{
		return false;
	}

	// The standalone automation world does not run the complete PlayerState component lifecycle.
	AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);
	AbilitySystemComponent->InitAbilityActorInfo(Pawn, Pawn);
	if (!TestNotNull(TEXT("Health set is registered with GAS"), AbilitySystemComponent->GetSet<URpgHealthSet>()))
	{
		return false;
	}

	TestEqual(TEXT("Fixture starts with 100 MaxHealth"), HealthSet->GetMaxHealth(), 100.0f);

	URpgEquipmentInstance* Helmet = EquipmentManager->EquipItemInSlot(
		URpgEquipmentAutomationTestHelmetDefinition::StaticClass(),
		ERpgEquipmentSlot::Head);
	if (!TestNotNull(TEXT("Helmet equips in the Head slot"), Helmet))
	{
		return false;
	}

	TestEqual(TEXT("Helmet persistent effect raises MaxHealth to 600"), HealthSet->GetMaxHealth(), 600.0f);
	AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), 600.0f);
	TestEqual(TEXT("Fixture can heal to the helmet-adjusted maximum"), HealthSet->GetHealth(), 600.0f);

	URpgEquipmentInstance* Sword = EquipmentManager->EquipItemInSlot(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(),
		ERpgEquipmentSlot::MainHand);
	TestNotNull(TEXT("Sword equips in the MainHand slot"), Sword);
	TestEqual(TEXT("Unrelated weapon equip keeps helmet MaxHealth active"), HealthSet->GetMaxHealth(), 600.0f);
	TestEqual(TEXT("Unrelated weapon equip does not clamp current Health"), HealthSet->GetHealth(), 600.0f);

	return true;
}

#endif
