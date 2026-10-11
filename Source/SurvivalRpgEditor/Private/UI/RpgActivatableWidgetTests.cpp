#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/UI/RpgActivatableWidget.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSettings.h"
#include "CommonInputTypeEnum.h"
#include "CommonUITypes.h"
#include "ICommonInputModule.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActivatableWidgetSelectActionHintTest,
	"SurvivalRpg.UI.Input.SelectActionHint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActivatableWidgetSelectActionHintTest::RunTest(const FString& Parameters)
{
	// A running game loads the CommonUI input data when its first local player starts; the test loads it itself.
	UCommonInputSettings& InputSettings = ICommonInputModule::GetSettings();
	InputSettings.LoadData();

	const FText Label = FText::FromString(TEXT("Test Label"));
	const FBindUIActionArgs Args =
		URpgActivatableWidget::MakeSelectActionBindArgs(Label, FSimpleDelegate::CreateLambda([] {}));

	TestFalse(TEXT("The select hint never consumes the press, so the focused button still receives it"), Args.bConsumeInput);
	TestFalse(TEXT("The select hint starts hidden until a gamepad is active"), Args.bDisplayInActionBar);
	TestFalse(TEXT("The select hint is bound like any screen action, not persistently"), Args.bIsPersistent);
	TestTrue(TEXT("The select hint carries the screen's label"), Args.OverrideDisplayName.EqualTo(Label));
	TestTrue(TEXT("The select hint has a handler, as the action router requires"), Args.OnExecuteAction.IsBound());

	const UInputAction* EnhancedClickAction =
		UCommonInputSettings::IsEnhancedInputSupportEnabled() ? InputSettings.GetEnhancedInputClickAction() : nullptr;
	if (EnhancedClickAction)
	{
		TestTrue(TEXT("The select hint shows the platform's enhanced click action"), Args.InputAction.Get() == EnhancedClickAction);
	}
	else
	{
		const FDataTableRowHandle ClickAction = InputSettings.GetDefaultClickAction();
		TestTrue(TEXT("The select hint shows the platform's click action"), Args.LegacyActionTableRow == ClickAction);

		const FCommonInputActionDataBase* ClickRow =
			ClickAction.GetRow<FCommonInputActionDataBase>(TEXT("SurvivalRpg.UI.Input.SelectActionHint"));
		if (TestNotNull(TEXT("The click action names an existing UI action row"), ClickRow))
		{
			TestTrue(TEXT("The click action has a gamepad key, so the action bar can show its glyph"),
				ClickRow->GetInputTypeInfo(ECommonInputType::Gamepad, FCommonInputDefaults::GamepadGeneric).GetKey().IsValid());
		}
	}

	TestTrue(TEXT("A gamepad shows the select hint"), URpgActivatableWidget::ShouldDisplaySelectActionFor(ECommonInputType::Gamepad));
	TestFalse(TEXT("Mouse and keyboard never show the select hint"),
		URpgActivatableWidget::ShouldDisplaySelectActionFor(ECommonInputType::MouseAndKeyboard));
	TestFalse(TEXT("Touch never shows the select hint"), URpgActivatableWidget::ShouldDisplaySelectActionFor(ECommonInputType::Touch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActivatableWidgetSharedMappingContextTest,
	"SurvivalRpg.UI.Input.SharedMappingContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActivatableWidgetSharedMappingContextTest::RunTest(const FString& Parameters)
{
	const FObjectProperty* InputMappingProperty =
		FindFProperty<FObjectProperty>(UCommonActivatableWidget::StaticClass(), TEXT("InputMapping"));
	if (!TestNotNull(TEXT("CommonUI screens expose their input mapping context as a property"), InputMappingProperty))
	{
		return false;
	}

	UInputMappingContext* Shared = NewObject<UInputMappingContext>(GetTransientPackage());
	UInputMappingContext* Other = NewObject<UInputMappingContext>(GetTransientPackage());
	auto MakeScreen = [&](UInputMappingContext* Mapping)
	{
		UCommonActivatableWidget* Screen = NewObject<UCommonActivatableWidget>(GetTransientPackage());
		InputMappingProperty->SetObjectPropertyValue_InContainer(Screen, Mapping);
		return Screen;
	};

	// Two stacked screens share the generic UI context; a third uses its own.
	UCommonActivatableWidget* Below = MakeScreen(Shared);
	UCommonActivatableWidget* Above = MakeScreen(Shared);
	UCommonActivatableWidget* Unrelated = MakeScreen(Other);
	Below->ActivateWidget();
	Above->ActivateWidget();
	Unrelated->ActivateWidget();

	TestTrue(TEXT("A screen still open below keeps the shared context when the upper one leaves"),
		URpgActivatableWidget::IsMappingContextUsedByOtherActiveScreen(*Above, Shared));
	TestFalse(TEXT("A screen without a context has nothing to keep"),
		URpgActivatableWidget::IsMappingContextUsedByOtherActiveScreen(*Above, nullptr));

	Below->DeactivateWidget();
	TestFalse(TEXT("The last screen using a context removes it"),
		URpgActivatableWidget::IsMappingContextUsedByOtherActiveScreen(*Above, Shared));
	TestTrue(TEXT("Another active screen keeps its own context"),
		URpgActivatableWidget::IsMappingContextUsedByOtherActiveScreen(*Above, Other));

	for (UCommonActivatableWidget* Screen : {Below, Above, Unrelated})
	{
		Screen->DeactivateWidget();
		Screen->MarkAsGarbage();
	}
	return true;
}

#endif
