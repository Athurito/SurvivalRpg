#pragma once

#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgInventorySpatialPaneWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryInteractionScreenWidget;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventoryPanelViewModel;
class URpgInventorySpatialGridWidget;

/**
 * Reusable authored presentation leaf for one exact spatial inventory container.
 *
 * The pane owns one stable read-only panel view model, while its parent screen owns payload validation, CommonUI
 * lifecycle, drag/drop coordination, navigation, quick-transfer policy, and every gameplay mutation. This widget is
 * deliberately neither activatable nor a screen-payload receiver.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialPaneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialPaneWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Projects one exact root or item-owned container without taking ownership of the inventory component. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Pane")
	void BindInventoryContainer(
		URpgInventoryManagerComponent* InInventory,
		FRpgInventoryContainerHandle InContainerHandle);

	/**
	 * Supplies the screen-owned interaction objects and semantic panel id.
	 * The pane stores no peer relationship and never creates quick-transfer routes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Pane")
	void SetInteractionContext(
		URpgInventoryDragDropCoordinator* InDragDropCoordinator,
		URpgInventoryPanelNavigationCoordinator* InPanelNavigator,
		FName InPanelId,
		URpgInventoryInteractionScreenWidget* InPresentationHost);

	/** Registers the bound grid inside the parent screen's current navigation refresh transaction. */
	void RegisterNavigationPanel(
		URpgInventoryPanelNavigationCoordinator* InPanelNavigator = nullptr);

	/** Idempotently releases gameplay observation and transient interaction state while retaining the stable VM. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Spatial Pane")
	void ReleaseInventoryPresentation();

	/** Authored spatial grid leaf, or null when the Blueprint composition is incomplete. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Pane")
	URpgInventorySpatialGridWidget* GetSpatialGrid() const { return SpatialGrid.Get(); }

	/** Stable pane-owned read-only projection retained across CommonUI screen pooling. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Pane")
	URpgInventoryPanelViewModel* GetPanelViewModel();

	/** Inventory currently projected by this pane; UI-read-only and never owned by the pane. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Pane")
	URpgInventoryManagerComponent* GetBoundInventory() const { return BoundInventory.Get(); }

	/** Exact graph address currently projected by this pane. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Pane")
	FRpgInventoryContainerHandle GetBoundContainerHandle() const { return BoundContainerHandle; }

	/** Semantic id used by the screen-owned panel navigator. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Pane")
	FName GetPanelId() const { return PanelId; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/**
	 * Required authored spatial grid for this pane. Runtime tests may instantiate the native class directly, but every
	 * concrete Widget Blueprint must provide this exact typed binding.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySpatialGridWidget> SpatialGrid = nullptr;

private:
	void EnsurePanelViewModel();
	void ApplyInteractionContextToGrid();

	/** Stable presentation-only VM whose UObject Outer is this pane. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelViewModel> PanelViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> BoundInventory = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle BoundContainerHandle;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigator = nullptr;

	UPROPERTY(Transient)
	FName PanelId = NAME_None;

	/** Parent screen that centrally owns transient context-menu and split-dialog presentation. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionScreenWidget> InventoryPresentationHost = nullptr;
};
