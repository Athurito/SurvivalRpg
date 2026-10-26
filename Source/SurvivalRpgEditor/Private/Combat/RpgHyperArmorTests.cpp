#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHyperArmorStaggerMeterTest,
	"SurvivalRpg.Combat.HyperArmor.MeterWaitsBelowTheBreakUntilTheTagIsGone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHyperArmorStaggerMeterTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	ON_SCOPE_EXIT
	{
		GameInstance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		GameInstance->RemoveFromRoot();
	};

	// Initialized actors run PostInitializeComponents on spawn, where the fixture registers its attribute sets.
	if (World && !World->AreActorsInitialized())
	{
		World->InitializeActorsForPlay(FURL());
	}
	ARpgCombatNetworkTargetFixture* Target = World ? World->SpawnActor<ARpgCombatNetworkTargetFixture>() : nullptr;
	URpgAbilitySystemComponent* AbilitySystem = Target ? Target->GetRpgAbilitySystemComponent() : nullptr;
	if (!TestNotNull(TEXT("The target and its ability system exist"), AbilitySystem))
	{
		return false;
	}

	const FGameplayTag Staggerable = FGameplayTag::RequestGameplayTag(TEXT("Trait.Staggerable"));
	const FGameplayTag HyperArmor = FGameplayTag::RequestGameplayTag(TEXT("State.HyperArmor"));
	AbilitySystem->AddLooseGameplayTag(Staggerable);
	AbilitySystem->SetNumericAttributeBase(URpgDefenseSet::GetMaxStaggerAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);

	UGameplayEffect* StaggerHit = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
	StaggerHit->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = StaggerHit->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URpgDefenseSet::GetStaggerAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(60.0f));
	auto Hit = [AbilitySystem, StaggerHit]()
	{
		AbilitySystem->ApplyGameplayEffectToSelf(StaggerHit, 1.0f, AbilitySystem->MakeEffectContext());
	};
	auto Meter = [AbilitySystem]()
	{
		return AbilitySystem->GetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute());
	};

	AbilitySystem->AddLooseGameplayTag(HyperArmor);
	Hit();
	Hit();
	TestEqual(TEXT("Under hyper armor the meter waits one point below the break"), Meter(), 99.0f);

	AbilitySystem->RemoveLooseGameplayTag(HyperArmor);
	Hit();
	TestEqual(TEXT("The first hit after hyper armor breaks the meter and resets it"), Meter(), 0.0f);
	return !HasAnyErrors();
}

#endif
