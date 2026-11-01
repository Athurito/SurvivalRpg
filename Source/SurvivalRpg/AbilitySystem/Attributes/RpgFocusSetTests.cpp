#if WITH_DEV_AUTOMATION_TESTS

#include "RpgFocusSet.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "UObject/StrongObjectPtr.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_FocusSetTest_Gain, "SetByCaller.AutomationFocus.Gain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_FocusSetTest_Spell, "State.AutomationFocus.Spell");

namespace RpgFocusSetTests
{
	/** A standalone world with an authoritative pawn whose ability system owns a focus set. */
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			APawn* Pawn = World ? World->SpawnActor<APawn>() : nullptr;
			if (Pawn)
			{
				AbilitySystem = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
				Pawn->AddInstanceComponent(AbilitySystem);
				AbilitySystem->RegisterComponent();
				AbilitySystem->AddAttributeSetSubobject(NewObject<URpgFocusSet>(Pawn, NAME_None, RF_Transient));
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
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("The test world exists"), World) &&
				Test.TestNotNull(TEXT("The ability system exists"), AbilitySystem) &&
				Test.TestTrue(
					TEXT("The ability system owns the focus set"),
					AbilitySystem->HasAttributeSetForAttribute(URpgFocusSet::GetFocusAttribute()));
		}

		float GetFocus() const { return AbilitySystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute()); }
		float GetMaxFocus() const { return AbilitySystem->GetNumericAttribute(URpgFocusSet::GetMaxFocusAttribute()); }

		/** Applies Effect instantly with Amount as its gain magnitude; returns whether it applied. */
		bool ApplyGain(const UGameplayEffect* Effect, const float Amount) const
		{
			FGameplayEffectSpec Spec(Effect, AbilitySystem->MakeEffectContext(), 1.0f);
			Spec.SetSetByCallerMagnitude(TAG_FocusSetTest_Gain, Amount);
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied();
		}

		UWorld* World = nullptr;
		URpgAbilitySystemComponent* AbilitySystem = nullptr;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
	};

	/** An instant effect that adds its SetByCaller magnitude to Focus, as the harvest focus gain effect does. */
	UGameplayEffect* MakeGainEffect()
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FSetByCallerFloat Gain;
		Gain.DataTag = TAG_FocusSetTest_Gain;
		FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = URpgFocusSet::GetFocusAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Gain);
		return Effect;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFocusSetClampTest,
	"SurvivalRpg.AbilitySystem.Attributes.FocusSet.ClampsAndFollowsLoweredMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgFocusSetClampTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgFocusSetTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	const UGameplayEffect* Gain = MakeGainEffect();

	TestEqual(TEXT("A new meter starts empty"), Fixture.GetFocus(), 0.0f);
	TestEqual(TEXT("A full meter holds 100 by default"), Fixture.GetMaxFocus(), 100.0f);

	TestTrue(TEXT("A gain applies"), Fixture.ApplyGain(Gain, 40.0f));
	TestEqual(TEXT("The gain adds to focus"), Fixture.GetFocus(), 40.0f);
	TestTrue(TEXT("A gain beyond the maximum applies"), Fixture.ApplyGain(Gain, 500.0f));
	TestEqual(TEXT("Focus stops at its maximum"), Fixture.GetFocus(), 100.0f);
	Fixture.AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetFocusAttribute(), 250.0f);
	TestEqual(TEXT("A base value beyond the maximum clamps too"), Fixture.GetFocus(), 100.0f);

	Fixture.AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetMaxFocusAttribute(), 60.0f);
	TestEqual(TEXT("A lowered maximum holds"), Fixture.GetMaxFocus(), 60.0f);
	TestEqual(TEXT("Focus follows a lowered maximum"), Fixture.GetFocus(), 60.0f);
	Fixture.AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetMaxFocusAttribute(), 120.0f);
	TestEqual(TEXT("A raised maximum leaves focus where it is"), Fixture.GetFocus(), 60.0f);

	TestTrue(TEXT("A spend applies"), Fixture.ApplyGain(Gain, -500.0f));
	TestEqual(TEXT("Focus never drops below zero"), Fixture.GetFocus(), 0.0f);
	Fixture.AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetMaxFocusAttribute(), 0.0f);
	TestEqual(TEXT("The maximum stays at least 1"), Fixture.GetMaxFocus(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFocusSetReplicationTest,
	"SurvivalRpg.AbilitySystem.Attributes.FocusSet.ReplicatesToOwnerOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgFocusSetReplicationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FLifetimeProperty> LifetimeProperties;
	GetDefault<URpgFocusSet>()->GetLifetimeReplicatedProps(LifetimeProperties);

	for (const FName PropertyName : {GET_MEMBER_NAME_CHECKED(URpgFocusSet, Focus), GET_MEMBER_NAME_CHECKED(URpgFocusSet, MaxFocus)})
	{
		const FProperty* Property = FindFProperty<FProperty>(URpgFocusSet::StaticClass(), PropertyName);
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *PropertyName.ToString()), Property))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s replicates"), *PropertyName.ToString()), Property->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			*FString::Printf(TEXT("%s notifies its owner"), *PropertyName.ToString()),
			Property->HasAnyPropertyFlags(CPF_RepNotify));
		const FLifetimeProperty* Lifetime = LifetimeProperties.FindByPredicate(
			[Property](const FLifetimeProperty& Candidate) { return Candidate.RepIndex == Property->RepIndex; });
		if (TestNotNull(*FString::Printf(TEXT("%s has a lifetime entry"), *PropertyName.ToString()), Lifetime))
		{
			// Partners and late joiners must never learn another player's focus.
			TestEqual(
				*FString::Printf(TEXT("%s replicates to its owner only"), *PropertyName.ToString()),
				static_cast<int32>(Lifetime->Condition),
				static_cast<int32>(COND_OwnerOnly));
			TestEqual(
				*FString::Printf(TEXT("%s notifies on every update"), *PropertyName.ToString()),
				static_cast<int32>(Lifetime->RepNotifyCondition),
				static_cast<int32>(REPNOTIFY_Always));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFocusSetIgnoredTagTest,
	"SurvivalRpg.AbilitySystem.Attributes.FocusSet.GainIgnoresItsBlockingTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgFocusSetIgnoredTagTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgFocusSetTests;
	FFixture Fixture;
	if (!Fixture.IsValid(*this))
	{
		return false;
	}
	// The harvest gain effect ignores the running spell this way, so the spell never refills its own meter.
	UGameplayEffect* Gain = MakeGainEffect();
	Gain->AddComponent<UTargetTagRequirementsGameplayEffectComponent>().ApplicationTagRequirements.IgnoreTags.AddTag(
		TAG_FocusSetTest_Spell);

	TestTrue(TEXT("Without the tag the gain applies"), Fixture.ApplyGain(Gain, 4.0f));
	TestEqual(TEXT("Without the tag focus fills"), Fixture.GetFocus(), 4.0f);

	Fixture.AbilitySystem->AddLooseGameplayTag(TAG_FocusSetTest_Spell);
	TestFalse(TEXT("While the tag is owned the instant gain does not apply"), Fixture.ApplyGain(Gain, 4.0f));
	TestEqual(TEXT("While the tag is owned focus stays"), Fixture.GetFocus(), 4.0f);

	Fixture.AbilitySystem->RemoveLooseGameplayTag(TAG_FocusSetTest_Spell);
	TestTrue(TEXT("Once the tag is gone the gain applies again"), Fixture.ApplyGain(Gain, 4.0f));
	TestEqual(TEXT("Once the tag is gone focus fills again"), Fixture.GetFocus(), 8.0f);
	return true;
}

#endif
