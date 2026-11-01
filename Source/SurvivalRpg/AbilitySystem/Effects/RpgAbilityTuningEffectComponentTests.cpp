#if WITH_DEV_AUTOMATION_TESTS

#include "RpgAbilityTuningEffectComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "NativeGameplayTags.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_Value, "Ability.Tuning.AutomationEffect.Value");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_Other, "Ability.Tuning.AutomationEffect.Other");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_Swing, "Ability.AutomationEffect.Swing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_HeavySwing, "Ability.AutomationEffect.Swing.Heavy");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_Strike, "Ability.AutomationEffect.Strike");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffectTuningTest_Gate, "State.AutomationEffect.Gate");

namespace RpgAbilityTuningEffectTests
{
	constexpr float BaseValue = 10.0f;

	FRpgSkillTreeAbilityTuning MakeTuning(
		const FGameplayTag AbilityId,
		const ERpgSkillTreeTuningOperation Operation,
		const float Value,
		const FGameplayTag TuningTag = TAG_EffectTuningTest_Value)
	{
		FRpgSkillTreeAbilityTuning Tuning;
		Tuning.AbilityIdTag = AbilityId;
		Tuning.TuningTag = TuningTag;
		Tuning.Operation = Operation;
		Tuning.Value = Value;
		return Tuning;
	}

	/** A spec without source item, so only active effects can tune it, named by AbilityId when it is valid. */
	FGameplayAbilitySpec MakeSpec(const FGameplayTag AbilityId = FGameplayTag())
	{
		FGameplayAbilitySpec Spec(URpgInventoryAutomationTestUseAbility::StaticClass());
		if (AbilityId.IsValid())
		{
			Spec.GetDynamicSpecSourceTags().AddTag(AbilityId);
		}
		return Spec;
	}

	/** A standalone world with an authoritative pawn that owns its ability system. */
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			Pawn = World ? World->SpawnActor<APawn>() : nullptr;
			if (Pawn)
			{
				AbilitySystem = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
				Pawn->AddInstanceComponent(AbilitySystem);
				AbilitySystem->RegisterComponent();
				AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);
			}
		}

		~FFixture()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GFrameCounter = CachedFrameCounter;
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("The test world exists"), World) &&
				Test.TestNotNull(TEXT("The ability system exists"), AbilitySystem) &&
				Test.TestTrue(TEXT("The ability system has actor info"), AbilitySystem->AbilityActorInfo.IsValid());
		}

		/** The value of TuningTag Spec reads through the production entry point. */
		float Tuned(const FGameplayAbilitySpec& Spec, const FGameplayTag TuningTag = TAG_EffectTuningTest_Value) const
		{
			return URpgGameplayAbility::GetTunedValueForSpec(Spec, *AbilitySystem->AbilityActorInfo, TuningTag, BaseValue);
		}

		FActiveGameplayEffectHandle Apply(const UGameplayEffect* Effect) const
		{
			return AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.0f, AbilitySystem->MakeEffectContext());
		}

		/** Gives a timer set outside a tick its expire time (see the GAS test pitfalls). */
		void PrimeTimerManager() const
		{
			++GFrameCounter;
			World->GetTimerManager().Tick(0.0f);
		}

		void AdvanceTimers(const float DeltaSeconds) const
		{
			World->TimeSeconds += DeltaSeconds;
			World->UnpausedTimeSeconds += DeltaSeconds;
			World->RealTimeSeconds += DeltaSeconds;
			++GFrameCounter;
			World->GetTimerManager().Tick(DeltaSeconds);
		}

		UWorld* World = nullptr;
		APawn* Pawn = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

