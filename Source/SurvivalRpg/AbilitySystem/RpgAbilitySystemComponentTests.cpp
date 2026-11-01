#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"

namespace RpgAbilitySetAuthorityTests
{
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FFixture()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			Owner = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Test.TestNotNull(TEXT("The ability system owner spawns"), Owner))
			{
				return false;
			}

			AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
			Owner->AddInstanceComponent(AbilitySystem);
			AbilitySystem->RegisterComponent();
			AbilitySystem->InitAbilityActorInfo(Owner, Owner);

			AbilitySet = NewObject<URpgAbilitySet>(GetTransientPackage(), NAME_None, RF_Transient);
			AbilitySet->AddGrantedGameplayAbility(URpgInventoryAutomationTestUseAbility::StaticClass());
			return Test.TestTrue(TEXT("The server-owned ability system has grant authority"), AbilitySystem->HasGrantAuthority());
		}

		/** Switches the owner between server and client view of the same ability system. */
		void SetOwnerRole(const ENetRole Role)
		{
			Owner->SetRole(Role);
			AbilitySystem->CacheIsNetSimulated();
		}

		bool HasGrantedAbility() const
		{
			for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass() == URpgInventoryAutomationTestUseAbility::StaticClass())
				{
					return true;
				}
			}
			return false;
		}

		UWorld* GetWorld() const { return World; }

		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		URpgAbilitySet* AbilitySet = nullptr;

	private:
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAbilitySetBlueprintAuthorityOnlyTest,
	"SurvivalRpg.AbilitySystem.AbilitySet.BlueprintCallsRequireAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilitySetBlueprintAuthorityOnlyTest::RunTest(const FString& Parameters)
{
	// Blueprint calls on clients are absorbed before they reach native code; no server RPC carries a grant request.
	for (const TCHAR* FunctionName : { TEXT("GrantAbilitySet"), TEXT("RemoveAbilitySet") })
	{
		const UFunction* Function = URpgAbilitySystemComponent::StaticClass()->FindFunctionByName(FunctionName);
		if (TestNotNull(FString::Printf(TEXT("%s is reflected"), FunctionName), Function))
		{
			TestTrue(FString::Printf(TEXT("%s is BlueprintAuthorityOnly"), FunctionName), Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
			TestFalse(FString::Printf(TEXT("%s is not a network function"), FunctionName), Function->HasAnyFunctionFlags(FUNC_Net));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAbilitySetNonAuthorityGrantTest,
	"SurvivalRpg.AbilitySystem.AbilitySet.NonAuthorityCannotGrantOrRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilitySetNonAuthorityGrantTest::RunTest(const FString& Parameters)
{
	RpgAbilitySetAuthorityTests::FFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}

	Fixture.SetOwnerRole(ROLE_AutonomousProxy);
	if (!TestFalse(TEXT("The owning client has no grant authority"), Fixture.AbilitySystem->HasGrantAuthority()))
	{
		return false;
	}
	AddExpectedError(TEXT("GrantAbilitySet: ignored"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A client grant request is rejected"), Fixture.AbilitySystem->GrantAbilitySet(Fixture.AbilitySet, nullptr));
	TestFalse(TEXT("The rejected set is not tracked"), Fixture.AbilitySystem->HasAbilitySet(Fixture.AbilitySet));
	TestFalse(TEXT("The rejected set grants no ability"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_Authority);
	if (!TestTrue(TEXT("The server grants the set"), Fixture.AbilitySystem->GrantAbilitySet(Fixture.AbilitySet, nullptr)))
	{
		return false;
	}
	TestTrue(TEXT("The server grant adds the ability"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_AutonomousProxy);
	AddExpectedError(TEXT("RemoveAbilitySet: ignored"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A client remove request is rejected"), Fixture.AbilitySystem->RemoveAbilitySet(Fixture.AbilitySet));
	TestTrue(TEXT("The granted set stays tracked"), Fixture.AbilitySystem->HasAbilitySet(Fixture.AbilitySet));
	TestTrue(TEXT("The granted ability stays"), Fixture.HasGrantedAbility());

	Fixture.SetOwnerRole(ROLE_Authority);
	TestTrue(TEXT("The server removes the set"), Fixture.AbilitySystem->RemoveAbilitySet(Fixture.AbilitySet));
	TestFalse(TEXT("The removed ability is gone"), Fixture.HasGrantedAbility());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAbilitySystemFirstAvatarCueTest,
	"SurvivalRpg.AbilitySystem.Cues.ActiveCuesReachTheFirstAvatar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgAbilitySystemFirstAvatarCueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgAbilitySetAuthorityTests;

	FFixture Fixture;
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!TestNotNull(TEXT("A test world exists"), Fixture.GetWorld()) || !TestNotNull(TEXT("The cue manager exists"), CueManager))
	{
		return false;
	}

	// A player state's ability system before its pawn: an owner without an avatar.
	AActor* Owner = Fixture.GetWorld()->SpawnActor<AActor>();
	URpgAbilitySystemComponent* AbilitySystem = NewObject<URpgAbilitySystemComponent>(Owner, NAME_None, RF_Transient);
	Owner->AddInstanceComponent(AbilitySystem);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(Owner, nullptr);

	TMap<FGameplayTag, int32> WhileActiveOnAvatar;
	const APawn* Avatar = nullptr;
	const FDelegateHandle Routed = CueManager->OnGameplayCueRouted().AddLambda(
		[&WhileActiveOnAvatar, &Avatar](AActor* Target, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
			const FGameplayCueParameters&, EGameplayCueExecutionOptions)
		{
			if (Event == EGameplayCueEvent::WhileActive && Target && Target == Avatar)
			{
				++WhileActiveOnAvatar.FindOrAdd(Tag);
			}
		});

	// A cue added outside effects and the looping cue of a timed effect both start while there is nowhere to show them.
	const FGameplayTag LooseCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.Swing"));
	const FGameplayTag EffectCue = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Combat.Telegraph"));
	AbilitySystem->AddGameplayCue(LooseCue);
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.0f));
	Effect->GameplayCues.Add(FGameplayEffectCue(EffectCue, 0.0f, 0.0f));
	const FActiveGameplayEffectHandle EffectHandle = AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.0f, AbilitySystem->MakeEffectContext());
	TestTrue(TEXT("The timed effect applies without an avatar"), EffectHandle.IsValid());
	TestTrue(TEXT("Both cues are active"), AbilitySystem->IsGameplayCueActive(LooseCue) && AbilitySystem->IsGameplayCueActive(EffectCue));
	// An owning client gets the effect through replication, and the engine defers its cue to the first avatar itself.
	const FActiveGameplayEffect* ActiveEffect = AbilitySystem->GetActiveGameplayEffect(EffectHandle);
	if (TestNotNull(TEXT("The effect is active"), ActiveEffect))
	{
		ActiveEffect->bPendingRepWhileActiveGC = true;
	}

	// The first pawn shows each active cue once, the deferred effect cue included.
	APawn* Pawn = Fixture.GetWorld()->SpawnActor<APawn>();
	Avatar = Pawn;
	AbilitySystem->InitAbilityActorInfo(Owner, Pawn);
	TestEqual(TEXT("The first avatar shows the loose cue once"), WhileActiveOnAvatar.FindRef(LooseCue), 1);
	TestEqual(TEXT("The first avatar shows the effect's cue once"), WhileActiveOnAvatar.FindRef(EffectCue), 1);

	// Binding the same pawn again replays nothing.
	AbilitySystem->InitAbilityActorInfo(Owner, Pawn);
	TestEqual(TEXT("A repeated binding does not replay the loose cue"), WhileActiveOnAvatar.FindRef(LooseCue), 1);
	TestEqual(TEXT("A repeated binding does not replay the effect's cue"), WhileActiveOnAvatar.FindRef(EffectCue), 1);

	// A death clears the avatar, as the pawn extension does; the respawned pawn shows each cue once.
	AbilitySystem->InitAbilityActorInfo(Owner, nullptr);
	APawn* RespawnedPawn = Fixture.GetWorld()->SpawnActor<APawn>();
	Avatar = RespawnedPawn;
	WhileActiveOnAvatar.Reset();
	AbilitySystem->InitAbilityActorInfo(Owner, RespawnedPawn);
	TestEqual(TEXT("The respawned pawn shows the loose cue once"), WhileActiveOnAvatar.FindRef(LooseCue), 1);
	TestEqual(TEXT("The respawned pawn shows the effect's cue once"), WhileActiveOnAvatar.FindRef(EffectCue), 1);

	CueManager->OnGameplayCueRouted().Remove(Routed);
	return true;
}

#endif
