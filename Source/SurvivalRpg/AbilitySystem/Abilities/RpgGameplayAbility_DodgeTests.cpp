#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "RpgGameplayAbility_Dodge.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "Misc/ScopeExit.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgDodgeRootMotionProfileSelectionTest,
	"SurvivalRpg.Combat.Dodge.SelectsEquipmentRootMotionProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgDodgeRootMotionProfileSelectionTest::RunTest(const FString& Parameters)
{
	FRpgDodgeRootMotionTuning Light;
	Light.ProfileName = TEXT("Dodge.Light");
	Light.MontagePlayRate = 1.2f;
	Light.TranslationScale = 1.15f;
	Light.StartSection = TEXT("Forward");

	FRpgDodgeRootMotionTuning Heavy;
	Heavy.ProfileName = TEXT("Dodge.Heavy");
	Heavy.MontagePlayRate = 0.8f;
	Heavy.TranslationScale = 0.7f;

	const TArray<FRpgDodgeRootMotionTuning> Tunings{Light, Heavy};
	const FRpgDodgeRootMotionTuning ResolvedHeavy = URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
		TEXT("Dodge.Heavy"),
		Tunings,
		1.0f,
		1.0f);
	TestEqual(TEXT("Heavy selects its named root-motion profile"), ResolvedHeavy.ProfileName, FName(TEXT("Dodge.Heavy")));
	TestEqual(TEXT("Heavy playback rate is selected without changing movement speed"), ResolvedHeavy.MontagePlayRate, 0.8f);
	TestEqual(TEXT("Heavy translation scale is selected"), ResolvedHeavy.TranslationScale, 0.7f);

	const FRpgDodgeRootMotionTuning Missing = URpgGameplayAbility_Dodge::ResolveRootMotionTuning(
		TEXT("Dodge.Unconfigured"),
		Tunings,
		0.95f,
		0.9f);
	TestEqual(TEXT("Unknown names retain the semantic profile for diagnostics"), Missing.ProfileName, FName(TEXT("Dodge.Unconfigured")));
	TestEqual(TEXT("Unknown names use the ability playback fallback"), Missing.MontagePlayRate, 0.95f);
	TestEqual(TEXT("Unknown names use the ability root-motion fallback"), Missing.TranslationScale, 0.9f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgDodgeProfileLifecycleTest,
	"SurvivalRpg.Combat.Dodge.ProfileLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgDodgeProfileLifecycleTest::RunTest(const FString& Parameters)
{
	UClass* AbilityClass = LoadClass<URpgGameplayAbility_Dodge>(nullptr,
		TEXT("/Game/SurvivalRpg/AbilitySystem/Abilities/GA_Dodge.GA_Dodge_C"));
	if (!TestNotNull(TEXT("Concrete dodge ability loads"), AbilityClass)) return false;

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
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
	};
	if (!TestNotNull(TEXT("Dodge lifecycle world exists"), World)) return false;
	APawn* Pawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Authoritative dodge avatar exists"), Pawn)) return false;
	URpgAbilitySystemComponent* ASC = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
	Pawn->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Pawn, Pawn);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	URpgGameplayAbility_Dodge* Ability = Spec ? Cast<URpgGameplayAbility_Dodge>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Grant creates the per-actor dodge instance"), Ability)) return false;

	const FStructProperty* ProfileProperty = FindFProperty<FStructProperty>(AbilityClass, TEXT("DefaultDodgeProfile"));
	const FFloatProperty* PlayRateProperty = FindFProperty<FFloatProperty>(AbilityClass, TEXT("DefaultMontagePlayRate"));
	UFunction* EndFunction = Ability->FindFunction(TEXT("K2_EndAbility"));
	if (!TestNotNull(TEXT("Dodge exposes profile tuning"), ProfileProperty) ||
		!TestNotNull(TEXT("Dodge exposes playback tuning"), PlayRateProperty) ||
		!TestNotNull(TEXT("GAS exposes its Blueprint end entry point"), EndFunction)) return false;
	// Exercise the native snapshot seam with instance-only tuning; saved designer assets stay untouched.
	*ProfileProperty->ContainerPtrToValuePtr<FRpgEquipmentDodgeProfile>(Ability) = FRpgEquipmentDodgeProfile();
	PlayRateProperty->SetPropertyValue_InContainer(Ability, 1.75f);

	int32 EndCount = 0;
	Ability->OnGameplayAbilityEnded.AddLambda([this, Ability, &EndCount](UGameplayAbility*)
	{
		++EndCount;
		TestFalse(TEXT("End callbacks cannot repopulate the retired snapshot"), Ability->ResolveDodgeProfileForActivation());
	});
	{
		FScopedTargetListLock ScopeLock(*ASC, *Ability);
		TestTrue(TEXT("Dodge activates while cancellation is deferred"), ASC->TryActivateAbility(Handle));
		TestTrue(TEXT("No-montage cancellation waits for the target-list lock"), Ability->IsActive());
		TestTrue(TEXT("Active dodge resolves its profile"), Ability->ResolveDodgeProfileForActivation());
		TestEqual(TEXT("Activation captures its configured playback rate"), Ability->GetResolvedDodgeProfile().MontagePlayRate, 1.75f);
		PlayRateProperty->SetPropertyValue_InContainer(Ability, 0.8f);
		Ability->ProcessEvent(EndFunction, nullptr);
		TestEqual(TEXT("Deferred end preserves the current snapshot"), Ability->GetResolvedDodgeProfile().MontagePlayRate, 1.75f);
		TestTrue(TEXT("Deferred end keeps the activation snapshot readable"), Ability->ResolveDodgeProfileForActivation());
		TestEqual(TEXT("An active snapshot ignores later tuning changes"), Ability->GetResolvedDodgeProfile().MontagePlayRate, 1.75f);
	}
	TestFalse(TEXT("Unlock completes cancellation"), Ability->IsActive());
	TestEqual(TEXT("Queued cancel and end complete the activation once"), EndCount, 1);
	TestEqual(TEXT("Completed end clears the snapshot"), Ability->GetResolvedDodgeProfile().MontagePlayRate, 1.0f);
	TestFalse(TEXT("Inactive dodge cannot resolve a profile"), Ability->ResolveDodgeProfileForActivation());
	{
		FScopedTargetListLock ScopeLock(*ASC, *Ability);
		TestTrue(TEXT("The same instance can activate again"), ASC->TryActivateAbility(Handle));
		TestEqual(TEXT("A new activation captures the new tuning"), Ability->GetResolvedDodgeProfile().MontagePlayRate, 0.8f);
	}
	Ability->OnGameplayAbilityEnded.Clear();
	ASC->ClearAbility(Handle);
	return !HasAnyErrors();
}

#endif
