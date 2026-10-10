#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContextActionTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryContextActionEntryWidget.generated.h"

class UCommonLazyImage;
class UTextBlock;
class UTexture2D;
class UWidget;
class URpgInventoryContextMenuWidget;

/**
 * Designer-owned CommonUI row for one semantic inventory context action.
 *
 * The canonical Blueprint must author Text_ActionLabel and may style the CommonButton normally. The
 * native class forwards clicks to the owning context menu without per-action delegates. Optional widgets show the
 * action's icon and set destructive actions such as Drop apart.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryContextActionEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContextActionEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Configures this recycled row for one action. Called by URpgInventoryContextMenuWidget. */
	void InitializeContextAction(
		URpgInventoryContextMenuWidget* InOwningMenu,
		ERpgInventoryContextAction InAction,
		const FText& InLabel);

	/** Semantic command represented by this row. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	ERpgInventoryContextAction GetContextAction() const { return ContextAction; }

	/** Localized label resolved by the menu, including Bind/Change/Unbind context. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FText GetActionLabel() const { return ActionLabel; }

	/** True for actions listed in DestructiveActions, such as Drop. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	bool IsDestructiveAction() const { return DestructiveActions.Contains(ContextAction); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnClicked() override;

	/** Required localized label owned and styled by the canonical authored action-row Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu")
	TObjectPtr<UTextBlock> Text_ActionLabel = nullptr;

	/** Optional action icon; collapsed when ActionIcons has no entry for the action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu")
	TObjectPtr<UCommonLazyImage> Image_ActionIcon = nullptr;

	/** Optional divider above the row, shown only for destructive actions. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Context Menu")
	TObjectPtr<UWidget> DestructiveDivider = nullptr;

	/** Icon per action. Designer data on the row Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Style")
	TMap<ERpgInventoryContextAction, TSoftObjectPtr<UTexture2D>> ActionIcons;

	/** Actions shown as destructive: a divider above and DestructiveColor on label and icon. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Style")
	TArray<ERpgInventoryContextAction> DestructiveActions = {ERpgInventoryContextAction::Drop};

	/** Label and icon colour of destructive actions. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Style")
	FSlateColor DestructiveColor = FSlateColor(FLinearColor(0.85f, 0.32f, 0.24f, 1.0f));

	/** Presentation hook for icons, colors, animations, or custom label widgets. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Context Menu", meta = (DisplayName = "On Context Action Configured"))
	void BP_OnContextActionConfigured(ERpgInventoryContextAction Action, const FText& Label);

private:
	void RefreshActionPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryContextMenuWidget> OwningMenu = nullptr;

	UPROPERTY(Transient)
	ERpgInventoryContextAction ContextAction = ERpgInventoryContextAction::Inspect;

	UPROPERTY(Transient)
	FText ActionLabel;

	/** Authored label and icon colours, restored when a pooled row shows a normal action again. */
	FSlateColor DefaultLabelColor;
	FLinearColor DefaultIconColor = FLinearColor::White;
	bool bCapturedDefaultColors = false;
};
