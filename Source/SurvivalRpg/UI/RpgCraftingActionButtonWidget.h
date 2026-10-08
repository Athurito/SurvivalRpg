#pragma once

#include "CommonButtonBase.h"

#include "RpgCraftingActionButtonWidget.generated.h"

class UCommonTextBlock;

/**
 * Graph-free authored button used by the crafting screen and its job rows.
 *
 * CommonUI owns input/focus/pressed state. The native screen supplies semantic actions and enabled state; this leaf
 * only renders its label and the style authored by its Widget Blueprint.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgCraftingActionButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Updates the authored label without introducing a Blueprint setter graph. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Presentation")
	void SetCraftButtonText(FText InText);

	/** Current presentation-only label. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Presentation")
	FText GetCraftButtonText() const { return CraftButtonText; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnCurrentTextStyleChanged() override;

	/** Required authored label receiving the button's presentation text. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text = nullptr;

	/**
	 * When true, Text takes the normal, hovered, selected and disabled text styles of the button style, so a disabled
	 * button also reads as disabled. Cosmetic; turn off when the Widget Blueprint styles the label itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Presentation")
	bool bLabelFollowsButtonTextStyle = true;

private:
	UPROPERTY(Transient)
	FText CraftButtonText;
};
