#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Modules/ModuleManager.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgAbilityBindingResolver.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStoneburstQuickAccessContractTest,
	"SurvivalRpg.Harvesting.Stoneburst.UniqueQuickAccessActivationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStoneburstQuickAccessContractTest::RunTest(const FString& Parameters)
{
	IModuleInterface* HarvestingModule = FModuleManager::Get().LoadModule(TEXT("GF_Harvesting_Magic"));
	TestNotNull(TEXT("GF_Harvesting_Magic runtime module can be loaded by the feature/progression path"), HarvestingModule);
	if (!HarvestingModule)
	{
		return false;
	}

	UClass* StoneburstClass = StaticLoadClass(
		URpgGameplayAbility::StaticClass(),
		nullptr,
		TEXT("/Script/GF_Harvesting_Magic.RpgGameplayAbility_Stoneburst"));
	TestNotNull(TEXT("The feature exposes the native Stoneburst gameplay ability"), StoneburstClass);
	if (!StoneburstClass)
	{
		return false;
	}

	const FGameplayTag AbilityId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.Stoneburst"), false);
	const URpgGameplayAbility* AbilityCDO = Cast<URpgGameplayAbility>(StoneburstClass->GetDefaultObject());
	TestTrue(TEXT("Stoneburst exposes a valid stable ability id"), AbilityId.IsValid());
	TestNotNull(TEXT("Stoneburst derives from the project's GAS ability base"), AbilityCDO);
	if (!AbilityCDO || !AbilityId.IsValid())
	{
		return false;
	}

	TestTrue(TEXT("The native ability advertises its id as a static GAS asset tag"), AbilityCDO->GetAssetTags().HasTagExact(AbilityId));
	TestEqual(
		TEXT("Quick-access activation stays server authoritative"),
		AbilityCDO->GetNetExecutionPolicy(),
		EGameplayAbilityNetExecutionPolicy::ServerOnly);

	URpgAbilitySystemComponent* AbilitySystemComponent = NewObject<URpgAbilitySystemComponent>();
	FGameplayAbilitySpec StoneburstSpec(StoneburstClass);
	AbilitySystemComponent->GetActivatableAbilities().Add(StoneburstSpec);

	FRpgUniqueAbilityBindingResolution Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(
		AbilitySystemComponent,
		AbilityId);
	TestEqual(TEXT("One Stoneburst grant resolves to one activation target"), Resolution.Result, ERpgAbilityBindingResolveResult::Unique);
	TestTrue(TEXT("The activation target is Stoneburst"), Resolution.AbilityCDO == AbilityCDO);
	TestTrue(TEXT("The activation target has a concrete spec handle"), Resolution.SpecHandle.IsValid());

	FGameplayAbilitySpec DuplicateSpec(StoneburstClass);
	AbilitySystemComponent->GetActivatableAbilities().Add(DuplicateSpec);
	AddExpectedError(TEXT("Ability binding blocked"), EAutomationExpectedErrorFlags::Contains, 1);
	Resolution = FRpgAbilityBindingResolver::ResolveUniqueAbilityId(AbilitySystemComponent, AbilityId);
	TestEqual(TEXT("Duplicate grants block activation rather than choosing arbitrarily"), Resolution.Result, ERpgAbilityBindingResolveResult::Ambiguous);
	TestFalse(TEXT("An ambiguous binding exposes no activation handle"), Resolution.SpecHandle.IsValid());
	return true;
}

#endif