#if WITH_EDITOR
	struct FValidation
	{
		EDataValidationResult Result = EDataValidationResult::NotValidated;
		uint32 Errors = 0;
		uint32 Warnings = 0;
	};

	/** Validates Effect's tuning component as the effect editor does. */
	FValidation Validate(const UGameplayEffect* Effect)
	{
		FValidation Validation;
		const URpgAbilityTuningEffectComponent* Component =
			Effect ? Effect->FindComponent<URpgAbilityTuningEffectComponent>() : nullptr;
		if (Component)
		{
			FDataValidationContext Context;
			Validation.Result = Component->IsDataValid(Context);
			Validation.Errors = Context.GetNumErrors();
			Validation.Warnings = Context.GetNumWarnings();
		}
		return Validation;
	}
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningLifetimeTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.EffectTunesWhileActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityTuningEffectTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const FGameplayAbilitySpec Spec = MakeSpec();
	const UGameplayEffect* Spell = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 5.0f)});
	const UGameplayEffect* Brief = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Multiply, 2.0f)}, 0.5f);
	UGameplayEffect* Plain = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
	Plain->DurationPolicy = EGameplayEffectDurationType::Infinite;

	TestEqual(TEXT("Without effects a spec without source item keeps its base"), Fixture.Tuned(Spec), BaseValue);
	TestTrue(TEXT("An effect without Ability Tunings applies"), Fixture.Apply(Plain).IsValid());
	TestEqual(TEXT("An effect without Ability Tunings tunes nothing"), Fixture.Tuned(Spec), BaseValue);

	const FActiveGameplayEffectHandle SpellHandle = Fixture.Apply(Spell);
	TestTrue(TEXT("The spell effect applies"), SpellHandle.IsValid());
	TestEqual(TEXT("The active effect tunes an ability without source item"), Fixture.Tuned(Spec), 15.0f);
	TestEqual(TEXT("Only the named tuning tag changes"), Fixture.Tuned(Spec, TAG_EffectTuningTest_Other), BaseValue);
	TestEqual(TEXT("An invalid tuning tag keeps the base"), Fixture.Tuned(Spec, FGameplayTag()), BaseValue);

	Fixture.PrimeTimerManager();
	const FActiveGameplayEffectHandle BriefHandle = Fixture.Apply(Brief);
	TestTrue(TEXT("The brief effect applies"), BriefHandle.IsValid());
	TestEqual(TEXT("Two effects fold together: Adds, then Multiplies"), Fixture.Tuned(Spec), 30.0f);
	Fixture.AdvanceTimers(0.55f);
	TestNull(TEXT("The brief effect expired"), Fixture.AbilitySystem->GetActiveGameplayEffect(BriefHandle));
	TestEqual(TEXT("Expiry takes the brief effect's tuning away"), Fixture.Tuned(Spec), 15.0f);

	TestTrue(TEXT("The spell effect is removed"), Fixture.AbilitySystem->RemoveActiveGameplayEffect(SpellHandle));
	TestEqual(TEXT("Removal restores the base"), Fixture.Tuned(Spec), BaseValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningInhibitionTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.InhibitedEffectIsIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningInhibitionTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityTuningEffectTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const FGameplayAbilitySpec Spec = MakeSpec();
	// A rune modifier: active only while its gate tag is owned, as OngoingTagRequirements switch it.
	UGameplayEffect* Modifier = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 5.0f)});
	Modifier->AddComponent<UTargetTagRequirementsGameplayEffectComponent>().OngoingTagRequirements.RequireTags.AddTag(
		TAG_EffectTuningTest_Gate);

	const FActiveGameplayEffectHandle Handle = Fixture.Apply(Modifier);
	auto IsInhibited = [&Fixture, Handle]()
	{
		const FActiveGameplayEffect* Active = Fixture.AbilitySystem->GetActiveGameplayEffect(Handle);
		return !Active || Active->bIsInhibited;
	};
	if (!TestNotNull(TEXT("The modifier is active"), Fixture.AbilitySystem->GetActiveGameplayEffect(Handle)))
	{
		return false;
	}
	TestTrue(TEXT("Without its gate tag the modifier is inhibited"), IsInhibited());
	TestEqual(TEXT("An inhibited effect tunes nothing"), Fixture.Tuned(Spec), BaseValue);

	Fixture.AbilitySystem->AddLooseGameplayTag(TAG_EffectTuningTest_Gate);
	TestFalse(TEXT("The gate tag wakes the modifier"), IsInhibited());
	TestEqual(TEXT("The awake modifier tunes"), Fixture.Tuned(Spec), 15.0f);

	Fixture.AbilitySystem->RemoveLooseGameplayTag(TAG_EffectTuningTest_Gate);
	TestTrue(TEXT("Losing the gate tag inhibits the modifier again"), IsInhibited());
	TestEqual(TEXT("The inhibited modifier stops tuning"), Fixture.Tuned(Spec), BaseValue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningAbilityIdTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.AbilityIdFilterIsExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningAbilityIdTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityTuningEffectTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const FGameplayAbilitySpec Swing = MakeSpec(TAG_EffectTuningTest_Swing);
	const FGameplayAbilitySpec HeavySwing = MakeSpec(TAG_EffectTuningTest_HeavySwing);
	const FGameplayAbilitySpec Strike = MakeSpec(TAG_EffectTuningTest_Strike);
	const FGameplayAbilitySpec Unnamed = MakeSpec();
	TestTrue(TEXT("The spell effect applies"), Fixture.Apply(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({
		MakeTuning(TAG_EffectTuningTest_Swing, ERpgSkillTreeTuningOperation::Add, 5.0f),
		MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 1.0f, TAG_EffectTuningTest_Other)})).IsValid());

	TestEqual(TEXT("An entry tunes the ability it names"), Fixture.Tuned(Swing), 15.0f);
	TestEqual(TEXT("An entry never tunes a child id of the named ability"), Fixture.Tuned(HeavySwing), BaseValue);
	TestEqual(TEXT("An entry never tunes another ability"), Fixture.Tuned(Strike), BaseValue);
	TestEqual(TEXT("An entry never tunes an ability without id"), Fixture.Tuned(Unnamed), BaseValue);
	TestEqual(TEXT("An entry without id tunes every ability"), Fixture.Tuned(Strike, TAG_EffectTuningTest_Other), 11.0f);
	TestEqual(TEXT("An entry without id tunes abilities without id"), Fixture.Tuned(Unnamed, TAG_EffectTuningTest_Other), 11.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningStackTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.StackCountIsIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningStackTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityTuningEffectTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const FGameplayAbilitySpec Spec = MakeSpec();
	UGameplayEffect* Stacking = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 5.0f)});
	// SetStackingType is not exported; assets set the same property.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Stacking->StackingType = EGameplayEffectStackingType::AggregateByTarget;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Stacking->StackLimitCount = 3;

	FActiveGameplayEffectHandle Handle;
	for (int32 Application = 0; Application < 3; ++Application)
	{
		Handle = Fixture.Apply(Stacking);
	}
	TestEqual(TEXT("Three applications stack on one active effect"), Fixture.AbilitySystem->GetCurrentStackCount(Handle), 3);
	TestEqual(TEXT("A stacked effect tunes once"), Fixture.Tuned(Spec), 15.0f);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningValidationTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgAbilityTuningEffectTests;
	const FRpgSkillTreeAbilityTuning Named = MakeTuning(TAG_EffectTuningTest_Swing, ERpgSkillTreeTuningOperation::Add, 150.0f);

	const FValidation Valid = Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({Named}));
	TestEqual(TEXT("A timed effect with a named Add entry is valid"), Valid.Result, EDataValidationResult::Valid);
	TestEqual(TEXT("A timed effect with a named Add entry has no warnings"), Valid.Warnings, 0u);

	auto ExpectInvalid = [this](const TCHAR* What, const FValidation& Validation)
	{
		TestEqual(What, Validation.Result, EDataValidationResult::Invalid);
		TestTrue(FString::Printf(TEXT("%s reports an error"), What), Validation.Errors > 0);
	};
	FRpgSkillTreeAbilityTuning MissingTag = Named;
	MissingTag.TuningTag = FGameplayTag();
	ExpectInvalid(TEXT("An entry without TuningTag is rejected"),
		Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({MissingTag})));
	FRpgSkillTreeAbilityTuning ForeignTag = Named;
	ForeignTag.TuningTag = TAG_EffectTuningTest_Strike;
	ExpectInvalid(TEXT("A TuningTag outside Ability.Tuning is rejected"),
		Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({ForeignTag})));
	FRpgSkillTreeAbilityTuning NotANumber = Named;
	NotANumber.Value = std::numeric_limits<float>::quiet_NaN();
	ExpectInvalid(TEXT("A NaN value is rejected"),
		Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({NotANumber})));
	FRpgSkillTreeAbilityTuning Infinite = Named;
	Infinite.Value = std::numeric_limits<float>::infinity();
	ExpectInvalid(TEXT("An infinite value is rejected"),
		Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({Infinite})));
	UGameplayEffect* Instant = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({Named});
	Instant->DurationPolicy = EGameplayEffectDurationType::Instant;
	ExpectInvalid(TEXT("An Instant effect is rejected"), Validate(Instant));

	auto ExpectWarning = [this](const TCHAR* What, const FValidation& Validation)
	{
		TestNotEqual(What, Validation.Result, EDataValidationResult::Invalid);
		TestEqual(FString::Printf(TEXT("%s warns once"), What), Validation.Warnings, 1u);
	};
	ExpectWarning(TEXT("An entry without ability id is allowed"), Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 150.0f)})));
	ExpectWarning(TEXT("A Set entry is allowed"), Validate(URpgAbilityTuningEffectComponent::MakeTransientEffectForTests(
		{MakeTuning(TAG_EffectTuningTest_Swing, ERpgSkillTreeTuningOperation::Set, 3.0f)})));
	UGameplayEffect* Stacking = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({Named});
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Stacking->StackingType = EGameplayEffectStackingType::AggregateBySource;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ExpectWarning(TEXT("A stacking effect is allowed"), Validate(Stacking));
	return true;
}
#endif

#endif
