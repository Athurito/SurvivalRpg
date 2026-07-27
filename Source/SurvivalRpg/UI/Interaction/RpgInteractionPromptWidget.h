// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Interaction/InteractionTypes.h"
#include "SurvivalRpg/UI/IndicatorSystem/IActorIndicatorWidget.h"

#include "RpgInteractionPromptWidget.generated.h"

class ARpgBasePlayerState;
class UCommonActionWidget;
class UCommonLazyImage;
class UCommonTextBlock;
class UIndicatorDescriptor;
class UInputAction;
class UTexture2D;
class URpgInteractionPromptData;
class URpgPawnData;

/**
 * CommonUI presentation base for focused prompts and compact nearby indicators.
 *
 * The widget observes a stable URpgInteractionPromptData object and resolves the Interact icon from
 * PawnData's semantic input config. CommonUI owns input-method and remapping refresh behavior.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInteractionPromptWidget : public UCommonUserWidget, public IIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	/** Binds a local presentation model; passing null releases the previous event subscription. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction|Prompt")
	void SetPromptData(URpgInteractionPromptData* InPromptData);

	/** Returns the stable presentation model currently represented by this pooled widget. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Interaction|Prompt")
	URpgInteractionPromptData* GetPromptData() const { return PromptData; }

	//~ IIndicatorWidgetInterface
	virtual void BindIndicator_Implementation(UIndicatorDescriptor* Indicator) override;
	virtual void UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator) override;
	//~ End IIndicatorWidgetInterface

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Reapplies optional native text, icon, and action widgets from the current presentation model. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction|Prompt")
	void RefreshPromptPresentation();

	/** Blueprint styling hook invoked after native fields have been synchronized. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Interaction|Prompt", meta = (DisplayName = "On Prompt Presentation Changed"))
	void BP_OnPromptPresentationChanged(ERpgInteractionPromptState NewState);

	/** Optional CommonUI glyph widget. It is configured from IA_Interact and refreshes on input/rebind events. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Interaction|Prompt")
	TObjectPtr<UCommonActionWidget> InputActionWidget;

	/** Optional lazily loaded interaction-specific icon. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Interaction|Prompt")
	TObjectPtr<UCommonLazyImage> PromptIcon;

	/** Optional localized action verb text block. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Interaction|Prompt")
	TObjectPtr<UCommonTextBlock> ActionTextBlock;

	/** Optional localized target-name text block. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Interaction|Prompt")
	TObjectPtr<UCommonTextBlock> TargetTextBlock;

	/** Optional localized blocked-reason text block. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Interaction|Prompt")
	TObjectPtr<UCommonTextBlock> BlockedReasonTextBlock;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgInteractionPromptInputResolutionTest;
#endif

	UFUNCTION()
	void HandlePromptDataChanged(URpgInteractionPromptData* ChangedPromptData);

	void RefreshPawnDataBinding();
	void ReleasePawnDataBinding();
	void HandlePawnDataChanged(const URpgPawnData* NewPawnData);
	void ApplyInputActionFromPawnData(const URpgPawnData* PawnData);
	void BuildFallbackWidgetTree();
	static const UInputAction* ResolveInteractionInputAction(const URpgPawnData* PawnData);

	UPROPERTY(Transient)
	TObjectPtr<URpgInteractionPromptData> PromptData;

	UPROPERTY(Transient)
	TObjectPtr<UIndicatorDescriptor> BoundIndicator;

	TWeakObjectPtr<ARpgBasePlayerState> BoundPlayerState;
	TSoftObjectPtr<UTexture2D> AppliedPromptIcon;
};
