#pragma once

#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

#include "RpgInventoryFeedbackToastWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Small owner-local inventory result toast with a functional native widget-tree fallback.
 *
 * The server result remains authoritative. This widget only translates reliable action feedback into a short
 * success/error message; Blueprint subclasses may bind the optional controls for project-specific styling.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryFeedbackToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryFeedbackToastWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Displays one request-correlated server result and restarts the auto-hide timer. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Feedback")
	void ShowInventoryActionFeedback(const FRpgInventoryActionFeedbackMessage& Message);

	/** Immediately hides the current feedback without affecting inventory state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Feedback")
	void HideInventoryActionFeedback();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** Optional styled background. Native fallback creates it when either required binding is absent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Feedback")
	TObjectPtr<UBorder> FeedbackBorder = nullptr;

	/** Optional message label. Native fallback creates it when either required binding is absent. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Feedback")
	TObjectPtr<UTextBlock> FeedbackText = nullptr;

	/** Seconds a completed action result remains visible. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Feedback", meta = (ClampMin = "0.25", UIMin = "0.25"))
	float DisplayDuration = 2.0f;

	/** Background tint for successful requests. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Feedback|Style")
	FLinearColor SuccessColor = FLinearColor(0.04f, 0.24f, 0.12f, 0.96f);

	/** Background tint for rejected or blocked requests. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Feedback|Style")
	FLinearColor FailureColor = FLinearColor(0.38f, 0.045f, 0.035f, 0.96f);

	/** Optional presentation hook for sound, pulse, or project-specific localized copy. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Feedback", meta = (DisplayName = "On Inventory Action Feedback Shown"))
	void BP_OnInventoryActionFeedbackShown(const FRpgInventoryActionFeedbackMessage& Message);

private:
	void EnsureFeedbackWidgetTree();
	void BuildNativeFeedbackWidgetTree();
	static FText BuildFeedbackText(const FRpgInventoryActionFeedbackMessage& Message);

	FTimerHandle HideTimerHandle;
};
