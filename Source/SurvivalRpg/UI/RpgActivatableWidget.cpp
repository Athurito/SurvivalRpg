// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgActivatableWidget.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#define LOCTEXT_NAMESPACE "RpgActivatableWidget"

URpgActivatableWidget::URpgActivatableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
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
