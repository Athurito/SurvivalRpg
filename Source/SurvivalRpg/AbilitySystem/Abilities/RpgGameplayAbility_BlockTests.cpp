#if WITH_DEV_AUTOMATION_TESTS

#include "RpgGameplayAbility_Block.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgBlockLifecycleTests
{
	struct FDefenseValues
	{
		float Angle = 65.0f;
		float StaminaCost = 7.0f;
		float DamageReduction = 0.2f;
		float StaggerMultiplier = 0.8f;
		float PerfectStamina = 3.0f;
		float PerfectStagger = 11.0f;
	};

	void SetValues(URpgDefenseSet& Set, const FDefenseValues& Values)
	{
		Set.InitBlockAngleDegrees(Values.Angle);
		Set.InitBlockStaminaCost(Values.StaminaCost);
		Set.InitBlockDamageReduction(Values.DamageReduction);
		Set.InitBlockStaggerDamageMultiplier(Values.StaggerMultiplier);
		Set.InitPerfectBlockStaminaRestore(Values.PerfectStamina);
		Set.InitPerfectBlockStaggerDamage(Values.PerfectStagger);
	}

	void CheckValues(FAutomationTestBase& Test, const URpgDefenseSet& Set, const FDefenseValues& Values)
	{
		const FGameplayAttributeData* Attributes[] = { &Set.BlockAngleDegrees, &Set.BlockStaminaCost,
			&Set.BlockDamageReduction, &Set.BlockStaggerDamageMultiplier, &Set.PerfectBlockStaminaRestore,
			&Set.PerfectBlockStaggerDamage };
		const float Expected[] = { Values.Angle, Values.StaminaCost, Values.DamageReduction,
			Values.StaggerMultiplier, Values.PerfectStamina, Values.PerfectStagger };
		const TCHAR* Names[] = { TEXT("BlockAngleDegrees"), TEXT("BlockStaminaCost"), TEXT("BlockDamageReduction"),
			TEXT("BlockStaggerDamageMultiplier"), TEXT("PerfectBlockStaminaRestore"), TEXT("PerfectBlockStaggerDamage") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Attributes); ++Index)
		{
			Test.TestEqual(FString::Printf(TEXT("%s base retains its own value"), Names[Index]),
				Attributes[Index]->GetBaseValue(), Expected[Index]);
			Test.TestEqual(FString::Printf(TEXT("%s current retains its own value"), Names[Index]),
				Attributes[Index]->GetCurrentValue(), Expected[Index]);
		}
	}

	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FFixture()
		{
			if (ASC)
			{
				ASC->ClearAbilityInput();
				Grants.TakeFromAbilitySystem(ASC);
				ASC->OnAbilityEnded.Remove(EndHandle);
			}
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			BlockAbilityClass = LoadClass<URpgGameplayAbility_Block>(nullptr,
				TEXT("/GF_Combat_Core/GAS/Abilities/GA_Combat_Block.GA_Combat_Block_C"));
			if (!Test.TestNotNull(TEXT("The concrete block Blueprint ability loads"), BlockAbilityClass.Get())) return false;
			if (!Test.TestNotNull(TEXT("Standalone block lifecycle world exists"), World)) return false;
			Pawn = World->SpawnActor<APawn>();
			if (!Test.TestNotNull(TEXT("Authoritative block avatar exists"), Pawn)) return false;
			ASC = NewObject<URpgAbilitySystemComponent>(Pawn, NAME_None, RF_Transient);
			Pawn->AddInstanceComponent(ASC);
			ASC->RegisterComponent();
			Defense.Reset(NewObject<URpgDefenseSet>(Pawn, NAME_None, RF_Transient));
			SetValues(*Defense, Baseline);
			ASC->AddAttributeSetSubobject(Defense.Get());
			ASC->InitAbilityActorInfo(Pawn, Pawn);
			Weapon.Reset(NewObject<URpgWeaponInstance>(Pawn, NAME_None, RF_Transient));
			// Exercise the reusable native lifecycle through its concrete Blueprint grant. Animation and
			// network presentation remain covered by the existing rendered block PIE test.
			Weapon->ConfigureMeleeBlock(true, true, 150.0f, 0.25f, 23.0f, 0.9f, 0.4f, 17.0f, 41.0f, nullptr);
			EndHandle = ASC->OnAbilityEnded.AddLambda([this](const FAbilityEndedData& Data)
			{
				if (Data.AbilityThatEnded && Data.AbilityThatEnded->IsA<URpgGameplayAbility_Block>())
				{
					++EndCount;
					bLastEndCancelled = Data.bWasCancelled;
				}
			});
			return Test.TestTrue(TEXT("The fixture has authority to grant and remove abilities"), ASC->HasGrantAuthority());
		}

		FGameplayAbilitySpecHandle Grant()
		{
			FGameplayAbilitySpec Spec(BlockAbilityClass, 1);
			Spec.SourceObject = Weapon.Get();
			Spec.GetDynamicSpecSourceTags().AddTag(RpgGameplayTags::InputTag_Weapon_Block);
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			Grants.AddAbilitySpecHandle(Handle);
			return Handle;
		}

		bool Press(FAutomationTestBase& Test, FGameplayAbilitySpecHandle Handle)
		{
			ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
			ASC->ProcessAbilityInput(0.0f, false);
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (!Test.TestTrue(TEXT("Held input activates the actual equipment block ability"), Spec && Spec->IsActive())) return false;
			Test.TestEqual(TEXT("Authority applies the configured blocking angle"), Defense->GetBlockAngleDegrees(), 150.0f);
			CheckTags(Test, true);
			return true;
		}

		void Release()
		{
			ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Block);
			ASC->ProcessAbilityInput(0.0f, false);
		}

		void CheckTags(FAutomationTestBase& Test, bool bBlocking) const
		{
			Test.TestEqual(TEXT("Blocking tag follows the active block"),
				ASC->GetTagCount(RpgGameplayTags::State_Blocking), bBlocking ? 1 : 0);
			Test.TestEqual(TEXT("Perfect-block tag follows this immediate activation and cleanup"),
				ASC->GetTagCount(RpgGameplayTags::State_PerfectBlockWindow), bBlocking ? 1 : 0);
		}

		bool IsActive(FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			return Spec && Spec->IsActive();
		}

		UWorld* World = nullptr;
		APawn* Pawn = nullptr;
		URpgAbilitySystemComponent* ASC = nullptr;
		TStrongObjectPtr<URpgDefenseSet> Defense;
		TStrongObjectPtr<URpgWeaponInstance> Weapon;
		FDefenseValues Baseline;
		FRpgAbilitySet_GrantedHandles Grants;
		int32 EndCount = 0;
		bool bLastEndCancelled = false;

	private:
		TStrongObjectPtr<UGameInstance> GameInstance;
		TSubclassOf<URpgGameplayAbility_Block> BlockAbilityClass;
		FDelegateHandle EndHandle;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockInputReleaseTest,
	"SurvivalRpg.Combat.Block.Lifecycle.InputReleaseRestoresDefenseValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockInputReleaseTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.Release();
	TestFalse(TEXT("Input release ends the granted block"), Fixture.IsActive(Handle));
	TestEqual(TEXT("Ordinary release ends exactly once"), Fixture.EndCount, 1);
	TestFalse(TEXT("Ordinary release is not cancellation"), Fixture.bLastEndCancelled);
	Fixture.CheckTags(*this, false);
	CheckValues(*this, *Fixture.Defense, Fixture.Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockRemovedDefenseSetTest,
	"SurvivalRpg.Combat.Block.Lifecycle.RemovedDefenseSetBeforeGrantRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockRemovedDefenseSetTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.ASC->RemoveSpawnedAttribute(Fixture.Defense.Get());
	TestNull(TEXT("The exact teardown precondition removes DefenseSet from GAS"), Fixture.ASC->GetSet<URpgDefenseSet>());
	// This is the real equipment-grant removal route from the recorded PIE teardown ensure.
	// No expected error masks an absent-attribute access: any ensure fails this regression.
	Fixture.Grants.TakeFromAbilitySystem(Fixture.ASC);
	TestNull(TEXT("Equipment grant removal removes the active block spec"), Fixture.ASC->FindAbilitySpecFromHandle(Handle));
	TestEqual(TEXT("Removing the active grant ends block once"), Fixture.EndCount, 1);
	Fixture.CheckTags(*this, false);
	TestNull(TEXT("Cleanup does not recreate a removed attribute set"), Fixture.ASC->GetSet<URpgDefenseSet>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockReplacementDefenseSetTest,
	"SurvivalRpg.Combat.Block.Lifecycle.ReplacementDefenseSetIsNotOverwritten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockReplacementDefenseSetTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.ASC->RemoveSpawnedAttribute(Fixture.Defense.Get());
	TStrongObjectPtr<URpgDefenseSet> Replacement(NewObject<URpgDefenseSet>(Fixture.Pawn, NAME_None, RF_Transient));
	const FDefenseValues ReplacementValues{ 95.0f, 13.0f, 0.6f, 0.7f, 19.0f, 29.0f };
	SetValues(*Replacement, ReplacementValues);
	Fixture.ASC->AddAttributeSetSubobject(Replacement.Get());
	TestTrue(TEXT("GAS now resolves a different instance of the same DefenseSet class"),
		Fixture.ASC->GetSet<URpgDefenseSet>() == Replacement.Get() && Replacement.Get() != Fixture.Defense.Get());
	Fixture.Grants.TakeFromAbilitySystem(Fixture.ASC);
	Fixture.CheckTags(*this, false);
	TestEqual(TEXT("Replacing attributes does not prevent ability cleanup"), Fixture.EndCount, 1);
	CheckValues(*this, *Replacement, ReplacementValues);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockRepeatedCleanupTest,
	"SurvivalRpg.Combat.Block.Lifecycle.RepeatedOldCleanupPreservesNewBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockRepeatedCleanupTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle OldHandle = Fixture.Grant();
	if (!Fixture.Press(*this, OldHandle)) return false;
	TStrongObjectPtr<UGameplayAbility> OldAbility(Fixture.ASC->FindAbilitySpecFromHandle(OldHandle)->GetPrimaryInstance());
	Fixture.Release();
	FRpgAbilitySet_GrantedHandles RetiredGrant;
	RetiredGrant.AddAbilitySpecHandle(OldHandle);
	RetiredGrant.TakeFromAbilitySystem(Fixture.ASC);
	const FGameplayAbilitySpecHandle NewHandle = Fixture.Grant();
	if (!Fixture.Press(*this, NewHandle)) return false;
	RetiredGrant.TakeFromAbilitySystem(Fixture.ASC);
	Fixture.ASC->ClearAbility(OldHandle);
	// A delayed native cancellation can still address the retired instance. It must not
	// clear tags owned by a subsequent block activation on this same ASC.
	OldAbility->CancelAbility(OldHandle, OldAbility->GetCurrentActorInfo(), OldAbility->GetCurrentActivationInfo(), false);
	TestTrue(TEXT("Repeated old removal leaves the new spec active"), Fixture.IsActive(NewHandle));
	TestEqual(TEXT("Repeated cleanup does not emit a second old end"), Fixture.EndCount, 1);
	Fixture.CheckTags(*this, true);
	TestEqual(TEXT("Repeated cleanup does not restore stale defense values"), Fixture.Defense->GetBlockAngleDegrees(), 150.0f);
	Fixture.Release();
	TestEqual(TEXT("The new activation has its own single normal end"), Fixture.EndCount, 2);
	Fixture.CheckTags(*this, false);
	CheckValues(*this, *Fixture.Defense, Fixture.Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockReentrantCleanupTest,
	"SurvivalRpg.Combat.Block.Lifecycle.TagCallbackCancellationEndsOnlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockReentrantCleanupTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	int32 CleanupCallbacks = 0;
	const FDelegateHandle Callback = Fixture.ASC->RegisterGameplayTagEvent(RpgGameplayTags::State_Blocking).AddLambda(
		[&Fixture, Handle, &CleanupCallbacks](FGameplayTag, int32 Count)
		{
			if (Count == 0 && ++CleanupCallbacks == 1)
			{
				Fixture.ASC->CancelAbilityHandle(Handle);
			}
		});
	Fixture.Release();
	Fixture.ASC->RegisterGameplayTagEvent(RpgGameplayTags::State_Blocking).Remove(Callback);
	TestEqual(TEXT("A real blocking-tag removal invoked the nested cancellation"), CleanupCallbacks, 1);
	TestEqual(TEXT("Nested cancellation cannot emit a duplicate ability end"), Fixture.EndCount, 1);
	TestFalse(TEXT("Nested cancellation does not replace the original normal-release outcome"), Fixture.bLastEndCancelled);
	TestFalse(TEXT("The original release completes"), Fixture.IsActive(Handle));
	Fixture.CheckTags(*this, false);
	CheckValues(*this, *Fixture.Defense, Fixture.Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockModifierRestorationTest,
	"SurvivalRpg.Combat.Block.Lifecycle.InputReleasePreservesActiveAttributeModifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockModifierRestorationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	TStrongObjectPtr<UGameplayEffect> Effect(NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient));
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URpgDefenseSet::GetBlockAngleDegreesAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(12.0f);
	const FActiveGameplayEffectHandle EffectHandle = Fixture.ASC->ApplyGameplayEffectToSelf(
		Effect.Get(), 1.0f, Fixture.ASC->MakeEffectContext());
	if (!TestTrue(TEXT("The independent defense modifier is active"), EffectHandle.IsValid())) return false;
	TestEqual(TEXT("Before block the modifier affects current, not base"), Fixture.Defense->GetBlockAngleDegrees(), 77.0f);
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	// This case retains the live modifier, so its current angle intentionally differs from
	// the unmodified fixture's Press assertion while the same production input path activates.
	Fixture.ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->ProcessAbilityInput(0.0f, false);
	if (!TestTrue(TEXT("The modifier does not prevent block activation"), Fixture.IsActive(Handle))) return false;
	TestEqual(TEXT("Blocking changes base while the modifier remains additive"), Fixture.Defense->BlockAngleDegrees.GetBaseValue(), 150.0f);
	TestEqual(TEXT("Blocking retains the live modifier"), Fixture.Defense->GetBlockAngleDegrees(), 162.0f);
	Fixture.Release();
	TestEqual(TEXT("Release restores the original base without baking in the modifier"),
		Fixture.Defense->BlockAngleDegrees.GetBaseValue(), Fixture.Baseline.Angle);
	TestEqual(TEXT("Release leaves exactly one active modifier contribution"), Fixture.Defense->GetBlockAngleDegrees(), 77.0f);
	TestNotNull(TEXT("Block cleanup leaves the unrelated effect active"), Fixture.ASC->GetActiveGameplayEffect(EffectHandle));
	Fixture.CheckTags(*this, false);
	Fixture.ASC->RemoveActiveGameplayEffect(EffectHandle);
	CheckValues(*this, *Fixture.Defense, Fixture.Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockMissingDefenseActivationTest,
	"SurvivalRpg.Combat.Block.Lifecycle.MissingDefenseSetRejectsActivationUntilRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockMissingDefenseActivationTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	Fixture.ASC->RemoveSpawnedAttribute(Fixture.Defense.Get());
	Fixture.ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->ProcessAbilityInput(0.0f, false);
	TestFalse(TEXT("A missing defense foundation rejects block activation"), Fixture.IsActive(Handle));
	TestNotNull(TEXT("A rejected activation retains its equipment grant"), Fixture.ASC->FindAbilitySpecFromHandle(Handle));
	Fixture.CheckTags(*this, false);
	Fixture.Release();
	Fixture.ASC->AddAttributeSetSubobject(Fixture.Defense.Get());
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.Release();
	TestFalse(TEXT("The same grant releases normally after the foundation is restored"), Fixture.IsActive(Handle));
	Fixture.CheckTags(*this, false);
	CheckValues(*this, *Fixture.Defense, Fixture.Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockDefenseReplacementDuringRestoreTest,
	"SurvivalRpg.Combat.Block.Lifecycle.AttributeCallbackReplacementStopsRemainingRestores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockDefenseReplacementDuringRestoreTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	TStrongObjectPtr<URpgDefenseSet> Replacement(NewObject<URpgDefenseSet>(Fixture.Pawn, NAME_None, RF_Transient));
	const FDefenseValues ReplacementValues{ 95.0f, 13.0f, 0.6f, 0.7f, 19.0f, 29.0f };
	SetValues(*Replacement, ReplacementValues);
	int32 ReplacementCallbacks = 0;
	const FGameplayAttribute AngleAttribute = URpgDefenseSet::GetBlockAngleDegreesAttribute();
	const FDelegateHandle Callback = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(AngleAttribute).AddLambda(
		[&Fixture, &Replacement, &ReplacementCallbacks](const FOnAttributeChangeData& Data)
		{
			if (ReplacementCallbacks == 0 && FMath::IsNearlyEqual(Data.NewValue, Fixture.Baseline.Angle))
			{
				++ReplacementCallbacks;
				// The first real GAS restoration synchronously retires the set. Keeping both
				// objects alive distinguishes registration/identity from UObject validity.
				Fixture.ASC->RemoveSpawnedAttribute(Fixture.Defense.Get());
				Fixture.ASC->AddAttributeSetSubobject(Replacement.Get());
			}
		});
	Fixture.Release();
	Fixture.ASC->GetGameplayAttributeValueChangeDelegate(AngleAttribute).Remove(Callback);
	TestEqual(TEXT("The first restoration synchronously replaces the registered DefenseSet"), ReplacementCallbacks, 1);
	TestTrue(TEXT("Cleanup retains the replacement instance registered by the callback"),
		Fixture.ASC->GetSet<URpgDefenseSet>() == Replacement.Get());
	TestEqual(TEXT("The original angle restoration completed before replacement"),
		Fixture.Defense->GetBlockAngleDegrees(), Fixture.Baseline.Angle);
	TestEqual(TEXT("Subsequent restoration does not write through the retired set"),
		Fixture.Defense->GetBlockStaminaCost(), Fixture.Weapon->GetBlockDefinition().StaminaCost);
	CheckValues(*this, *Replacement, ReplacementValues);
	TestFalse(TEXT("Attribute replacement does not prevent normal release"), Fixture.IsActive(Handle));
	TestEqual(TEXT("Callback replacement ends the ability once"), Fixture.EndCount, 1);
	TestFalse(TEXT("Callback replacement preserves the normal release outcome"), Fixture.bLastEndCancelled);
	Fixture.CheckTags(*this, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockConfirmedReactionIdentityTest,
	"SurvivalRpg.Combat.Block.Lifecycle.ConfirmedReactionRejectsRetiredActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockConfirmedReactionIdentityTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Local controller exists for receipt validation"), Controller)) return false;
	// SpawnActor bypasses the normal SetPlayer/local-player login path; possession alone leaves this
	// controller marked remote, and GAS correctly rejects its LocalPredicted block activation.
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Fixture.Pawn);
	Fixture.ASC->InitAbilityActorInfo(Fixture.Pawn, Fixture.Pawn);
	if (!TestTrue(TEXT("The receipt fixture is an explicitly local controller and pawn"),
		Controller->IsLocalController() && Fixture.Pawn->IsLocallyControlled())) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	UGameplayAbility* Ability = Fixture.ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
	const FPredictionKey FirstKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	if (!TestTrue(TEXT("The actual GAS activation has a valid identity"), FirstKey.IsValidKey())) return false;

	int32 ReceivedBlock = 0;
	int32 ReceivedPerfectBlock = 0;
	int32 ReceivedUnsupported = 0;
	const FDelegateHandle BlockCallback = Fixture.ASC->GenericGameplayEventCallbacks.FindOrAdd(RpgGameplayTags::GameplayEvent_Block).AddLambda(
		[&ReceivedBlock](const FGameplayEventData*) { ++ReceivedBlock; });
	const FDelegateHandle PerfectCallback = Fixture.ASC->GenericGameplayEventCallbacks.FindOrAdd(RpgGameplayTags::GameplayEvent_PerfectBlock).AddLambda(
		[&ReceivedPerfectBlock](const FGameplayEventData*) { ++ReceivedPerfectBlock; });
	const FDelegateHandle UnsupportedCallback = Fixture.ASC->GenericGameplayEventCallbacks.FindOrAdd(RpgGameplayTags::GameplayEvent_Stagger).AddLambda(
		[&ReceivedUnsupported](const FGameplayEventData*) { ++ReceivedUnsupported; });
	ON_SCOPE_EXIT
	{
		Fixture.ASC->GenericGameplayEventCallbacks.FindChecked(RpgGameplayTags::GameplayEvent_Block).Remove(BlockCallback);
		Fixture.ASC->GenericGameplayEventCallbacks.FindChecked(RpgGameplayTags::GameplayEvent_PerfectBlock).Remove(PerfectCallback);
		Fixture.ASC->GenericGameplayEventCallbacks.FindChecked(RpgGameplayTags::GameplayEvent_Stagger).Remove(UnsupportedCallback);
	};
	UFunction* Receipt = Fixture.ASC->FindFunction(TEXT("ClientConfirmBlockReaction"));
	struct FReceiptArguments
	{
		AActor* Avatar;
		FGameplayAbilitySpecHandle AbilityHandle;
		FPredictionKey ActivationPredictionKey;
		FGameplayTag ReactionTag;
	};
	if (!TestNotNull(TEXT("The production receipt RPC is reflected"), Receipt)
		|| !TestEqual(TEXT("Receipt argument layout matches its reflected signature"), int32(Receipt->ParmsSize), int32(sizeof(FReceiptArguments)))) return false;
	// This native regression exercises the real receive handler and real active ability/lease while
	// selecting the local receiving role. Transport and rendered reactions are separate multiplayer tests.
	auto Deliver = [&](AActor* Avatar, FGameplayAbilitySpecHandle Spec, FPredictionKey Key, FGameplayTag Tag, ENetRole Role = ROLE_AutonomousProxy)
	{
		const ENetRole OriginalRole = Fixture.Pawn->GetLocalRole();
		Fixture.Pawn->SetRole(Role);
		FReceiptArguments Arguments{ Avatar, Spec, Key, Tag };
		Fixture.ASC->ProcessEvent(Receipt, &Arguments);
		Fixture.Pawn->SetRole(OriginalRole);
	};
	Deliver(Fixture.Pawn, Handle, FirstKey, RpgGameplayTags::GameplayEvent_Block);
	Deliver(Fixture.Pawn, Handle, FirstKey, RpgGameplayTags::GameplayEvent_PerfectBlock);
	TestEqual(TEXT("Current block confirmation dispatches the local GAS event"), ReceivedBlock, 1);
	TestEqual(TEXT("Current perfect-block confirmation dispatches the local GAS event"), ReceivedPerfectBlock, 1);
	Fixture.Release();
	Deliver(Fixture.Pawn, Handle, FirstKey, RpgGameplayTags::GameplayEvent_Block);
	TestEqual(TEXT("Release drops an in-flight confirmation"), ReceivedBlock, 1);
	if (!Fixture.Press(*this, Handle)) return false;
	UGameplayAbility* ReactivatedAbility = Fixture.ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
	const FPredictionKey SecondKey = ReactivatedAbility->GetCurrentActivationInfo().GetActivationPredictionKey();
	TestTrue(TEXT("The same ability object/spec starts a distinct activation"), Ability == ReactivatedAbility && SecondKey.IsValidKey() && SecondKey != FirstKey);
	Deliver(Fixture.Pawn, Handle, FirstKey, RpgGameplayTags::GameplayEvent_Block);
	TestEqual(TEXT("A retired key cannot react on the new activation of the same object"), ReceivedBlock, 1);
	Deliver(Fixture.Pawn, FGameplayAbilitySpecHandle{}, SecondKey, RpgGameplayTags::GameplayEvent_Block);
	Deliver(Fixture.World->SpawnActor<APawn>(), Handle, SecondKey, RpgGameplayTags::GameplayEvent_Block);
	Deliver(Fixture.Pawn, Handle, FPredictionKey{}, RpgGameplayTags::GameplayEvent_Block);
	Deliver(Fixture.Pawn, Handle, SecondKey, RpgGameplayTags::GameplayEvent_Block, ROLE_Authority);
	Deliver(Fixture.Pawn, Handle, SecondKey, RpgGameplayTags::GameplayEvent_Block, ROLE_SimulatedProxy);
	Deliver(Fixture.Pawn, Handle, SecondKey, RpgGameplayTags::GameplayEvent_Stagger);
	TestEqual(TEXT("Wrong spec, avatar, invalid key and non-owner roles cannot deliver"), ReceivedBlock, 1);
	TestEqual(TEXT("The bridge rejects events outside its two reaction tags"), ReceivedUnsupported, 0);
	Deliver(Fixture.Pawn, Handle, SecondKey, RpgGameplayTags::GameplayEvent_PerfectBlock);
	TestEqual(TEXT("The replacement activation accepts its own confirmed reaction"), ReceivedPerfectBlock, 2);
	Fixture.Release();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgBlockCancelledHoldRequiresReleaseTest,
	"SurvivalRpg.Combat.Block.Lifecycle.CancelledHoldRequiresPhysicalRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBlockCancelledHoldRequiresReleaseTest::RunTest(const FString& Parameters)
{
	using namespace RpgBlockLifecycleTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("The cancellation fixture has a local input controller"), Controller)) return false;
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Fixture.Pawn);
	Fixture.ASC->InitAbilityActorInfo(Fixture.Pawn, Fixture.Pawn);
	if (!TestTrue(TEXT("Cancellation exercises the locally controlled input policy"), Fixture.Pawn->IsLocallyControlled())) return false;
	const FGameplayAbilitySpecHandle Handle = Fixture.Grant();
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.ASC->CancelAbilityHandle(Handle);
	TestFalse(TEXT("Real GAS cancellation ends the held block"), Fixture.IsActive(Handle));
	TestEqual(TEXT("Cancellation ends exactly one activation"), Fixture.EndCount, 1);
	TestTrue(TEXT("The end retains its cancellation outcome"), Fixture.bLastEndCancelled);
	TestTrue(TEXT("Cancellation does not fabricate a physical release"), Fixture.ASC->FindAbilitySpecFromHandle(Handle)->InputPressed);
	Fixture.CheckTags(*this, false);

	// Enhanced Input binds ability presses to Triggered, so the same held key sends another
	// press every frame. No interruption tag is needed to keep this cancelled hold consumed.
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		if (Frame == 2) Fixture.ASC->ClearAbilityInput();
		Fixture.ASC->AbilityInputTagPressed(RpgGameplayTags::InputTag_Weapon_Block);
		Fixture.ASC->ProcessAbilityInput(1.0f / 60.0f, false);
		TestFalse(TEXT("Repeated Triggered input cannot resurrect the cancelled activation"), Fixture.IsActive(Handle));
	}
	TestEqual(TEXT("Repeated held input does not create additional ends"), Fixture.EndCount, 1);
	Fixture.Release();
	TestFalse(TEXT("The actual release clears the spec's physical input flag"), Fixture.ASC->FindAbilitySpecFromHandle(Handle)->InputPressed);
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.Release();
	TestEqual(TEXT("A fresh press and normal release form exactly one new activation"), Fixture.EndCount, 2);
	TestFalse(TEXT("Normal release remains a successful end"), Fixture.bLastEndCancelled);

	// A cancellation arriving after the actual release was queued must not latch a second release.
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.ASC->AbilityInputTagReleased(RpgGameplayTags::InputTag_Weapon_Block);
	Fixture.ASC->CancelAbilityHandle(Handle);
	Fixture.ASC->ProcessAbilityInput(0.0f, false);
	if (!Fixture.Press(*this, Handle)) return false;
	Fixture.Release();
	TestEqual(TEXT("Release/cancellation in the same frame does not prevent the next physical press"), Fixture.EndCount, 4);
	Fixture.CheckTags(*this, false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
