#pragma once

#include "CoreMinimal.h"
#include "RpgActivatableWidget.h"

#include "RpgFrontendWidgets.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UWidgetSwitcher;

/**
 * Project base for full-screen frontend widgets hosted by CommonGame.
 *
 * Frontend screens consistently request menu input; viewport ownership and
 * input-mode changes remain with CommonUI rather than individual Blueprints.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgFrontendScreenWidget : public URpgActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgFrontendScreenWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * Native presenter for the authored Boot splash pages.
 *
 * CUI_BootMenu owns only its visuals. This presenter advances the switcher and
 * performs the configured map travel, so activation has one explicit lifecycle.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgBootScreenWidget : public URpgFrontendScreenWidget
{
	GENERATED_BODY()

public:
	explicit URpgBootScreenWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the immutable designer-authored splash timings used by this instance. */
	const TArray<float>& GetPageDisplayDurations() const { return PageDisplayDurations; }

	/** Returns the frontend map opened after the final splash page. */
	const TSoftObjectPtr<UWorld>& GetDestinationMap() const { return DestinationMap; }

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/**
	 * Seconds each authored switcher page remains visible before advancing.
	 * Static designer configuration; the entry count must match WidgetSwitcher.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Frontend|Boot",
		meta = (ClampMin = "0.01"))
	TArray<float> PageDisplayDurations;

	/** Frontend map opened after the final splash page. Static designer configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Frontend|Boot")
	TSoftObjectPtr<UWorld> DestinationMap;

	/** Designer-authored Boot splash pages advanced by this presenter at runtime. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;

private:
	void ScheduleCurrentPageAdvance();
	void AdvanceBootSequence();
	void ClearBootSequenceTimer();

	FTimerHandle BootSequenceTimerHandle;
	int32 CurrentPageIndex = INDEX_NONE;
	bool bTravelRequested = false;
};

/**
 * Canonical Main Menu composition root.
 *
 * The five legacy visual stacks remain designer-authored in CUI_MainMenuStack,
 * while this native presenter owns their navigation API and initial screen.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgMainMenuStackWidget : public URpgFrontendScreenWidget
{
	GENERATED_BODY()

public:
	/** Pushes a page onto the primary Main Menu navigation stack. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu")
	UCommonActivatableWidget* PushToMainStack(TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	/** Pushes a page onto the general options stack. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu")
	UCommonActivatableWidget* PushToOptionStack(TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	/** Pushes a blocking Main Menu popup. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu")
	UCommonActivatableWidget* PushToPopupStack(TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	/** Pushes a page onto the first nested options stack. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu")
	UCommonActivatableWidget* PushToOption1Stack(TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

	/** Pushes a page onto the second nested options stack. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Frontend|Main Menu")
	UCommonActivatableWidget* PushToOption2Stack(TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass);

protected:
	virtual void NativeOnActivated() override;

	/** First page pushed into MenuStack when the Main Menu root is activated and empty. Static designer configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Frontend|Main Menu")
	TSubclassOf<UCommonActivatableWidget> InitialMenuClass;

	/** Primary page history authored by CUI_MainMenuStack. Runtime-owned by this presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	/** Settings, credits, load, and other secondary pages. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> OptionStack;

	/** First nested loading/options page history retained for existing menu flows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> OptionStack_1;

	/** Second nested loading/options page history retained for existing menu flows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> OptionStack_2;

	/** Modal popup history, including the desktop-exit confirmation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> PopupStack;
};
