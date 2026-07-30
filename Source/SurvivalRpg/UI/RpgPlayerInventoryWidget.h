#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

#include "RpgPlayerInventoryWidget.generated.h"

class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySpatialGridWidget;
class URpgPlayerInventoryPaneWidget;
class URpgPlayerInventoryViewModel;
struct FRpgInventoryScreenPresentationContext;

/**
 * Thin standalone CommonUI shell for the reusable player-inventory pane.
 *
 * This activatable root owns the one screen interaction context, input actions, focus, modals, feedback, and drag
 * canvas. The passive child owns only the complete read-only Player inventory presentation.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgPlayerInventoryWidget
	: public URpgInventoryInteractionScreenWidget
{
	GENERATED_BODY()

public:
	/** Legacy source name retained for C++ compatibility; the PropertyPath now belongs to the pane asset. */
	static const FName PlayerInventoryViewModelSourceName;

	/** Deprecated compatibility accessor forwarding to the pane-owned stable aggregate projection. */
	UFUNCTION(
		BlueprintPure,
		Category = "Inventory|Player",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use PlayerInventoryPane.GetPlayerInventoryViewModel instead."))
	URpgPlayerInventoryViewModel* GetPlayerInventoryViewModel() const;

	/** Complete passive pane embedded by the authored standalone screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player")
	URpgPlayerInventoryPaneWidget* GetPlayerInventoryPane() const
	{
		return PlayerInventoryPane;
	}

	/** Forwards the pane's compact runtime binding and projection summary. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player|Debug")
	FString GetPlayerInventoryWidgetDebugSummary() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void BindInventoryScreenPresentation() override;
	virtual void UnbindInventoryScreenPresentation() override;
	virtual void ForwardInventoryInteractionContextToChildren() override;
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual FName GetInitialInventoryNavigationPanelId() const override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed) override;
	virtual void ClearInventoryScreenSpecificDragPreviews() override;
	virtual bool UpdateInventoryScreenSpecificControllerDragVisual(
		const FRpgInventoryDragPayload& Payload) override;
	virtual void RefreshInventoryScreenSpecificInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest) override;

	/** Required reusable player inventory pane authored in CUI_PlayerInventory. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgPlayerInventoryPaneWidget> PlayerInventoryPane = nullptr;

private:
	FRpgInventoryScreenPresentationContext MakePlayerPaneContext() const;
	void HandlePlayerInventoryPaneNavigationPanelsChanged();
};
