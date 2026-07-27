#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryContextActionTypes.h"

#include "RpgInventoryContextActionEntryWidget.generated.h"

class UTextBlock;
class URpgInventoryContextMenuWidget;

/**
 * Designer-owned CommonUI row for one semantic inventory context action.
 *
 * The canonical Blueprint must author Text_ActionLabel and may style the CommonButton normally. The
 * native class forwards clicks to the owning context menu without per-action delegates.
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

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnClicked() override;

	/** Required localized label owned and styled by the canonical authored action-row Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu")
	TObjectPtr<UTextBlock> Text_ActionLabel = nullptr;

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
};
