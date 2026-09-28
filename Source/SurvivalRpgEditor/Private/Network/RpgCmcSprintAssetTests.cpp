#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "InputAction.h"
#include "InputTriggers.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "UObject/UnrealType.h"

namespace RpgCmcSprintAssetTests
{
	const FString Root = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Sprint/");
	constexpr const TCHAR* BasePawnData = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC.DA_PawnData_GaspCMC");
	constexpr const TCHAR* MantlePawnData = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/DA_PawnData_GaspMantle.DA_PawnData_GaspMantle");

	FString ObjectPath(const TCHAR* Name, bool bGeneratedClass = false)
	{
		return Root + Name + TEXT(".") + Name + (bGeneratedClass ? TEXT("_C") : TEXT(""));
	}

	const URpgGameplayAbility* LoadAbility(FAutomationTestBase& Test, const TCHAR* Name,
		ERpgAbilityActivationPolicy Policy, EGameplayAbilityNetExecutionPolicy::Type NetworkPolicy)
	{
		UClass* Class = LoadClass<URpgGameplayAbility>(nullptr, *ObjectPath(Name, true));
		if (!Test.TestNotNull(FString::Printf(TEXT("Designer ability loads: %s"), Name), Class)) return nullptr;
		const UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
		Test.TestNotNull(TEXT("Concrete ability is Blueprint authored"), Blueprint);
		Test.TestTrue(TEXT("Concrete ability directly uses the reusable RPG GAS base"), Class->GetSuperClass() == URpgGameplayAbility::StaticClass());
		Test.TestFalse(TEXT("Concrete ability is instantiable and current"), Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists));
		const URpgGameplayAbility* Ability = Class->GetDefaultObject<URpgGameplayAbility>();
		Test.TestTrue(TEXT("Ability uses its intended input or spawn activation policy"), Ability->GetActivationPolicy() == Policy);
		Test.TestTrue(TEXT("Ability uses its intended network execution policy"), Ability->GetNetExecutionPolicy() == NetworkPolicy);
		Test.TestTrue(TEXT("Sprint and recovery do not take an exclusive combat activation group"), Ability->GetActivationGroup() == ERpgAbilityActivationGroup::Independent);
		return Ability;
	}

	template<typename T>
	const TArray<T>* ReadGrants(const URpgAbilitySet* Set, FName PropertyName)
	{
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(URpgAbilitySet::StaticClass(), PropertyName);
		const FStructProperty* Element = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
		return Set && Element && Element->Struct == T::StaticStruct()
			? Property->ContainerPtrToValuePtr<TArray<T>>(Set) : nullptr;
	}

	bool SupportsHeldInput(const UInputAction* Action)
	{
		if (!Action || Action->ValueType != EInputActionValueType::Boolean) return false;
		bool bHasExplicit = false;
		bool bHasContinuousExplicit = false;
		for (const UInputTrigger* Trigger : Action->Triggers)
		{
			if (!Trigger || !FMath::IsFinite(Trigger->ActuationThreshold)
				|| Trigger->ActuationThreshold < 0.f || Trigger->ActuationThreshold > 1.f) return false;
			const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger);
			const bool bContinuous = Trigger->IsA<UInputTriggerDown>()
				|| (Hold && !Hold->bIsOneShot && FMath::IsFinite(Hold->HoldTimeThreshold) && Hold->HoldTimeThreshold >= 0.f);
			if (Trigger->GetTriggerType() == ETriggerType::Explicit)
			{
				bHasExplicit = true;
				bHasContinuousExplicit |= bContinuous;
			}
			else if (Trigger->GetTriggerType() != ETriggerType::Implicit || !bContinuous)
			{
				// An unknown implicit/blocking trigger cannot establish this simple held-button contract.
				return false;
			}
		}
		// No triggers uses Enhanced Input's default sustained actuation. Pressed/Released/Tap alone
		// cannot sustain GAS Triggered input, even though some advertise the same event flags as Down.
		return !bHasExplicit || bHasContinuousExplicit;
	}

	void CheckLocalDependencies(FAutomationTestBase& Test)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.WaitForCompletion();
		TArray<FName> Pending;
		for (const TCHAR* Name : { TEXT("GA_RpgGasp_CMCSprint"), TEXT("GA_RpgGasp_StaminaRegen"),
			TEXT("GE_RpgGasp_StaminaDelta"), TEXT("GE_RpgGasp_StaminaDefaults"),
			TEXT("AS_RpgGasp_CMCSprint"), TEXT("DA_InputConfig_GaspCMC") })
		{
			Pending.Add(FName(*(Root + Name)));
		}
		TSet<FName> Visited;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			const FName Package = Pending[Index];
			if (Visited.Contains(Package)) continue;
			Visited.Add(Package);
			const FString Name = Package.ToString();
			if (Name.StartsWith(TEXT("/Script/"))) continue;
			FString Filename;
			if (!Test.TestTrue(FString::Printf(TEXT("Dependency resolves through a registered mount: %s"), *Name),
				FPackageName::TryConvertLongPackageNameToFilename(Name, Filename))) continue;
			Filename = FPaths::ConvertRelativePathToFull(Filename);
			if (FPaths::IsUnderDirectory(Filename, FPaths::ConvertRelativePathToFull(FPaths::EngineDir()))) continue;
			if (!Test.TestTrue(FString::Printf(TEXT("Non-engine dependency belongs to this project: %s"), *Name),
				FPaths::IsUnderDirectory(Filename, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())))) continue;
			if (!Test.TestTrue(FString::Printf(TEXT("Project dependency exists on disk: %s"), *Name), FPackageName::DoesPackageExist(Name))) continue;
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Package, Assets, true);
			if (!Test.TestTrue(FString::Printf(TEXT("Saved registry covers the project dependency: %s"), *Name), !Assets.IsEmpty())) continue;
			TArray<FName> Dependencies;
			Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			Pending.Append(Dependencies);
		}
		Test.AddInfo(FString::Printf(TEXT("Inspected %d packages from the six sprint roots; engine/script boundaries excluded from expansion."), Visited.Num()));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCmcSprintAssetDefinitionsTest,
	"SurvivalRpg.GASP.CMC.Sprint.AssetDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCmcSprintAssetDefinitionsTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintAssetTests;
	const URpgGameplayAbility* Sprint = LoadAbility(*this, TEXT("GA_RpgGasp_CMCSprint"), ERpgAbilityActivationPolicy::WhileInputActive, EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	const URpgGameplayAbility* Regen = LoadAbility(*this, TEXT("GA_RpgGasp_StaminaRegen"), ERpgAbilityActivationPolicy::OnSpawn, EGameplayAbilityNetExecutionPolicy::ServerOnly);
	UClass* DeltaClass = LoadClass<UGameplayEffect>(nullptr, *ObjectPath(TEXT("GE_RpgGasp_StaminaDelta"), true));
	UClass* DefaultsClass = LoadClass<UGameplayEffect>(nullptr, *ObjectPath(TEXT("GE_RpgGasp_StaminaDefaults"), true));
	const URpgAbilitySet* Set = LoadObject<URpgAbilitySet>(nullptr, *ObjectPath(TEXT("AS_RpgGasp_CMCSprint")));
	if (!Sprint || !Regen || !TestNotNull(TEXT("Designer stamina delta effect loads"), DeltaClass)
		|| !TestNotNull(TEXT("Designer stamina defaults effect loads"), DefaultsClass)
		|| !TestNotNull(TEXT("CMC sprint ability set loads"), Set)) return false;
	const UGameplayEffect* Delta = DeltaClass->GetDefaultObject<UGameplayEffect>();
	const UGameplayEffect* Defaults = DefaultsClass->GetDefaultObject<UGameplayEffect>();
	TestTrue(TEXT("Delta effect is an Instant resource change, not a predicted periodic effect"),
		Delta->DurationPolicy == EGameplayEffectDurationType::Instant && Delta->Period.GetValueAtLevel(1.f) == 0.f);
	TestTrue(TEXT("Resource delta uses the task's one-modifier contract"), Delta->Modifiers.Num() == 1 && Delta->Executions.IsEmpty());
	if (Delta->Modifiers.Num() == 1)
	{
		const FGameplayModifierInfo& Modifier = Delta->Modifiers[0];
		TestTrue(TEXT("Delta modifies the canonical Stamina attribute additively"),
			Modifier.Attribute == URpgStaminaSet::GetStaminaAttribute() && Modifier.ModifierOp == EGameplayModOp::Additive);
		TestTrue(TEXT("Delta magnitude is explicitly supplied through the shared resource tag"),
			Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
			&& Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag == RpgGameplayTags::Data_StaminaDelta);
	}
	TestTrue(TEXT("Recovery tuning is a persistent task-owned effect without periodic execution or shared stacks"),
		Defaults->DurationPolicy == EGameplayEffectDurationType::Infinite && Defaults->Period.GetValueAtLevel(1.f) == 0.f
		&& Defaults->GetStackingType() == EGameplayEffectStackingType::None && Defaults->Executions.IsEmpty()
		&& Defaults->Modifiers.Num() == 1);
	TestTrue(TEXT("Defaults grant a finite positive recovery rate without freezing its tuning"),
		Defaults->Modifiers.ContainsByPredicate([](const FGameplayModifierInfo& Modifier)
		{
			float Magnitude = 0.f;
			return Modifier.Attribute == URpgStaminaSet::GetStaminaRegenAttribute()
				&& Modifier.ModifierOp == EGameplayModOp::Additive
				&& Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.f, Magnitude)
				&& FMath::IsFinite(Magnitude) && Magnitude > 0.f;
		}));
	const TArray<FRpgAbilitySet_GameplayAbility>* Abilities = ReadGrants<FRpgAbilitySet_GameplayAbility>(Set, TEXT("GrantedGameplayAbilities"));
	const TArray<FRpgAbilitySet_GameplayEffect>* Effects = ReadGrants<FRpgAbilitySet_GameplayEffect>(Set, TEXT("GrantedGameplayEffects"));
	if (!TestNotNull(TEXT("Ability-set grants use the expected reusable schema"), Abilities)
		|| !TestNotNull(TEXT("Ability-set effects use the expected reusable schema"), Effects)) return false;
	TestEqual(TEXT("The sprint class is not duplicated under another input tag"), Abilities->FilterByPredicate([Sprint](const FRpgAbilitySet_GameplayAbility& Entry)
		{ return Entry.Ability.Get() == Sprint->GetClass(); }).Num(), 1);
	TestEqual(TEXT("The recovery class is not duplicated under another input tag"), Abilities->FilterByPredicate([Regen](const FRpgAbilitySet_GameplayAbility& Entry)
		{ return Entry.Ability.Get() == Regen->GetClass(); }).Num(), 1);
	TestEqual(TEXT("The sprint ability has one input-routed grant"), Abilities->FilterByPredicate([Sprint](const FRpgAbilitySet_GameplayAbility& Entry)
		{ return Entry.Ability.Get() == Sprint->GetClass() && Entry.InputTag == RpgGameplayTags::InputTag_Ability_Sprint; }).Num(), 1);
	TestEqual(TEXT("The server recovery ability has one input-independent grant"), Abilities->FilterByPredicate([Regen](const FRpgAbilitySet_GameplayAbility& Entry)
		{ return Entry.Ability.Get() == Regen->GetClass() && !Entry.InputTag.IsValid(); }).Num(), 1);
	TestEqual(TEXT("Recovery tuning waits for the task's registered attribute set instead of an early ability-set grant"), Effects->FilterByPredicate([DefaultsClass](const FRpgAbilitySet_GameplayEffect& Entry)
		{ return Entry.GameplayEffect.Get() == DefaultsClass; }).Num(), 0);
	// The real PIE rate/recovery assertions verify that the authored ability supplies this tuning
	// to its task. Do not turn the Blueprint's graph layout or literal pin storage into a contract.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCmcSprintInputCompositionTest,
	"SurvivalRpg.GASP.CMC.Sprint.InputCompositionAndLocalDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCmcSprintInputCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgCmcSprintAssetTests;
	const URpgAbilitySet* Set = LoadObject<URpgAbilitySet>(nullptr, *ObjectPath(TEXT("AS_RpgGasp_CMCSprint")));
	const URpgInputConfig* Input = LoadObject<URpgInputConfig>(nullptr, *ObjectPath(TEXT("DA_InputConfig_GaspCMC")));
	if (!TestNotNull(TEXT("Sprint ability set exists"), Set) || !TestNotNull(TEXT("CMC input config exists"), Input)) return false;
	FDataValidationContext InputValidation;
	TestTrue(TEXT("Input config validates semantic mappings without ambiguity"), Input->IsDataValid(InputValidation) == EDataValidationResult::Valid);
	const UInputAction* Action = Input->FindAbilityInputActionForTag(RpgGameplayTags::InputTag_Ability_Sprint, false);
	TestTrue(TEXT("The GAS sprint tag resolves to a Boolean action that sustains held input"), SupportsHeldInput(Action));
	TestNotNull(TEXT("CMC native movement remains configured"), Input->FindNativeInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Move")), false));
	TestNotNull(TEXT("CMC native camera input remains configured"), Input->FindNativeInputActionForTag(FGameplayTag::RequestGameplayTag(TEXT("InputTag.Look.Mouse")), false));
	for (const TCHAR* Path : { BasePawnData, MantlePawnData })
	{
		const URpgPawnData* Data = LoadObject<URpgPawnData>(nullptr, Path);
		if (!TestNotNull(FString::Printf(TEXT("CMC composition exists: %s"), Path), Data)) continue;
		TestTrue(TEXT("CMC variants use the authored sprint input config"), Data->InputConfig == Input);
		TestEqual(TEXT("CMC variants compose the sprint set exactly once"), Data->AbilitySets.FilterByPredicate([Set](const auto& Entry) { return Entry == Set; }).Num(), 1);
		const ARpgCharacter* Pawn = Data->PawnClass ? Cast<ARpgCharacter>(Data->PawnClass->GetDefaultObject()) : nullptr;
		const URpgCharacterMovementComponent* Movement = Pawn ? Cast<URpgCharacterMovementComponent>(Pawn->GetCharacterMovement()) : nullptr;
		if (!TestNotNull(TEXT("CMC variants retain their native RPG movement component"), Movement)) continue;
		TestTrue(TEXT("CMC variants opt into GAS sprint"), Movement->bEnableGASSprint);
		TestTrue(TEXT("Authored sprint is finite and faster than ordinary movement"),
			FMath::IsFinite(Movement->MaxWalkSpeed) && Movement->MaxWalkSpeed > 0.f
			&& FMath::IsFinite(Movement->SprintSpeed) && Movement->SprintSpeed > Movement->MaxWalkSpeed);
	}
	// This is a saved-package contract, not a claim about key mappings, live movement or network receipt.
	// Those are exercised by the rendered PIE tests through ordinary Enhanced Input.
	CheckLocalDependencies(*this);
	return true;
}

#endif
