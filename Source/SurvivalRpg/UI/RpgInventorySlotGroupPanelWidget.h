#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "RpgInventorySlotGroupPanelWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySlotGroupViewModel;
class URpgInventorySlotGroupWidget;
class URpgInventorySpatialGridWidget;
class UPanelWidget;

/**
 * Plain panel builder for carry/content spatial groups.
 *
 * It creates URpgInventorySlotGroupWidget children directly, avoiding CommonListView/ListView recycling for spatial
 * inventory containers whose layout must stay fixed regardless of resolution.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventorySlotGroupPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotGroupPanelWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated group widgets and their spatial grids. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Assigns the screen-local navigator used by generated group widgets and their spatial grids. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelIdPrefix);

	/** Reconciles children by stable group id so existing grids retain cursor, focus, and interaction state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Slot Group")
	void SetSlotGroupItems(const TArray<URpgInventorySlotGroupViewModel*>& InGroups);

	/** Appends all currently generated spatial grids to OutGrids for screen-level drag/drop routing. */
	void GetSpatialGridWidgets(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;

protected:
	/** Optional Blueprint panel that receives group widgets. If unset, an existing root panel is used. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> GroupsPanel = nullptr;

	/** Widget class used for one spatial group. Blueprint should usually point this at CUI_InventorySlotGroupEntry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Slot Group")
	TSubclassOf<URpgInventorySlotGroupWidget> GroupWidgetClass;

private:
	void EnsureGroupsPanel();
	void RebuildGroupWidgets();
	void ApplyCoordinatorToGroup(URpgInventorySlotGroupWidget* GroupWidget) const;
	void ApplyNavigationToGroup(URpgInventorySlotGroupWidget* GroupWidget) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationIdPrefix = NAME_None;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> GroupItems;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgInventorySlotGroupWidget>> GroupWidgets;
};
