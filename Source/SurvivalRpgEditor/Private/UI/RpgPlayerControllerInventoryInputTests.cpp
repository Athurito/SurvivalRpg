#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddInputContextMapping.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InputKeyDelegateBinding.h"
#include "EnhancedInputActionDelegateBinding.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_InputKey.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr TCHAR PlayerControllerBlueprintPath[] =
		TEXT("/Game/SurvivalRpg/Core/Player/BP_Rpg_PlayerController.BP_Rpg_PlayerController");
	constexpr TCHAR InventoryInputActionPath[] =
		TEXT("/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/IA_UI_Inventory.IA_UI_Inventory");
	constexpr TCHAR MenuInputActionPath[] =
		TEXT("/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/IA_UI_Menu.IA_UI_Menu");
	constexpr TCHAR PlayerHudMappingContextPath[] =
		TEXT("/Game/SurvivalRpg/Input/InputMappings/IMC_UI_PlayerHUD.IMC_UI_PlayerHUD");
	constexpr TCHAR PlayerInputConfigPath[] =
		TEXT("/Game/SurvivalRpg/Input/DA_InputConfig.DA_InputConfig");
	constexpr TCHAR PlayerPawnDataPath[] =
		TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData");
	constexpr TCHAR PlayerCharacterBlueprintPath[] =
		TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character.BP_Rpg_Character");
	constexpr TCHAR PrototypeExperiencePath[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience");

	const FGameplayTag& GetInventoryInputTag()
	{
		static const FGameplayTag Tag =
			FGameplayTag::RequestGameplayTag(TEXT("InputTag.UI.Inventory"));
		return Tag;
	}

	const FGameplayTag& GetInventoryScreenTag()
	{
		static const FGameplayTag Tag =
			FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Inventory"));
		return Tag;
	}

	bool IsPlayerInventoryScreenCall(const UEdGraphNode* Node)
	{
		const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		const UFunction* ToggleFunction =
			URpgUIScreenBlueprintLibrary::StaticClass()->FindFunctionByName(
				GET_FUNCTION_NAME_CHECKED(URpgUIScreenBlueprintLibrary, ToggleUIScreen));
		const UFunction* OpenFunction =
			URpgUIScreenBlueprintLibrary::StaticClass()->FindFunctionByName(
				GET_FUNCTION_NAME_CHECKED(URpgUIScreenBlueprintLibrary, OpenUIScreen));
		if (!CallNode ||
			(CallNode->GetTargetFunction() != ToggleFunction &&
			 CallNode->GetTargetFunction() != OpenFunction))
		{
			return false;
		}

		const UEdGraphPin* ScreenTagPin =
			CallNode->FindPin(TEXT("ScreenTag"), EGPD_Input);
		if (!ScreenTagPin)
		{
			return false;
		}

		FGameplayTag ScreenTag;
		ScreenTag.FromExportString(ScreenTagPin->DefaultValue);
		return ScreenTag == GetInventoryScreenTag();
	}

	int32 CountKey(const TArray<FKey>& Keys, const FKey& Key)
	{
		int32 Count = 0;
		for (const FKey& Candidate : Keys)
		{
			Count += Candidate == Key ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerControllerInventoryInputAssetTest,
	"SurvivalRpg.UI.Input.PlayerInventoryNativeTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerControllerInventoryInputAssetTest::RunTest(const FString& Parameters)
{
	const UInputAction* InventoryAction =
		LoadObject<UInputAction>(nullptr, InventoryInputActionPath);
	const UInputAction* MenuAction =
		LoadObject<UInputAction>(nullptr, MenuInputActionPath);
	const UInputMappingContext* PlayerHudContext =
		LoadObject<UInputMappingContext>(nullptr, PlayerHudMappingContextPath);
	const URpgInputConfig* PlayerInputConfig =
		LoadObject<URpgInputConfig>(nullptr, PlayerInputConfigPath);
	const URpgPawnData* PlayerPawnData =
		LoadObject<URpgPawnData>(nullptr, PlayerPawnDataPath);
	UBlueprint* PlayerControllerBlueprint =
		LoadObject<UBlueprint>(nullptr, PlayerControllerBlueprintPath);
	if (!TestNotNull(TEXT("IA_UI_Inventory loads"), InventoryAction) ||
		!TestNotNull(TEXT("IA_UI_Menu loads"), MenuAction) ||
		!TestNotNull(TEXT("IMC_UI_PlayerHUD loads"), PlayerHudContext) ||
		!TestNotNull(TEXT("DA_InputConfig loads"), PlayerInputConfig) ||
		!TestNotNull(TEXT("DA_PawnData loads"), PlayerPawnData) ||
		!TestNotNull(TEXT("BP_Rpg_PlayerController loads"), PlayerControllerBlueprint))
	{
		return false;
	}

	TestTrue(
		TEXT("Player Controller Blueprint is compiled without errors"),
		PlayerControllerBlueprint->Status == BS_UpToDate ||
			PlayerControllerBlueprint->Status ==
				BS_UpToDateWithWarnings);
	if (!TestTrue(
		TEXT("Player Controller Blueprint has a generated class"),
		PlayerControllerBlueprint->GeneratedClass != nullptr))
	{
		return false;
	}

	TestTrue(TEXT("InputTag.UI.Inventory is registered"), GetInventoryInputTag().IsValid());
	TestTrue(
		TEXT("IA_UI_Inventory is a Boolean action"),
		InventoryAction->ValueType == EInputActionValueType::Boolean);
	TestEqual(
		TEXT("The Inventory input tag keeps its canonical name"),
		GetInventoryInputTag().ToString(),
		FString(TEXT("InputTag.UI.Inventory")));
	TestTrue(
		TEXT("DA_PawnData uses the audited player InputConfig"),
		PlayerPawnData->InputConfig.Get() == PlayerInputConfig);

	TArray<FKey> InventoryKeys;
	TArray<FKey> MenuKeys;
	for (const FEnhancedActionKeyMapping& Mapping : PlayerHudContext->GetMappings())
	{
		if (Mapping.Action == InventoryAction)
		{
			InventoryKeys.Add(Mapping.Key);
		}
		else if (Mapping.Action == MenuAction)
		{
			MenuKeys.Add(Mapping.Key);
		}
	}
	TestEqual(TEXT("IA_UI_Inventory has exactly two default mappings"), InventoryKeys.Num(), 2);
	TestEqual(TEXT("IA_UI_Inventory maps I exactly once"), CountKey(InventoryKeys, EKeys::I), 1);
	TestEqual(
		TEXT("IA_UI_Inventory maps Gamepad Special Right exactly once"),
		CountKey(InventoryKeys, EKeys::Gamepad_Special_Right),
		1);
	TestEqual(TEXT("IA_UI_Inventory does not own the legacy Y key"), CountKey(InventoryKeys, EKeys::Y), 0);

	TestEqual(TEXT("IA_UI_Menu has exactly two default mappings"), MenuKeys.Num(), 2);
	TestEqual(TEXT("IA_UI_Menu keeps Y exactly once"), CountKey(MenuKeys, EKeys::Y), 1);
	TestEqual(
		TEXT("IA_UI_Menu maps Gamepad Special Left exactly once"),
		CountKey(MenuKeys, EKeys::Gamepad_Special_Left),
		1);

	int32 InventoryNativeMappingCount = 0;
	for (const FRpgInputAction& Mapping : PlayerInputConfig->NativeInputActions)
	{
		if (Mapping.InputAction == InventoryAction ||
			Mapping.InputTag == GetInventoryInputTag())
		{
			++InventoryNativeMappingCount;
			TestEqual(
				TEXT("The Inventory native mapping uses IA_UI_Inventory"),
				Mapping.InputAction.Get(),
				InventoryAction);
			TestTrue(
				TEXT("The Inventory native mapping uses InputTag.UI.Inventory"),
				Mapping.InputTag == GetInventoryInputTag());
		}
	}
	TestEqual(
		TEXT("DA_InputConfig has exactly one Inventory native-action mapping"),
		InventoryNativeMappingCount,
		1);

	int32 InventoryAbilityMappingCount = 0;
	for (const FRpgInputAction& Mapping : PlayerInputConfig->AbilityInputActions)
	{
		InventoryAbilityMappingCount +=
			Mapping.InputAction == InventoryAction ||
			Mapping.InputTag == GetInventoryInputTag()
				? 1
				: 0;
	}
	TestEqual(
		TEXT("Inventory is not routed through GAS ability input"),
		InventoryAbilityMappingCount,
		0);
	TestEqual(
		TEXT("InputTag.UI.Inventory resolves to IA_UI_Inventory"),
		PlayerInputConfig->FindNativeInputActionForTag(
			GetInventoryInputTag(),
			/*bLogNotFound=*/ false),
		InventoryAction);

	TArray<UEdGraph*> Graphs;
	PlayerControllerBlueprint->GetAllGraphs(Graphs);

	int32 RawYKeyNodeCount = 0;
	int32 InventoryActionNodeCount = 0;
	int32 InventoryToggleCallCount = 0;

	for (const UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_InputKey* InputKeyNode = Cast<UK2Node_InputKey>(Node))
			{
				RawYKeyNodeCount += InputKeyNode->InputKey == EKeys::Y ? 1 : 0;
			}

			if (IsPlayerInventoryScreenCall(Node))
			{
				++InventoryToggleCallCount;
			}

			const UK2Node_EnhancedInputAction* EnhancedInputNode =
				Cast<UK2Node_EnhancedInputAction>(Node);
			if (EnhancedInputNode && EnhancedInputNode->InputAction == InventoryAction)
			{
				++InventoryActionNodeCount;
			}
		}
	}

	int32 CompiledRawYBindingCount = 0;
	if (const UInputKeyDelegateBinding* KeyBindings =
		Cast<UInputKeyDelegateBinding>(
			UBlueprintGeneratedClass::GetDynamicBindingObject(
				PlayerControllerBlueprint->GeneratedClass,
				UInputKeyDelegateBinding::StaticClass())))
	{
		for (const FBlueprintInputKeyDelegateBinding& Binding : KeyBindings->InputKeyDelegateBindings)
		{
			CompiledRawYBindingCount += Binding.InputChord.Key == EKeys::Y ? 1 : 0;
		}
	}

	int32 CompiledInventoryActionBindingCount = 0;
	if (const UEnhancedInputActionDelegateBinding* ActionBindings =
		Cast<UEnhancedInputActionDelegateBinding>(
			UBlueprintGeneratedClass::GetDynamicBindingObject(
				PlayerControllerBlueprint->GeneratedClass,
				UEnhancedInputActionDelegateBinding::StaticClass())))
	{
		for (const FBlueprintEnhancedInputActionBinding& Binding : ActionBindings->InputActionDelegateBindings)
		{
			CompiledInventoryActionBindingCount += Binding.InputAction == InventoryAction ? 1 : 0;
		}
	}

	TestEqual(
		TEXT("Player Controller has no direct raw Y-key event"),
		RawYKeyNodeCount,
		0);
	TestEqual(
		TEXT("Player Controller has no IA_UI_Inventory event node"),
		InventoryActionNodeCount,
		0);
	TestEqual(
		TEXT("Player Controller has no UI.Screen.Inventory open or toggle call"),
		InventoryToggleCallCount,
		0);
	TestEqual(
		TEXT("Compiled Player Controller has no raw Y-key binding"),
		CompiledRawYBindingCount,
		0);
	TestEqual(
		TEXT("Compiled Player Controller has no IA_UI_Inventory binding"),
		CompiledInventoryActionBindingCount,
		0);

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	const FName PlayerControllerPackage(
		TEXT("/Game/SurvivalRpg/Core/Player/BP_Rpg_PlayerController"));
	const FName PlayerInputConfigPackage(
		TEXT("/Game/SurvivalRpg/Input/DA_InputConfig"));
	const FName InventoryActionPackage(
		TEXT("/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/IA_UI_Inventory"));
	TestFalse(
		TEXT("Player Controller package no longer depends on IA_UI_Inventory"),
		AssetRegistry.ContainsDependency(
			PlayerControllerPackage,
			InventoryActionPackage));
	TestTrue(
		TEXT("DA_InputConfig owns the IA_UI_Inventory dependency"),
		AssetRegistry.ContainsDependency(
			PlayerInputConfigPackage,
			InventoryActionPackage));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerHudMappingCompositionAuthorityTest,
	"SurvivalRpg.UI.Input.PlayerHudMappingCompositionAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerHudMappingCompositionAuthorityTest::RunTest(const FString& Parameters)
{
	const UInputMappingContext* PlayerHudContext =
		LoadObject<UInputMappingContext>(nullptr, PlayerHudMappingContextPath);
	const UBlueprint* PrototypeExperienceBlueprint =
		LoadObject<UBlueprint>(nullptr, PrototypeExperiencePath);
	const UBlueprint* PlayerCharacterBlueprint =
		LoadObject<UBlueprint>(nullptr, PlayerCharacterBlueprintPath);
	if (!TestNotNull(TEXT("IMC_UI_PlayerHUD loads"), PlayerHudContext) ||
		!TestNotNull(TEXT("RpgPrototypeExperience Blueprint loads"), PrototypeExperienceBlueprint) ||
		!TestNotNull(TEXT("BP_Rpg_Character loads"), PlayerCharacterBlueprint))
	{
		return false;
	}

	const URpgExperienceDefinition* PrototypeExperience =
		PrototypeExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(
				PrototypeExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (!TestNotNull(
		TEXT("RpgPrototypeExperience generated defaults load"),
		PrototypeExperience))
	{
		return false;
	}

	TestTrue(
		TEXT("Player Character Blueprint is compiled and up to date"),
		PlayerCharacterBlueprint->Status == BS_UpToDate);

	const FSoftObjectPath ExpectedMappingPath(PlayerHudContext);
	int32 OwningActionCount = 0;
	int32 OwningMappingCount = 0;
	for (const UGameFeatureAction* Action : PrototypeExperience->Actions)
	{
		const URpgGameFeatureAction_AddInputContextMapping* MappingAction =
			Cast<URpgGameFeatureAction_AddInputContextMapping>(Action);
		if (!MappingAction)
		{
			continue;
		}

		int32 ActionMappingCount = 0;
		for (const FRpgInputMappingContextAndPriority& Mapping : MappingAction->InputMappings)
		{
			if (Mapping.InputMapping.ToSoftObjectPath() == ExpectedMappingPath)
			{
				++ActionMappingCount;
				++OwningMappingCount;
			}
		}
		OwningActionCount += ActionMappingCount > 0 ? 1 : 0;
	}

	TestEqual(
		TEXT("Prototype Experience has exactly one Player HUD mapping action"),
		OwningActionCount,
		1);
	TestEqual(
		TEXT("Prototype Experience owns IMC_UI_PlayerHUD exactly once"),
		OwningMappingCount,
		1);

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	const FName PlayerCharacterPackage(
		TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character"));
	const FName PrototypeExperiencePackage(
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience"));
	const FName PlayerHudMappingPackage(
		TEXT("/Game/SurvivalRpg/Input/InputMappings/IMC_UI_PlayerHUD"));
	TestFalse(
		TEXT("Player Character no longer owns IMC_UI_PlayerHUD"),
		AssetRegistry.ContainsDependency(
			PlayerCharacterPackage,
			PlayerHudMappingPackage));
	TestTrue(
		TEXT("Prototype Experience owns the IMC_UI_PlayerHUD package dependency"),
		AssetRegistry.ContainsDependency(
			PrototypeExperiencePackage,
			PlayerHudMappingPackage));

	return true;
}

#endif
