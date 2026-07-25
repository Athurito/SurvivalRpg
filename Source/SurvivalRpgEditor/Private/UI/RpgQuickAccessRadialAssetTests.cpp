#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddInputContextMapping.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddWidgets.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarSlotViewModel.h"
#include "SurvivalRpg/UI/RpgQuickAccessRadialWidget.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMPropertyPath.h"
#include "Types/MVVMBindingMode.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "WidgetBlueprint.h"
#include "Widgets/UIExtensionPointWidget.h"

namespace RpgQuickAccessRadialAssetTests
{
	constexpr TCHAR HoldActionPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/"
			"IA_UI_QuickAccessRadial_Hold.IA_UI_QuickAccessRadial_Hold");
	constexpr TCHAR SelectActionPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/"
			"IA_UI_QuickAccessRadial_Select.IA_UI_QuickAccessRadial_Select");
	constexpr TCHAR CancelActionPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/Actions/PlayerHUDActions/"
			"IA_UI_QuickAccessRadial_Cancel.IA_UI_QuickAccessRadial_Cancel");
	constexpr TCHAR PlayerHudMappingContextPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/InputMappings/"
			"IMC_UI_PlayerHUD.IMC_UI_PlayerHUD");
	constexpr TCHAR RadialMappingContextPath[] =
		TEXT(
			"/Game/SurvivalRpg/Input/InputMappings/"
			"IMC_UI_QuickAccessRadial.IMC_UI_QuickAccessRadial");
	constexpr TCHAR PlayerInputConfigPath[] =
		TEXT("/Game/SurvivalRpg/Input/DA_InputConfig.DA_InputConfig");

	constexpr TCHAR HudLayoutClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_RpgHudLayout.CUI_RpgHudLayout_C");
	constexpr TCHAR RadialClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_QuickAccessRadial.CUI_QuickAccessRadial_C");
	constexpr TCHAR RadialBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_QuickAccessRadial.CUI_QuickAccessRadial");
	constexpr TCHAR RadialSlotClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_QuickAccessRadialSlotEntry."
			"CUI_QuickAccessRadialSlotEntry_C");
	constexpr TCHAR RadialSlotBlueprintPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/"
			"CUI_QuickAccessRadialSlotEntry."
			"CUI_QuickAccessRadialSlotEntry");
	constexpr TCHAR PrototypeExperiencePath[] =
		TEXT(
			"/Game/SurvivalRpg/System/Experiences/"
			"RpgPrototypeExperience.RpgPrototypeExperience");
	constexpr TCHAR CommonUiActionTablePath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Input/"
			"CDT_RpgUIActions_All.CDT_RpgUIActions_All");

	struct FExpectedRadialInput
	{
		const TCHAR* Label;
		const TCHAR* ActionPath;
		const TCHAR* TagName;
		EInputActionValueType ValueType;
		FKey Key;
	};

	bool HasBlueprintGraphLogic(const UWidgetBlueprint& Blueprint)
	{
		const auto HasNodes =
			[](const TArray<TObjectPtr<UEdGraph>>& Graphs)
			{
				return Graphs.ContainsByPredicate(
					[](const TObjectPtr<UEdGraph>& Graph)
					{
						return Graph && !Graph->Nodes.IsEmpty();
					});
			};

		return HasNodes(Blueprint.UbergraphPages)
			|| HasNodes(Blueprint.FunctionGraphs)
			|| HasNodes(Blueprint.MacroGraphs)
			|| HasNodes(Blueprint.DelegateSignatureGraphs);
	}

	UClass* ResolveBlueprintContextClass(
		const UWidgetBlueprint& Blueprint)
	{
		return Blueprint.SkeletonGeneratedClass
			? Blueprint.SkeletonGeneratedClass
			: Blueprint.GeneratedClass
				? Blueprint.GeneratedClass
				: Blueprint.ParentClass;
	}

	bool HasExactFieldPath(
		const FMVVMBlueprintPropertyPath& Path,
		const UWidgetBlueprint& Blueprint,
		const FName ExpectedField)
	{
		const TArray<FName> FieldNames =
			Path.GetFieldNames(
				ResolveBlueprintContextClass(Blueprint));
		return FieldNames.Num() == 1
			&& FieldNames[0] == ExpectedField;
	}

	struct FMvvmBindingContract
	{
		FName SourceField;
		FName DestinationWidget;
		UFunction* DestinationSetter = nullptr;
	};

	const FGameplayTag* GetExtensionPointTag(
		const UUIExtensionPointWidget* ExtensionPoint)
	{
		const FStructProperty* TagProperty =
			FindFProperty<FStructProperty>(
				UUIExtensionPointWidget::StaticClass(),
				TEXT("ExtensionPointTag"));
		return ExtensionPoint && TagProperty
			? TagProperty->ContainerPtrToValuePtr<FGameplayTag>(
				ExtensionPoint)
			: nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessRadialInputAssetTest,
	"SurvivalRpg.UI.QuickAccessRadial.InputAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessRadialInputAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgQuickAccessRadialAssetTests;

	const FExpectedRadialInput ExpectedInputs[] = {
		{
			TEXT("Hold"),
			HoldActionPath,
			TEXT("InputTag.UI.QuickAccessRadial.Hold"),
			EInputActionValueType::Boolean,
			EKeys::Gamepad_DPad_Up
		},
		{
			TEXT("Select"),
			SelectActionPath,
			TEXT("InputTag.UI.QuickAccessRadial.Select"),
			EInputActionValueType::Axis2D,
			EKeys::Gamepad_Right2D
		},
		{
			TEXT("Cancel"),
			CancelActionPath,
			TEXT("InputTag.UI.QuickAccessRadial.Cancel"),
			EInputActionValueType::Boolean,
			EKeys::Gamepad_FaceButton_Right
		}
	};

	const UInputMappingContext* PlayerHudContext =
		LoadObject<UInputMappingContext>(
			nullptr,
			PlayerHudMappingContextPath);
	const UInputMappingContext* RadialContext =
		LoadObject<UInputMappingContext>(
			nullptr,
			RadialMappingContextPath);
	const URpgInputConfig* PlayerInputConfig =
		LoadObject<URpgInputConfig>(nullptr, PlayerInputConfigPath);
	if (!TestNotNull(
			TEXT("IMC_UI_PlayerHUD loads"),
			PlayerHudContext) ||
		!TestNotNull(
			TEXT("IMC_UI_QuickAccessRadial loads"),
			RadialContext) ||
		!TestNotNull(
			TEXT("DA_InputConfig loads"),
			PlayerInputConfig))
	{
		return false;
	}

	TArray<const UInputAction*> LoadedActions;
	LoadedActions.Reserve(UE_ARRAY_COUNT(ExpectedInputs));
	bool bAllActionsLoaded = true;
	for (const FExpectedRadialInput& Expected : ExpectedInputs)
	{
		const UInputAction* Action =
			LoadObject<UInputAction>(nullptr, Expected.ActionPath);
		LoadedActions.Add(Action);
		bAllActionsLoaded &=
			TestNotNull(
				*FString::Printf(
					TEXT("Quick Access Radial %s action loads"),
					Expected.Label),
				Action);
	}
	if (!bAllActionsLoaded)
	{
		return false;
	}

	TestEqual(
		TEXT(
			"IMC_UI_QuickAccessRadial contains exactly the three "
			"radial mappings"),
		RadialContext->GetMappings().Num(),
		static_cast<int32>(UE_ARRAY_COUNT(ExpectedInputs)));

	int32 LegacyPlayerHudRadialMappingCount = 0;
	for (const FEnhancedActionKeyMapping& Mapping :
		PlayerHudContext->GetMappings())
	{
		LegacyPlayerHudRadialMappingCount +=
			LoadedActions.Contains(Mapping.Action.Get())
				? 1
				: 0;
	}
	TestEqual(
		TEXT(
			"IMC_UI_PlayerHUD owns no Quick Access Radial actions"),
		LegacyPlayerHudRadialMappingCount,
		0);

	for (int32 InputIndex = 0;
		InputIndex < UE_ARRAY_COUNT(ExpectedInputs);
		++InputIndex)
	{
		const FExpectedRadialInput& Expected = ExpectedInputs[InputIndex];
		const UInputAction* Action = LoadedActions[InputIndex];

		TestTrue(
			*FString::Printf(
				TEXT("%s action has the expected value type"),
				Expected.Label),
			Action->ValueType == Expected.ValueType);
		TestFalse(
			*FString::Printf(
				TEXT("%s action does not consume lower-priority mappings"),
				Expected.Label),
			Action->bConsumeInput);
		TestEqual(
			*FString::Printf(
				TEXT("%s action owns no action-level triggers"),
				Expected.Label),
			Action->Triggers.Num(),
			0);
		TestEqual(
			*FString::Printf(
				TEXT("%s action owns no action-level modifiers"),
				Expected.Label),
			Action->Modifiers.Num(),
			0);

		int32 RelatedMappingCount = 0;
		for (const FEnhancedActionKeyMapping& Mapping :
			RadialContext->GetMappings())
		{
			const bool bUsesExpectedAction =
				Mapping.Action == Action;
			const bool bUsesExpectedKey =
				Mapping.Key == Expected.Key;
			if (!bUsesExpectedAction && !bUsesExpectedKey)
			{
				continue;
			}

			++RelatedMappingCount;
			TestTrue(
				*FString::Printf(
					TEXT("%s mapping uses its exact InputAction"),
					Expected.Label),
				bUsesExpectedAction);
			TestTrue(
				*FString::Printf(
					TEXT("%s mapping uses its exact default key"),
					Expected.Label),
				bUsesExpectedKey);
			TestEqual(
				*FString::Printf(
					TEXT("%s mapping owns no mapping-level triggers"),
					Expected.Label),
				Mapping.Triggers.Num(),
				0);
			TestEqual(
				*FString::Printf(
					TEXT("%s mapping owns no mapping-level modifiers"),
					Expected.Label),
				Mapping.Modifiers.Num(),
				0);
		}
		TestEqual(
			*FString::Printf(
				TEXT(
					"%s has exactly one exclusive Action-Key mapping "
					"in IMC_UI_QuickAccessRadial"),
				Expected.Label),
			RelatedMappingCount,
			1);

		const FGameplayTag InputTag =
			FGameplayTag::RequestGameplayTag(
				FName(Expected.TagName),
				/*ErrorIfNotFound=*/ false);
		TestTrue(
			*FString::Printf(
				TEXT("%s native input tag is registered"),
				Expected.Label),
			InputTag.IsValid());
		TestEqual(
			*FString::Printf(
				TEXT("%s native input tag keeps its exact name"),
				Expected.Label),
			InputTag.ToString(),
			FString(Expected.TagName));

		int32 NativeMappingCount = 0;
		for (const FRpgInputAction& Mapping :
			PlayerInputConfig->NativeInputActions)
		{
			if (Mapping.InputAction.Get() != Action &&
				Mapping.InputTag != InputTag)
			{
				continue;
			}

			++NativeMappingCount;
			TestEqual(
				*FString::Printf(
					TEXT("%s native mapping uses its exact InputAction"),
					Expected.Label),
				Mapping.InputAction.Get(),
				Action);
			TestTrue(
				*FString::Printf(
					TEXT("%s native mapping uses its exact input tag"),
					Expected.Label),
				Mapping.InputTag == InputTag);
		}
		TestEqual(
			*FString::Printf(
				TEXT(
					"DA_InputConfig owns exactly one %s native mapping"),
				Expected.Label),
			NativeMappingCount,
			1);

		int32 AbilityMappingCount = 0;
		for (const FRpgInputAction& Mapping :
			PlayerInputConfig->AbilityInputActions)
		{
			AbilityMappingCount +=
				Mapping.InputAction.Get() == Action ||
					Mapping.InputTag == InputTag
					? 1
					: 0;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s is not routed through GAS ability input"),
				Expected.Label),
			AbilityMappingCount,
			0);
		TestEqual(
			*FString::Printf(
				TEXT("%s native tag resolves to its exact InputAction"),
				Expected.Label),
			PlayerInputConfig->FindNativeInputActionForTag(
				InputTag,
				/*bLogNotFound=*/ false),
			Action);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessRadialCompositionAssetTest,
	"SurvivalRpg.UI.QuickAccessRadial.CompositionAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessRadialCompositionAssetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgQuickAccessRadialAssetTests;

	UClass* HudLayoutClass =
		LoadClass<UUserWidget>(nullptr, HudLayoutClassPath);
	UClass* RadialClass =
		LoadClass<URpgQuickAccessRadialWidget>(
			nullptr,
			RadialClassPath);
	UClass* RadialSlotClass =
		LoadClass<URpgQuickAccessRadialSlotWidget>(
			nullptr,
			RadialSlotClassPath);
	const UInputMappingContext* RadialContext =
		LoadObject<UInputMappingContext>(
			nullptr,
			RadialMappingContextPath);
	const UWidgetBlueprint* RadialBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			RadialBlueprintPath);
	const UWidgetBlueprint* RadialSlotBlueprint =
		LoadObject<UWidgetBlueprint>(
			nullptr,
			RadialSlotBlueprintPath);
	const UBlueprint* PrototypeExperienceBlueprint =
		LoadObject<UBlueprint>(nullptr, PrototypeExperiencePath);
	if (!TestNotNull(
			TEXT("CUI_RpgHudLayout class loads"),
			HudLayoutClass) ||
		!TestNotNull(
			TEXT("CUI_QuickAccessRadial class loads"),
			RadialClass) ||
		!TestNotNull(
			TEXT("CUI_QuickAccessRadialSlotEntry class loads"),
			RadialSlotClass) ||
		!TestNotNull(
			TEXT("IMC_UI_QuickAccessRadial loads"),
			RadialContext) ||
		!TestNotNull(
			TEXT("CUI_QuickAccessRadial Blueprint loads"),
			RadialBlueprint) ||
		!TestNotNull(
			TEXT("CUI_QuickAccessRadialSlotEntry Blueprint loads"),
			RadialSlotBlueprint) ||
		!TestNotNull(
			TEXT("RpgPrototypeExperience Blueprint loads"),
			PrototypeExperienceBlueprint))
	{
		return false;
	}

	TestEqual(
		TEXT(
			"CUI_QuickAccessRadial has the exact native radial presenter "
			"as parent"),
		RadialClass->GetSuperClass(),
		URpgQuickAccessRadialWidget::StaticClass());
	TestEqual(
		TEXT(
			"CUI_QuickAccessRadialSlotEntry has the exact native slot "
			"presenter as parent"),
		RadialSlotClass->GetSuperClass(),
		URpgQuickAccessRadialSlotWidget::StaticClass());

	UWidgetBlueprintGeneratedClass* HudGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(HudLayoutClass);
	UWidgetBlueprintGeneratedClass* RadialGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(RadialClass);
	UWidgetBlueprintGeneratedClass* RadialSlotGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(RadialSlotClass);
	if (!TestNotNull(
			TEXT("HUD layout is an authored Widget Blueprint"),
			HudGeneratedClass) ||
		!TestNotNull(
			TEXT("Quick Access Radial is an authored Widget Blueprint"),
			RadialGeneratedClass) ||
		!TestNotNull(
			TEXT(
				"Quick Access Radial Slot Entry is an authored "
				"Widget Blueprint"),
			RadialSlotGeneratedClass))
	{
		return false;
	}

	const URpgQuickAccessRadialWidget* RadialDefaults =
		Cast<URpgQuickAccessRadialWidget>(
			RadialClass->GetDefaultObject());
	if (!TestNotNull(
			TEXT("Quick Access Radial defaults load"),
			RadialDefaults))
	{
		return false;
	}
	TestTrue(
		TEXT("Quick Access Radial defaults to Collapsed"),
		RadialDefaults->GetVisibility() ==
			ESlateVisibility::Collapsed);
	TestFalse(
		TEXT("Quick Access Radial is not focusable"),
		RadialDefaults->IsFocusable());

	const UWidgetTree* HudTree =
		HudGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* RadialTree =
		RadialGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* RadialSlotTree =
		RadialSlotGeneratedClass->GetWidgetTreeArchetype();
	if (!TestNotNull(
			TEXT("HUD layout has an authored WidgetTree"),
			HudTree) ||
		!TestNotNull(
			TEXT("Quick Access Radial has an authored WidgetTree"),
			RadialTree) ||
		!TestNotNull(
			TEXT(
				"Quick Access Radial Slot Entry has an authored "
				"WidgetTree"),
			RadialSlotTree))
	{
		return false;
	}

	TestFalse(
		TEXT(
			"Quick Access Radial owns no Blueprint gameplay "
			"graph logic"),
		HasBlueprintGraphLogic(*RadialBlueprint));
	TestFalse(
		TEXT(
			"Quick Access Radial Slot Entry owns no Blueprint "
			"gameplay graph logic"),
		HasBlueprintGraphLogic(*RadialSlotBlueprint));

	const FGameplayTag RadialExtensionTag =
		FGameplayTag::RequestGameplayTag(
			TEXT("UI.HUD.Slot.QuickAccessRadial"),
			/*ErrorIfNotFound=*/ false);
	TestTrue(
		TEXT("UI.HUD.Slot.QuickAccessRadial is registered"),
		RadialExtensionTag.IsValid());

	int32 MatchingExtensionPointCount = 0;
	HudTree->ForEachWidget(
		[&MatchingExtensionPointCount, &RadialExtensionTag](
			UWidget* Widget)
		{
			const UUIExtensionPointWidget* ExtensionPoint =
				Cast<UUIExtensionPointWidget>(Widget);
			const FGameplayTag* ExtensionTag =
				GetExtensionPointTag(ExtensionPoint);
			MatchingExtensionPointCount +=
				ExtensionTag &&
					*ExtensionTag == RadialExtensionTag
					? 1
					: 0;
		});
	TestEqual(
		TEXT(
			"HUD layout owns exactly one "
			"UI.HUD.Slot.QuickAccessRadial extension point"),
		MatchingExtensionPointCount,
		1);

	int32 AuthoredRadialSlotCount = 0;
	RadialTree->ForEachWidget(
		[&AuthoredRadialSlotCount, RadialSlotClass](UWidget* Widget)
		{
			AuthoredRadialSlotCount +=
				Widget && Widget->IsA(RadialSlotClass)
					? 1
					: 0;
		});
	TestEqual(
		TEXT("Quick Access Radial owns exactly eight slot-entry children"),
		AuthoredRadialSlotCount,
		8);

	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		const FName SlotName(
			*FString::Printf(TEXT("RadialSlot_%d"), SlotIndex));
		const UWidget* SlotWidget =
			RadialTree->FindWidget(SlotName);
		if (TestNotNull(
				*FString::Printf(
					TEXT("%s exists"),
					*SlotName.ToString()),
				SlotWidget))
		{
			TestEqual(
				*FString::Printf(
					TEXT("%s uses the canonical slot-entry class"),
					*SlotName.ToString()),
				SlotWidget->GetClass(),
				RadialSlotClass);
		}
	}

	TestNotNull(
		TEXT("Radial slot MVVM destination ItemIcon is an Image"),
		Cast<UImage>(
			RadialSlotTree->FindWidget(TEXT("ItemIcon"))));
	const FObjectPropertyBase* NativeItemIconProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgQuickAccessRadialSlotWidget::StaticClass(),
			TEXT("ItemIcon"));
	if (TestNotNull(
			TEXT("Native radial slot declares ItemIcon"),
			NativeItemIconProperty))
	{
		TestEqual(
			TEXT("Native ItemIcon property has the exact UImage type"),
			NativeItemIconProperty->PropertyClass.Get(),
			UImage::StaticClass());
		TestTrue(
			TEXT("Native ItemIcon property requires BindWidget"),
			NativeItemIconProperty->HasMetaData(TEXT("BindWidget")));
	}
	TestNotNull(
		TEXT(
			"Radial slot MVVM destination ItemNameText is a "
			"TextBlock"),
		Cast<UTextBlock>(
			RadialSlotTree->FindWidget(TEXT("ItemNameText"))));
	TestNotNull(
		TEXT(
			"Radial slot MVVM destination StackCountText is a "
			"TextBlock"),
		Cast<UTextBlock>(
			RadialSlotTree->FindWidget(TEXT("StackCountText"))));

	const URpgExperienceDefinition* PrototypeExperience =
		PrototypeExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(
				PrototypeExperienceBlueprint->GeneratedClass
					->GetDefaultObject())
			: nullptr;
	if (!TestNotNull(
			TEXT("RpgPrototypeExperience generated defaults load"),
			PrototypeExperience))
	{
		return false;
	}

	const FSoftObjectPath ExpectedRadialClassPath(RadialClass);
	int32 RelatedRadialRegistrationCount = 0;
	for (const UGameFeatureAction* Action :
		PrototypeExperience->Actions)
	{
		const URpgGameFeatureAction_AddWidgets* AddWidgetsAction =
			Cast<URpgGameFeatureAction_AddWidgets>(Action);
		if (!AddWidgetsAction)
		{
			continue;
		}

		for (const FRpgGameFeatureWidgetEntry& WidgetEntry :
			AddWidgetsAction->Widgets)
		{
			const bool bUsesRadialClass =
				WidgetEntry.WidgetClass.ToSoftObjectPath() ==
					ExpectedRadialClassPath;
			const bool bUsesRadialSlot =
				WidgetEntry.SlotTag == RadialExtensionTag;
			if (!bUsesRadialClass && !bUsesRadialSlot)
			{
				continue;
			}

			++RelatedRadialRegistrationCount;
			TestTrue(
				TEXT(
					"Quick Access Radial registration uses "
					"CUI_QuickAccessRadial"),
				bUsesRadialClass);
			TestTrue(
				TEXT(
					"Quick Access Radial registration uses "
					"UI.HUD.Slot.QuickAccessRadial"),
				bUsesRadialSlot);
		}
	}
	TestEqual(
		TEXT(
			"RpgPrototypeExperience registers CUI_QuickAccessRadial "
			"exactly once"),
		RelatedRadialRegistrationCount,
		1);

	const FSoftObjectPath ExpectedRadialMappingPath(RadialContext);
	int32 RadialMappingRegistrationCount = 0;
	for (const UGameFeatureAction* Action :
		PrototypeExperience->Actions)
	{
		const URpgGameFeatureAction_AddInputContextMapping*
			AddInputMappingsAction =
				Cast<URpgGameFeatureAction_AddInputContextMapping>(
					Action);
		if (!AddInputMappingsAction)
		{
			continue;
		}

		for (const FRpgInputMappingContextAndPriority& Mapping :
			AddInputMappingsAction->InputMappings)
		{
			if (Mapping.InputMapping.ToSoftObjectPath() !=
				ExpectedRadialMappingPath)
			{
				continue;
			}

			++RadialMappingRegistrationCount;
			TestEqual(
				TEXT(
					"Prototype Experience registers the radial IMC "
					"at priority 10"),
				Mapping.Priority,
				10);
			TestFalse(
				TEXT(
					"Prototype Experience keeps the fixed radial IMC "
					"out of Enhanced Input user settings"),
				Mapping.bRegisterWithSettings);
		}
	}
	TestEqual(
		TEXT(
			"Prototype Experience registers "
			"IMC_UI_QuickAccessRadial exactly once"),
		RadialMappingRegistrationCount,
		1);

	TestEqual(
		TEXT("Radial slot keeps its canonical manual MVVM source name"),
		URpgQuickAccessRadialSlotWidget::
			ActionBarSlotViewModelSourceName,
		FName(TEXT("RpgActionBarSlotViewModel")));
	TestEqual(
		TEXT(
			"Action-bar slot ViewModel permits manual composition only"),
		URpgActionBarSlotViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));

	const UFunction* NativeSourceSetter =
		URpgQuickAccessRadialSlotWidget::StaticClass()
			->FindFunctionByName(
				GET_FUNCTION_NAME_CHECKED(
					URpgQuickAccessRadialSlotWidget,
					SetActionBarSlotViewModel));
	if (TestNotNull(
			TEXT(
				"Native radial slot exposes its canonical ViewModel "
				"injection API"),
			NativeSourceSetter))
	{
		const FObjectPropertyBase* SourceParameter =
			FindFProperty<FObjectPropertyBase>(
				NativeSourceSetter,
				TEXT("InSlotViewModel"));
		if (TestNotNull(
				TEXT(
					"Native radial slot ViewModel injection has its "
					"object parameter"),
				SourceParameter))
		{
			TestEqual(
				TEXT(
					"Native radial slot ViewModel injection expects "
					"URpgActionBarSlotViewModel"),
				SourceParameter->PropertyClass.Get(),
				URpgActionBarSlotViewModel::StaticClass());
		}
	}

	const UFunction* IconSourceSetter =
		URpgQuickAccessRadialSlotWidget::StaticClass()
			->FindFunctionByName(
				GET_FUNCTION_NAME_CHECKED(
					URpgQuickAccessRadialSlotWidget,
					SetIconSource));
	if (TestNotNull(
			TEXT(
				"Native radial slot exposes the MVVM icon-source "
				"setter"),
			IconSourceSetter))
	{
		TestTrue(
			TEXT("SetIconSource is reflected as a BlueprintSetter"),
			IconSourceSetter->HasMetaData(TEXT("BlueprintSetter")));

		int32 InputParameterCount = 0;
		for (TFieldIterator<FProperty> ParameterIt(IconSourceSetter);
			ParameterIt;
			++ParameterIt)
		{
			if (ParameterIt->HasAnyPropertyFlags(CPF_Parm) &&
				!ParameterIt->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				++InputParameterCount;
			}
		}
		TestEqual(
			TEXT("SetIconSource owns exactly one input parameter"),
			InputParameterCount,
			1);

		const FSoftObjectProperty* IconSourceParameter =
			FindFProperty<FSoftObjectProperty>(
				IconSourceSetter,
				TEXT("InIconSource"));
		if (TestNotNull(
				TEXT(
					"SetIconSource parameter is reflected as a "
					"soft-object property"),
				IconSourceParameter))
		{
			TestTrue(
				TEXT("SetIconSource soft-object property is a parameter"),
				IconSourceParameter->HasAnyPropertyFlags(CPF_Parm));
			TestFalse(
				TEXT("SetIconSource parameter is not a return value"),
				IconSourceParameter->HasAnyPropertyFlags(
					CPF_ReturnParm));
			TestEqual(
				TEXT(
					"SetIconSource parameter is exactly "
					"TSoftObjectPtr<UTexture2D>"),
				IconSourceParameter->PropertyClass.Get(),
				UTexture2D::StaticClass());
		}
	}

	const FName GeneratedManualSourceSetter(
		TEXT("SetRpgActionBarSlotViewModel"));
	TestNotNull(
		TEXT(
			"Authored radial slot exposes MVVM's generated manual-source "
			"setter"),
		RadialSlotGeneratedClass->FindFunctionByName(
			GeneratedManualSourceSetter));

	const UMVVMEditorSubsystem* MvvmEditor =
		GEditor
			? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()
			: nullptr;
	const UMVVMBlueprintView* BlueprintView =
		MvvmEditor
			? MvvmEditor->GetView(RadialSlotBlueprint)
			: nullptr;
	if (TestNotNull(
			TEXT("MVVM Editor subsystem is available"),
			MvvmEditor) &&
		TestNotNull(
			TEXT("Authored radial slot owns an MVVM Blueprint view"),
			BlueprintView))
	{
		const TArrayView<const FMVVMBlueprintViewModelContext>
			ViewModelSources = BlueprintView->GetViewModels();
		TestEqual(
			TEXT(
				"Authored radial slot owns exactly one MVVM "
				"ViewModel source"),
			ViewModelSources.Num(),
			1);

		FGuid CanonicalSourceId;
		if (ViewModelSources.Num() == 1)
		{
			const FMVVMBlueprintViewModelContext& Source =
				ViewModelSources[0];
			CanonicalSourceId = Source.GetViewModelId();
			TestEqual(
				TEXT("Radial slot MVVM source has its canonical name"),
				Source.GetViewModelName(),
				URpgQuickAccessRadialSlotWidget::
					ActionBarSlotViewModelSourceName);
			TestEqual(
				TEXT(
					"Radial slot MVVM source uses the exact "
					"URpgActionBarSlotViewModel type"),
				Source.GetViewModelClass(),
				URpgActionBarSlotViewModel::StaticClass());
			TestTrue(
				TEXT("Radial slot MVVM source is manually assigned"),
				Source.CreationType ==
					EMVVMBlueprintViewModelContextCreationType::
						Manual);
			TestTrue(
				TEXT("Radial slot MVVM source is optional"),
				Source.bOptional);
			TestTrue(
				TEXT(
					"Radial slot MVVM source exposes its generated "
					"manual setter"),
				Source.bCreateSetterFunction);
		}

		TestEqual(
			TEXT("Radial slot owns no MVVM events"),
			BlueprintView->GetEvents().Num(),
			0);
		TestEqual(
			TEXT("Radial slot owns no MVVM conditions"),
			BlueprintView->GetConditions().Num(),
			0);
		TestEqual(
			TEXT("Radial slot owns exactly three MVVM bindings"),
			BlueprintView->GetNumBindings(),
			3);

		const FMvvmBindingContract BindingContracts[] = {
			{
				TEXT("Icon"),
				NAME_None,
				URpgQuickAccessRadialSlotWidget::StaticClass()
					->FindFunctionByName(
					GET_FUNCTION_NAME_CHECKED(
						URpgQuickAccessRadialSlotWidget,
						SetIconSource))
			},
			{
				TEXT("ShortDisplayName"),
				TEXT("ItemNameText"),
				UTextBlock::StaticClass()->FindFunctionByName(
					GET_FUNCTION_NAME_CHECKED(
						UTextBlock,
						SetText))
			},
			{
				TEXT("StackCountText"),
				TEXT("StackCountText"),
				UTextBlock::StaticClass()->FindFunctionByName(
					GET_FUNCTION_NAME_CHECKED(
						UTextBlock,
						SetText))
			}
		};

		bool bBindingContractsResolve = true;
		for (const FMvvmBindingContract& Contract :
			BindingContracts)
		{
			const FString DestinationOwner =
				Contract.DestinationWidget.IsNone()
					? FString(TEXT("Self"))
					: Contract.DestinationWidget.ToString();
			bBindingContractsResolve &=
				TestNotNull(
					*FString::Printf(
						TEXT(
							"MVVM source field %s exists on "
							"URpgActionBarSlotViewModel"),
						*Contract.SourceField.ToString()),
					FindFProperty<FProperty>(
						URpgActionBarSlotViewModel::StaticClass(),
						Contract.SourceField));
			bBindingContractsResolve &=
				TestNotNull(
					*FString::Printf(
						TEXT(
							"MVVM destination setter for %s exists"),
						*DestinationOwner),
					Contract.DestinationSetter);
		}

		for (const FMVVMBlueprintViewBinding& Binding :
			BlueprintView->GetBindings())
		{
			TestTrue(
				TEXT(
					"Every radial-slot MVVM binding is "
					"OneWayToDestination"),
				Binding.BindingType ==
					EMVVMBindingMode::OneWayToDestination);
			TestTrue(
				TEXT("Every radial-slot MVVM binding is enabled"),
				Binding.bEnabled);
			TestTrue(
				TEXT("Every radial-slot MVVM binding is compiled"),
				Binding.bCompile);
			TestNull(
				TEXT(
					"Radial-slot MVVM bindings need no "
					"source-to-destination conversion"),
				Binding.Conversion.GetConversionFunction(
					/*bSourceToDestination=*/ true));
			TestNull(
				TEXT(
					"Radial-slot MVVM bindings need no "
					"destination-to-source conversion"),
				Binding.Conversion.GetConversionFunction(
					/*bSourceToDestination=*/ false));
		}

		if (bBindingContractsResolve &&
			ViewModelSources.Num() == 1)
		{
			for (const FMvvmBindingContract& Contract :
				BindingContracts)
			{
				const FString DestinationOwner =
					Contract.DestinationWidget.IsNone()
						? FString(TEXT("Self"))
						: Contract.DestinationWidget.ToString();
				int32 MatchingBindingCount = 0;
				for (const FMVVMBlueprintViewBinding& Binding :
					BlueprintView->GetBindings())
				{
					const bool bMatches =
						Binding.BindingType ==
							EMVVMBindingMode::
								OneWayToDestination
						&& Binding.bEnabled
						&& Binding.bCompile
						&& Binding.SourcePath.GetViewModelId() ==
							CanonicalSourceId
						&& (
							Contract.DestinationWidget.IsNone()
								? Binding.DestinationPath.GetSource(
									RadialSlotBlueprint) ==
										EMVVMBlueprintFieldPathSource::
											SelfContext
								: Binding.DestinationPath.GetSource(
									RadialSlotBlueprint) ==
										EMVVMBlueprintFieldPathSource::
											Widget
									&& Binding.DestinationPath
										.GetWidgetName() ==
											Contract.DestinationWidget)
						&& HasExactFieldPath(
							Binding.SourcePath,
							*RadialSlotBlueprint,
							Contract.SourceField)
						&& HasExactFieldPath(
							Binding.DestinationPath,
							*RadialSlotBlueprint,
							Contract.DestinationSetter->GetFName())
						&& !Binding.Conversion.GetConversionFunction(
							/*bSourceToDestination=*/ true)
						&& !Binding.Conversion.GetConversionFunction(
							/*bSourceToDestination=*/ false);
					MatchingBindingCount += bMatches ? 1 : 0;
				}

				TestEqual(
					*FString::Printf(
						TEXT(
							"Radial slot owns exactly one binding "
							"%s -> %s.%s"),
						*Contract.SourceField.ToString(),
						*DestinationOwner,
						*Contract.DestinationSetter->GetName()),
					MatchingBindingCount,
					1);
			}
		}
	}

	const TArray<UWidgetBlueprintGeneratedClassExtension*>
		MvvmExtensions =
			RadialSlotGeneratedClass->GetExtensions(
				UMVVMViewClass::StaticClass(),
				/*bIncludeSuper=*/ false);
	TestEqual(
		TEXT(
			"Authored radial slot owns exactly one compiled MVVM view"),
		MvvmExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		MvvmExtensions.Num() == 1
			? Cast<UMVVMViewClass>(MvvmExtensions[0])
			: nullptr;
	if (TestNotNull(
			TEXT("Authored radial slot compiled MVVM view is valid"),
			CompiledViewClass))
	{
		TestEqual(
			TEXT(
				"Compiled radial-slot MVVM view owns exactly three "
				"bindings"),
			CompiledViewClass->GetBindings().Num(),
			3);
		for (const FMVVMViewClass_Binding& Binding :
			CompiledViewClass->GetBindings())
		{
			TestTrue(
				TEXT(
					"Every compiled radial-slot MVVM binding is "
					"one-way"),
				Binding.IsOneWay());
		}

		int32 ViewModelSourceCount = 0;
		int32 CanonicalSourceCount = 0;
		const FMVVMViewClass_Source* CanonicalSource = nullptr;
		for (const FMVVMViewClass_Source& Source :
			CompiledViewClass->GetSources())
		{
			if (!Source.IsViewModel())
			{
				continue;
			}

			++ViewModelSourceCount;
			if (Source.GetName() ==
					URpgQuickAccessRadialSlotWidget::
						ActionBarSlotViewModelSourceName &&
				Source.GetSourceClass() ==
					URpgActionBarSlotViewModel::StaticClass())
			{
				++CanonicalSourceCount;
				CanonicalSource = &Source;
			}
		}
		TestEqual(
			TEXT(
				"Compiled radial slot owns exactly one ViewModel "
				"source"),
			ViewModelSourceCount,
			1);
		TestEqual(
			TEXT(
				"Compiled radial slot owns exactly one canonical "
				"action-bar slot source"),
			CanonicalSourceCount,
			1);
		if (CanonicalSource)
		{
			TestTrue(
				TEXT(
					"Compiled radial-slot ViewModel source can be "
					"set manually"),
				CanonicalSource->CanBeSet());
			TestTrue(
				TEXT(
					"Compiled radial-slot ViewModel source is "
					"optional"),
				CanonicalSource->IsOptional());
			TestEqual(
				TEXT(
					"Canonical radial-slot source owns all three "
					"compiled bindings"),
				CanonicalSource->GetBindings().Num(),
				3);
		}
	}

	const FStructProperty* CancelActionProperty =
		FindFProperty<FStructProperty>(
			URpgQuickAccessRadialWidget::StaticClass(),
			TEXT("CommonUiCancelAction"));
	if (TestNotNull(
			TEXT("CommonUiCancelAction property exists"),
			CancelActionProperty))
	{
		TestEqual(
			TEXT("CommonUiCancelAction remains a DataTable row handle"),
			CancelActionProperty->Struct.Get(),
			FDataTableRowHandle::StaticStruct());
		const FDataTableRowHandle* CancelAction =
			CancelActionProperty
				->ContainerPtrToValuePtr<FDataTableRowHandle>(
					RadialDefaults);
		if (TestNotNull(
				TEXT("CommonUiCancelAction defaults are readable"),
				CancelAction))
		{
			TestEqual(
				TEXT("CommonUiCancelAction uses the UI.Back row"),
				CancelAction->RowName,
				FName(TEXT("UI.Back")));
			if (TestNotNull(
				TEXT("CommonUiCancelAction owns an action table"),
				CancelAction->DataTable.Get()))
			{
				TestEqual(
					TEXT(
						"CommonUiCancelAction uses "
						"CDT_RpgUIActions_All"),
					CancelAction->DataTable->GetPathName(),
					FString(CommonUiActionTablePath));
			}
		}
	}

	return true;
}

#endif
