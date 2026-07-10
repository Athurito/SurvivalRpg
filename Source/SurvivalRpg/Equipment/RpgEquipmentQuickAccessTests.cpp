#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgAbilityBindingResolver.h"
#include "RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

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

#endif
