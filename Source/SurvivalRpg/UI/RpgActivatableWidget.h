// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIInputTypes.h"
#include "Input/UIActionBindingHandle.h"

#include "RpgActivatableWidget.generated.h"

enum class ECommonInputType : uint8;
struct FUIInputConfig;
class UInputMappingContext;
class UWidgetTree;

UENUM(BlueprintType)
enum class ERpgWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	explicit URpgActivatableWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/**
	 * Builds the binding behind the select hint: the platform click action under the given label. It never
	 * consumes input, so the focused button still receives the press, and it starts hidden from the action bar.
	 */
	static FBindUIActionArgs MakeSelectActionBindArgs(const FText& DisplayName, const FSimpleDelegate& OnExecuteAction);

	/** Whether the select hint belongs in the action bar for an input type. Only gamepads show it. */
	static bool ShouldDisplaySelectActionFor(ECommonInputType InputType);

	/**
	 * Whether an activated screen other than Leaving, owned by the same local player, still applies Mapping as its
	 * input mapping context. CommonUI removes a screen's context outright on deactivation, so stacked screens that
	 * share one context would otherwise strip it from the screens still open below.
	 */
	static bool IsMappingContextUsedByOtherActiveScreen(const UCommonActivatableWidget& Leaving, const UInputMappingContext* Mapping);

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void DeactivateMappingContext() override;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	ERpgWidgetInputMode InputConfig = ERpgWidgetInputMode::Default;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/**
	 * Shows the click action as a select hint ("A Select") in the action bar while a gamepad is active.
	 * Menu buttons show no click glyph of their own; this hint names the confirm button once for the screen.
	 * Display only: the press still goes to the focused button. Mouse, keyboard and touch never show it.
	 * Static designer configuration.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	bool bIsSelectActionDisplayedInActionBar = false;

	/** Action bar label of the select hint. Static designer configuration. */
	UPROPERTY(EditDefaultsOnly, Category = Input, meta = (EditCondition = "bIsSelectActionDisplayedInActionBar"))
	FText SelectActionDisplayName;

private:
	void HandleInputMethodChanged(ECommonInputType InputType);

	FUIActionBindingHandle SelectActionHandle;
	FDelegateHandle InputMethodChangedHandle;
};
