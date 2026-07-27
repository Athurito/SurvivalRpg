// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CommonInputSettings.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "PlayerMappableKeySettings.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Interact.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptWidget.h"
#include "UObject/UnrealType.h"

namespace RpgInteractionAssetContractTests
{
	constexpr TCHAR InteractActionPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/Actions/MovementActions/"
			"IA_Interact.IA_Interact");
	constexpr TCHAR MovementMappingContextPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/InputMappings/"
			"IMC_Movement.IMC_Movement");
	constexpr TCHAR InputConfigPath[] =
		TEXT("/Game/SurvivalRpg/Input/DA_InputConfig.DA_InputConfig");
	constexpr TCHAR DefaultAbilitySetPath[] =
		TEXT(
			"/Game/SurvivalRpg/Core/Character/"
			"DA_DefaultAbilitySet.DA_DefaultAbilitySet");
	constexpr TCHAR InteractionAbilityClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Interaction/Abilities/"
			"GA_Interaction.GA_Interaction_C");
	constexpr TCHAR InteractionPromptClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Interaction/"
			"CUI_InteractionPrompt.CUI_InteractionPrompt_C");
	constexpr TCHAR NearbyIndicatorClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Interaction/"
			"CUI_InteractionNearbyIndicator."
			"CUI_InteractionNearbyIndicator_C");

	const FFloatProperty* FindInteractionFloatProperty(
		const URpgGameplayAbility_Interact& Ability,
		const FName PropertyName)
	{
		return FindFProperty<FFloatProperty>(
			Ability.GetClass(),
			PropertyName);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionInputAssetContractTest,
	"SurvivalRpg.Interaction.Assets.InputContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionInputAssetContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionAssetContractTests;

	const UInputAction* InteractAction =
		LoadObject<UInputAction>(nullptr, InteractActionPath);
	const UInputMappingContext* MovementContext =
		LoadObject<UInputMappingContext>(
			nullptr,
			MovementMappingContextPath);
	const URpgInputConfig* InputConfig =
		LoadObject<URpgInputConfig>(nullptr, InputConfigPath);
	if (!TestNotNull(TEXT("IA_Interact loads"), InteractAction) ||
		!TestNotNull(TEXT("IMC_Movement loads"), MovementContext) ||
		!TestNotNull(TEXT("DA_InputConfig loads"), InputConfig))
	{
		return false;
	}

	const UPlayerMappableKeySettings* PlayerMappableSettings =
		InteractAction->GetPlayerMappableKeySettings();
	if (!TestNotNull(
			TEXT("IA_Interact has player-mappable settings"),
			PlayerMappableSettings))
	{
		return false;
	}

	TestEqual(
		TEXT("IA_Interact uses the stable player-mappable name"),
		PlayerMappableSettings->GetMappingName(),
		FName(TEXT("Interact")));
	TestEqual(
		TEXT("IA_Interact exposes the localized German display name"),
		PlayerMappableSettings->DisplayName.ToString(),
		FString(TEXT("Interagieren")));
	TestEqual(
		TEXT("IA_Interact is grouped under Gameplay"),
		PlayerMappableSettings->DisplayCategory.ToString(),
		FString(TEXT("Gameplay")));

	int32 InteractMappingCount = 0;
	int32 KeyboardMappingCount = 0;
	int32 GamepadMappingCount = 0;
	for (const FEnhancedActionKeyMapping& Mapping :
		MovementContext->GetMappings())
	{
		if (Mapping.Action != InteractAction)
		{
			continue;
		}

		++InteractMappingCount;
		KeyboardMappingCount += Mapping.Key == EKeys::E ? 1 : 0;
		GamepadMappingCount +=
			Mapping.Key == EKeys::Gamepad_FaceButton_Left ? 1 : 0;
		TestTrue(
			TEXT("Every IA_Interact mapping is player mappable"),
			Mapping.IsPlayerMappable());
		TestEqual(
			TEXT("Every IA_Interact mapping resolves the stable name"),
			Mapping.GetMappingName(),
			FName(TEXT("Interact")));
		TestTrue(
			TEXT("IA_Interact has no unexpected default binding"),
			Mapping.Key == EKeys::E ||
				Mapping.Key == EKeys::Gamepad_FaceButton_Left);
	}

	TestEqual(
		TEXT("IMC_Movement owns exactly two IA_Interact defaults"),
		InteractMappingCount,
		2);
	TestEqual(
		TEXT("IA_Interact has one keyboard E default"),
		KeyboardMappingCount,
		1);
	TestEqual(
		TEXT("IA_Interact has one left face-button gamepad default"),
		GamepadMappingCount,
		1);

	int32 SemanticMappingCount = 0;
	for (const FRpgInputAction& Mapping : InputConfig->AbilityInputActions)
	{
		if (Mapping.InputAction.Get() != InteractAction &&
			Mapping.InputTag != RpgGameplayTags::InputTag_Ability_Interact)
		{
			continue;
		}

		++SemanticMappingCount;
		TestEqual(
			TEXT("Interaction semantic mapping uses IA_Interact"),
			Mapping.InputAction.Get(),
			InteractAction);
		TestTrue(
			TEXT("IA_Interact uses InputTag.Ability.Interact"),
			Mapping.InputTag ==
				RpgGameplayTags::InputTag_Ability_Interact);
	}
	TestEqual(
		TEXT("DA_InputConfig owns one unambiguous interaction mapping"),
		SemanticMappingCount,
		1);
	TestEqual(
		TEXT("The interaction tag resolves back to IA_Interact"),
		InputConfig->FindAbilityInputActionForTag(
			RpgGameplayTags::InputTag_Ability_Interact,
			/*bLogNotFound=*/ false),
		InteractAction);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionAbilityAssetContractTest,
	"SurvivalRpg.Interaction.Assets.AbilityContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionAbilityAssetContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionAssetContractTests;

	const URpgAbilitySet* AbilitySet =
		LoadObject<URpgAbilitySet>(nullptr, DefaultAbilitySetPath);
	UClass* InteractionAbilityClass =
		LoadClass<URpgGameplayAbility_Interact>(
			nullptr,
			InteractionAbilityClassPath);
	if (!TestNotNull(
			TEXT("DA_DefaultAbilitySet loads"),
			AbilitySet) ||
		!TestNotNull(
			TEXT("GA_Interaction generated class loads"),
			InteractionAbilityClass))
	{
		return false;
	}

	const FArrayProperty* GrantedAbilitiesProperty =
		FindFProperty<FArrayProperty>(
			URpgAbilitySet::StaticClass(),
			TEXT("GrantedGameplayAbilities"));
	if (!TestNotNull(
			TEXT("AbilitySet exposes its reflected grant array"),
			GrantedAbilitiesProperty))
	{
		return false;
	}

	const FStructProperty* GrantEntryProperty =
		CastField<FStructProperty>(GrantedAbilitiesProperty->Inner);
	if (!TestNotNull(
			TEXT("AbilitySet grant entries are reflected structs"),
			GrantEntryProperty) ||
		!TestEqual(
			TEXT("AbilitySet uses the expected grant-entry type"),
			GrantEntryProperty->Struct.Get(),
			FRpgAbilitySet_GameplayAbility::StaticStruct()))
	{
		return false;
	}

	const void* GrantedAbilities =
		GrantedAbilitiesProperty->ContainerPtrToValuePtr<void>(AbilitySet);
	FScriptArrayHelper GrantArray(
		GrantedAbilitiesProperty,
		GrantedAbilities);
	int32 InteractionGrantCount = 0;
	for (int32 Index = 0; Index < GrantArray.Num(); ++Index)
	{
		const auto* Grant =
			reinterpret_cast<const FRpgAbilitySet_GameplayAbility*>(
				GrantArray.GetRawPtr(Index));
		if (Grant->Ability.Get() != InteractionAbilityClass &&
			Grant->InputTag != RpgGameplayTags::InputTag_Ability_Interact)
		{
			continue;
		}

		++InteractionGrantCount;
		TestEqual(
			TEXT("Interaction grant uses GA_Interaction"),
			Grant->Ability.Get(),
			InteractionAbilityClass);
		TestTrue(
			TEXT("GA_Interaction grant uses InputTag.Ability.Interact"),
			Grant->InputTag ==
				RpgGameplayTags::InputTag_Ability_Interact);
	}
	TestEqual(
		TEXT("DA_DefaultAbilitySet grants GA_Interaction exactly once"),
		InteractionGrantCount,
		1);

	const URpgGameplayAbility_Interact* InteractionAbility =
		InteractionAbilityClass->
			GetDefaultObject<URpgGameplayAbility_Interact>();
	if (!TestNotNull(
			TEXT("GA_Interaction CDO exists"),
			InteractionAbility))
	{
		return false;
	}

	TestEqual(
		TEXT("GA_Interaction activates when granted"),
		InteractionAbility->GetActivationPolicy(),
		ERpgAbilityActivationPolicy::OnSpawn);

	const auto TestFloatProperty =
		[this, InteractionAbility](
			const TCHAR* Label,
			const FName PropertyName,
			const float ExpectedValue)
		{
			const FFloatProperty* Property =
				RpgInteractionAssetContractTests::
					FindInteractionFloatProperty(
						*InteractionAbility,
						PropertyName);
			if (!TestNotNull(Label, Property))
			{
				return;
			}
			const float ActualValue =
				Property->GetPropertyValue_InContainer(
					InteractionAbility);
			TestTrue(
				*FString::Printf(
					TEXT("%s has the authored value"),
					Label),
				FMath::IsNearlyEqual(
					ActualValue,
					ExpectedValue));
		};
	TestFloatProperty(
		TEXT("InteractionScanRange property"),
		TEXT("InteractionScanRange"),
		650.0f);
	TestFloatProperty(
		TEXT("AwarenessScanRange property"),
		TEXT("AwarenessScanRange"),
		1000.0f);
	TestFloatProperty(
		TEXT("FocusSweepRadius property"),
		TEXT("FocusSweepRadius"),
		12.0f);

	UClass* InteractionPromptClass =
		LoadClass<URpgInteractionPromptWidget>(
			nullptr,
			InteractionPromptClassPath);
	UClass* NearbyIndicatorClass =
		LoadClass<URpgInteractionPromptWidget>(
			nullptr,
			NearbyIndicatorClassPath);
	TestNotNull(
		TEXT("CUI_InteractionPrompt loads as the native prompt type"),
		InteractionPromptClass);
	TestNotNull(
		TEXT(
			"CUI_InteractionNearbyIndicator loads as the native "
			"prompt type"),
		NearbyIndicatorClass);

	const auto TestSoftClassProperty =
		[this, InteractionAbility](
			const TCHAR* Label,
			const FName PropertyName,
			const TCHAR* ExpectedPath)
		{
			const FSoftClassProperty* Property =
				FindFProperty<FSoftClassProperty>(
					InteractionAbility->GetClass(),
					PropertyName);
			if (!TestNotNull(Label, Property))
			{
				return;
			}

			const FSoftObjectPtr Value =
				Property->GetPropertyValue_InContainer(
					InteractionAbility);
			TestEqual(
				*FString::Printf(
					TEXT("%s references the authored widget"),
					Label),
				Value.ToSoftObjectPath().ToString(),
				FString(ExpectedPath));
		};
	TestSoftClassProperty(
		TEXT("DefaultInteractionWidgetClass property"),
		TEXT("DefaultInteractionWidgetClass"),
		InteractionPromptClassPath);
	TestSoftClassProperty(
		TEXT("DefaultNearbyWidgetClass property"),
		TEXT("DefaultNearbyWidgetClass"),
		NearbyIndicatorClassPath);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionCommonInputContractTest,
	"SurvivalRpg.Interaction.Assets.CommonInputContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionCommonInputContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const UCommonInputSettings* CommonInputSettings =
		GetDefault<UCommonInputSettings>();
	if (!TestNotNull(
			TEXT("CommonInput settings CDO exists"),
			CommonInputSettings))
	{
		return false;
	}

	TestTrue(
		TEXT("DefaultGame enables CommonInput Enhanced Input support"),
		CommonInputSettings->GetEnableEnhancedInputSupport());
	TestTrue(
		TEXT("CommonInput exposes Enhanced Input support as active"),
		UCommonInputSettings::IsEnhancedInputSupportEnabled());
	return true;
}

#endif
