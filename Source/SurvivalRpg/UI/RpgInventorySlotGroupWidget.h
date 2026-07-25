#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"

#include "RpgInventorySlotGroupWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySlotGroupViewModel;
class URpgInventorySpatialGridWidget;

/**
 * Widget for one visible spatial slot group.
 *
 * It owns or binds a URpgInventorySpatialGridWidget directly; no ListView entry recycling is used for spatial groups.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySlotGroupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Exact manual MVVM source name authored by the canonical spatial slot-group widget. */
	static const FName SlotGroupViewModelSourceName;

	/**
	 * Atomically composes the group MVVM source, spatial grid, coordinator, navigator, and screen presentation host.
	 * Static host geometry remains designer-authored; this method binds only transient read/interaction state.
	 */
	void BindInventoryPresentation(
		URpgInventorySlotGroupViewModel* InGroupViewModel,
		const FRpgInventoryScreenPresentationContext& InContext,
		FName InPanelIdPrefix);

	/** Releases the group MVVM source and complete spatial presentation context for CommonUI pooling. */
	void ReleaseInventoryPresentation();

	/** Assigns the screen-local coordinator and forwards it to the spatial grid. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local panel navigator used for LB/RB focus and shortcut routing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/**
	 * Assigns the group VM to both the native spatial grid and the authored read-only MVVM leaf source.
	 * Spatial groups are created by a panel builder, not a ListView.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Spatial grid owned by this group widget, if the Blueprint supplied one. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	URpgInventorySpatialGridWidget* GetSpatialGridWidget() const { return SpatialGrid.Get(); }

	/** Stable container/group id used by the parent panel to retain this widget across VM refreshes. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	FName GetSlotGroupId() const;

	/** Full root or item-owned identity used to distinguish equal local container ids. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Slot Group")
	FRpgInventoryContainerHandle GetSlotGroupHandle() const;

protected:
	virtual void NativeDestruct() override;

	/** Optional inner spatial grid. Name the widget SpatialGrid for automatic binding. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialGridWidget> SpatialGrid = nullptr;

private:
	bool InjectSlotGroupViewModelIntoMvvm();
	void EnsureSpatialGrid();
	void RegisterPanelNavigationEntry();
	URpgInventoryManagerComponent* ResolveGroupInventory() const;
	FName MakePanelNavigationId() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> GroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationIdPrefix = NAME_None;
};
