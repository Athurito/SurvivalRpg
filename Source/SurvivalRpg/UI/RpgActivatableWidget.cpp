// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgActivatableWidget.h"

#include "CommonInputSettings.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "ICommonInputModule.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#define LOCTEXT_NAMESPACE "RpgActivatableWidget"

URpgActivatableWidget::URpgActivatableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SelectActionDisplayName = LOCTEXT("SelectActionDisplayName", "Select");
}

FBindUIActionArgs URpgActivatableWidget::MakeSelectActionBindArgs(const FText& DisplayName, const FSimpleDelegate& OnExecuteAction)
{
	const UCommonInputSettings& InputSettings = ICommonInputModule::GetSettings();
	const UInputAction* EnhancedClickAction =
		UCommonInputSettings::IsEnhancedInputSupportEnabled() ? InputSettings.GetEnhancedInputClickAction() : nullptr;

	FBindUIActionArgs BindArgs = EnhancedClickAction
		? FBindUIActionArgs(EnhancedClickAction, OnExecuteAction)
		: FBindUIActionArgs(InputSettings.GetDefaultClickAction(), OnExecuteAction);
	BindArgs.bConsumeInput = false;
	BindArgs.bDisplayInActionBar = false;
	BindArgs.OverrideDisplayName = DisplayName;
	return BindArgs;
}

bool URpgActivatableWidget::ShouldDisplaySelectActionFor(ECommonInputType InputType)
{
	return InputType == ECommonInputType::Gamepad;
}

void URpgActivatableWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!bIsSelectActionDisplayedInActionBar || SelectActionHandle.IsValid())
	{
		return;
	}

	// The click itself stays with Slate's accept on the focused button; this binding only feeds the action bar.
	SelectActionHandle = RegisterUIActionBinding(
		MakeSelectActionBindArgs(SelectActionDisplayName, FSimpleDelegate::CreateWeakLambda(this, [] {})));

	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		InputMethodChangedHandle =
			InputSubsystem->OnInputMethodChangedNative.AddUObject(this, &URpgActivatableWidget::HandleInputMethodChanged);
		HandleInputMethodChanged(InputSubsystem->GetCurrentInputType());
	}
}

void URpgActivatableWidget::NativeDestruct()
{
	if (InputMethodChangedHandle.IsValid())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
		{
			InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
		}
		InputMethodChangedHandle.Reset();
	}

	Super::NativeDestruct();

	if (SelectActionHandle.IsValid())
	{
		SelectActionHandle.Unregister();
	}
}

void URpgActivatableWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	// Changing the flag refreshes every bound action bar on its own.
	SelectActionHandle.SetDisplayInActionBar(ShouldDisplaySelectActionFor(InputType));
}

TOptional<FUIInputConfig> URpgActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case ERpgWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ERpgWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ERpgWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ERpgWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

#if WITH_EDITOR

void URpgActivatableWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	if (!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(URpgActivatableWidget, BP_GetDesiredFocusTarget)))
	{
		if (GetParentNativeClass(GetClass()) == URpgActivatableWidget::StaticClass())
		{
			CompileLog.Warning(LOCTEXT("ValidateGetDesiredFocusTarget_Warning", "GetDesiredFocusTarget wasn't implemented, gamepad focus may not work correctly on this screen."));
		}
		else
		{
			CompileLog.Note(LOCTEXT("ValidateGetDesiredFocusTarget_Note", "GetDesiredFocusTarget wasn't implemented. Ignore this if a native parent class supplies the focus target."));
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE
