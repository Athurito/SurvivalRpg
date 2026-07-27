#include "RpgGameFeatureAction_AddWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "NativeGameplayTags.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgActivatableWidget.h"
#include "SurvivalRpg/UI/RpgWidgetClassValidation.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_RpgAddWidgets_UnregisteredLayer,
	"UI.Layer.Automation.UnregisteredGameFeatureLayer");

namespace
{
	bool HasIssueContaining(
		const FDataValidationContext& Context,
		const TCHAR* ExpectedText)
	{
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Message.ToString().Contains(
				ExpectedText))
			{
				return true;
			}
		}

		return false;
	}

	FRpgGameFeatureWidgetLayoutEntry MakeLayoutEntry(
		const FGameplayTag LayerTag)
	{
		FRpgGameFeatureWidgetLayoutEntry Entry;
		Entry.LayoutClass =
			UCommonActivatableWidget::StaticClass();
		Entry.LayerTag = LayerTag;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGameFeatureAddWidgetsLayerValidationTest,
	"SurvivalRpg.GameFeatures.AddWidgets.Validation.LayerContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGameFeatureAddWidgetsLayerValidationTest::RunTest(
	const FString& Parameters)
{
	URpgGameFeatureAction_AddWidgets* Action =
		NewObject<URpgGameFeatureAction_AddWidgets>(
			GetTransientPackage());
	if (!TestNotNull(
		TEXT("Transient AddWidgets action exists"),
		Action))
	{
		return false;
	}

	Action->Layouts = {
		MakeLayoutEntry(RpgGameplayTags::UI_Layer_Game),
		MakeLayoutEntry(RpgGameplayTags::UI_Layer_GameMenu),
		MakeLayoutEntry(RpgGameplayTags::UI_Layer_Menu),
		MakeLayoutEntry(RpgGameplayTags::UI_Layer_Modal)
	};

	FDataValidationContext ValidContext;
	TestEqual(
		TEXT("All four root-layout tags validate for GameFeature layouts"),
		Action->IsDataValid(ValidContext),
		EDataValidationResult::Valid);
	TestEqual(
		TEXT("Registered GameFeature layout tags emit no errors"),
		ValidContext.GetNumErrors(),
		0u);

	Action->Layouts[0].LayerTag =
		TAG_RpgAddWidgets_UnregisteredLayer;
	FDataValidationContext UnsupportedContext;
	TestEqual(
		TEXT("An unregistered UI.Layer descendant invalidates the action"),
		Action->IsDataValid(UnsupportedContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT("The unsupported GameFeature layer receives one focused error"),
		UnsupportedContext.GetNumErrors(),
		1u);
	TestTrue(
		TEXT("The GameFeature diagnostic names root-layout registration"),
		HasIssueContaining(
			UnsupportedContext,
			TEXT("not registered by URpgPrimaryGameLayout")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGameFeatureAddWidgetsClassValidationTest,
	"SurvivalRpg.GameFeatures.AddWidgets.Validation.Classes",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGameFeatureAddWidgetsClassValidationTest::RunTest(
	const FString& Parameters)
{
	bool bDeferredClassValidation = false;
	const UClass* ExactLoadedClass =
		RpgWidgetClassValidation::
			ResolveAuthoredClassWithoutLoading(
				FSoftObjectPath(
					TEXT(
						"/Script/CommonUI."
						"CommonActivatableWidget")),
				UCommonActivatableWidget::StaticClass(),
				bDeferredClassValidation);
	TestTrue(
		TEXT("An exact stable loaded class path resolves"),
		ExactLoadedClass ==
			UCommonActivatableWidget::StaticClass());
	TestFalse(
		TEXT("An exact stable loaded class is not deferred"),
		bDeferredClassValidation);

	bDeferredClassValidation = false;
	const UClass* MismatchedLoadedClass =
		RpgWidgetClassValidation::
			ResolveAuthoredClassWithoutLoading(
				FSoftObjectPath(
					TEXT("/Script/UMG.UserWidget")),
				UCommonActivatableWidget::StaticClass(),
				bDeferredClassValidation);
	TestNull(
		TEXT(
			"A stable loaded class cannot satisfy a different "
			"authored path"),
		MismatchedLoadedClass);
	TestFalse(
		TEXT("A mismatched loaded class is not deferred"),
		bDeferredClassValidation);

	bDeferredClassValidation = false;
	const UClass* SubobjectClass =
		RpgWidgetClassValidation::
			ResolveAuthoredClassWithoutLoading(
				FSoftObjectPath(
					TEXT(
						"/Game/SurvivalRpg/UI/"
						"CUI_StorageSpatial."
						"CUI_StorageSpatial_C:Bogus")),
				nullptr,
				bDeferredClassValidation);
	TestNull(
		TEXT("A generated-class subobject path does not resolve"),
		SubobjectClass);
	TestFalse(
		TEXT("A generated-class subobject path is not deferred"),
		bDeferredClassValidation);

	URpgGameFeatureAction_AddWidgets* Action =
		NewObject<URpgGameFeatureAction_AddWidgets>(
			GetTransientPackage());
	if (!TestNotNull(
		TEXT("Transient AddWidgets action exists"),
		Action))
	{
		return false;
	}

	FRpgGameFeatureWidgetLayoutEntry MissingLayout =
		MakeLayoutEntry(RpgGameplayTags::UI_Layer_Game);
	MissingLayout.LayoutClass =
		TSoftClassPtr<UCommonActivatableWidget>(
			FSoftObjectPath(
				TEXT(
					"/Game/Automation/MissingLayout."
					"MissingLayout_C")));
	Action->Layouts.Add(MissingLayout);

	FRpgGameFeatureWidgetEntry MissingWidget;
	MissingWidget.WidgetClass =
		TSoftClassPtr<UUserWidget>(
			FSoftObjectPath(
				TEXT(
					"/Game/Automation/MissingWidget."
					"MissingWidget_C")));
	MissingWidget.SlotTag =
		RpgGameplayTags::UI_HUD_Slot_ActionBar;
	Action->Widgets.Add(MissingWidget);

	FDataValidationContext MissingContext;
	TestEqual(
		TEXT("Unresolved GameFeature widget classes invalidate the action"),
		Action->IsDataValid(MissingContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT("Each unresolved class receives one focused error"),
		MissingContext.GetNumErrors(),
		2u);
	TestTrue(
		TEXT("The layout-class diagnostic identifies failed resolution"),
		HasIssueContaining(
			MissingContext,
			TEXT("MissingLayout_C' could not be loaded")));
	TestTrue(
		TEXT("The slot-widget diagnostic identifies failed resolution"),
		HasIssueContaining(
			MissingContext,
			TEXT("MissingWidget_C' could not be loaded")));

	Action->Layouts[0].LayoutClass =
		TSoftClassPtr<UCommonActivatableWidget>(
			FSoftObjectPath(
				TEXT(
					"/Game/SurvivalRpg/UI/CUI_StorageSpatial."
					"StaleLayoutClass_C")));
	Action->Widgets.Reset();
	FDataValidationContext StaleObjectContext;
	TestEqual(
		TEXT(
			"A stale generated-class name in an existing package "
			"invalidates the action"),
		Action->IsDataValid(StaleObjectContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT(
			"The stale generated-class name receives one "
			"focused error"),
		StaleObjectContext.GetNumErrors(),
		1u);
	TestTrue(
		TEXT(
			"The stale-object diagnostic preserves the exact "
			"authored class path"),
		HasIssueContaining(
			StaleObjectContext,
			TEXT("StaleLayoutClass_C' could not be loaded")));

	Action->Layouts[0].LayoutClass =
		URpgActivatableWidget::StaticClass();
	Action->Widgets.Reset();
	FDataValidationContext AbstractContext;
	TestEqual(
		TEXT("An abstract layout class invalidates the action"),
		Action->IsDataValid(AbstractContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT("The abstract layout receives one focused error"),
		AbstractContext.GetNumErrors(),
		1u);
	TestTrue(
		TEXT("The layout diagnostic identifies the abstract class"),
		HasIssueContaining(
			AbstractContext,
			TEXT("is abstract")));

	Action->Layouts.Reset();
	FRpgGameFeatureWidgetEntry WrongSlotEntry;
	WrongSlotEntry.WidgetClass =
		UCommonActivatableWidget::StaticClass();
	WrongSlotEntry.SlotTag =
		RpgGameplayTags::UI_Screen_Inventory;
	Action->Widgets.Add(WrongSlotEntry);
	FDataValidationContext WrongSlotContext;
	TestEqual(
		TEXT(
			"A slot tag outside UI.HUD.Slot invalidates the "
			"action"),
		Action->IsDataValid(WrongSlotContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT("The wrong slot namespace receives one focused error"),
		WrongSlotContext.GetNumErrors(),
		1u);
	TestTrue(
		TEXT("The slot diagnostic identifies UI.HUD.Slot"),
		HasIssueContaining(
			WrongSlotContext,
			TEXT("strict descendant of UI.HUD.Slot")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGameFeatureAddWidgetsAuthoredClassesTest,
	"SurvivalRpg.GameFeatures.AddWidgets.Validation.AuthoredClasses",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGameFeatureAddWidgetsAuthoredClassesTest::RunTest(
	const FString& Parameters)
{
	const UBlueprint* ExperienceBlueprint =
		LoadObject<UBlueprint>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/System/Experiences/"
				"RpgPrototypeExperience.RpgPrototypeExperience"));
	if (!TestNotNull(
		TEXT("RpgPrototypeExperience Blueprint loads"),
		ExperienceBlueprint) ||
		!TestNotNull(
			TEXT("RpgPrototypeExperience generated class exists"),
			ExperienceBlueprint
				? ExperienceBlueprint->GeneratedClass.Get()
				: nullptr))
	{
		return false;
	}

	const URpgExperienceDefinition* Experience =
		Cast<URpgExperienceDefinition>(
			ExperienceBlueprint->GeneratedClass->
				GetDefaultObject());
	if (!TestNotNull(
		TEXT("RpgPrototypeExperience defaults load"),
		Experience))
	{
		return false;
	}

	int32 AddWidgetsActionCount = 0;
	for (const UGameFeatureAction* FeatureAction :
		Experience->Actions)
	{
		const URpgGameFeatureAction_AddWidgets* AddWidgetsAction =
			Cast<URpgGameFeatureAction_AddWidgets>(
				FeatureAction);
		if (!AddWidgetsAction)
		{
			continue;
		}

		++AddWidgetsActionCount;
		for (int32 LayoutIndex = 0;
			LayoutIndex < AddWidgetsAction->Layouts.Num();
			++LayoutIndex)
		{
			const UClass* LayoutClass =
				AddWidgetsAction->Layouts[LayoutIndex].
					LayoutClass.LoadSynchronous();
			if (TestNotNull(
				*FString::Printf(
					TEXT(
						"Layouts[%d] authored class loads"),
					LayoutIndex),
				LayoutClass))
			{
				TestTrue(
					*FString::Printf(
						TEXT(
							"Layouts[%d] derives from "
							"CommonActivatableWidget"),
						LayoutIndex),
					LayoutClass->IsChildOf(
						UCommonActivatableWidget::
							StaticClass()));
				TestFalse(
					*FString::Printf(
						TEXT(
							"Layouts[%d] authored class is "
							"concrete"),
						LayoutIndex),
					LayoutClass->HasAnyClassFlags(
						CLASS_Abstract));
			}
		}

		for (int32 WidgetIndex = 0;
			WidgetIndex < AddWidgetsAction->Widgets.Num();
			++WidgetIndex)
		{
			const UClass* WidgetClass =
				AddWidgetsAction->Widgets[WidgetIndex].
					WidgetClass.LoadSynchronous();
			if (TestNotNull(
				*FString::Printf(
					TEXT(
						"Widgets[%d] authored class loads"),
					WidgetIndex),
				WidgetClass))
			{
				TestTrue(
					*FString::Printf(
						TEXT(
							"Widgets[%d] derives from "
							"UserWidget"),
						WidgetIndex),
					WidgetClass->IsChildOf(
						UUserWidget::StaticClass()));
				TestFalse(
					*FString::Printf(
						TEXT(
							"Widgets[%d] authored class is "
							"concrete"),
						WidgetIndex),
					WidgetClass->HasAnyClassFlags(
						CLASS_Abstract));
			}
		}

		FDataValidationContext ValidationContext;
		TestEqual(
			TEXT(
				"Loaded authored AddWidgets defaults validate"),
			AddWidgetsAction->IsDataValid(
				ValidationContext),
			EDataValidationResult::Valid);
		TestEqual(
			TEXT(
				"Loaded authored AddWidgets defaults emit no "
				"errors"),
			ValidationContext.GetNumErrors(),
			0u);
	}

	TestTrue(
		TEXT(
			"RpgPrototypeExperience contains an Add Rpg "
			"Widgets action"),
		AddWidgetsActionCount > 0);
	return true;
}

#endif
