#pragma once

#include "CommonTileView.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"

#include "RpgActionBarTileView.generated.h"

class APlayerController;
class URpgActionBarSlotWidget;
class URpgActionBarSlotViewModel;
class URpgInventoryDragDropCoordinator;
class URpgInventoryPanelNavigationCoordinator;

/**
 * TileView specialization for 1..8 actionbar slot VMs.
 *
 * The actionbar remains a non-spatial single-slot strip, so CommonTileView is still appropriate here.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgActionBarTileView : public UCommonTileView
{
	GENERATED_BODY()

public:
	explicit URpgActionBarTileView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by generated actionbar slot entries. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slots")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Replaces list items with the supplied actionbar slot VMs. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slots")
	void SetActionBarSlotItems(const TArray<URpgActionBarSlotViewModel*>& InSlots);

	/** Current selected actionbar slot view model, or null when selection is empty/non-actionbar. */
	UFUNCTION(BlueprintPure, Category = "Action Bar|Navigation")
	URpgActionBarSlotViewModel* GetSelectedActionBarSlot() const;

	/** Selects a list item and moves controller focus to this actionbar TileView. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectActionBarListItem(UObject* Item, APlayerController* OwningPlayer);

	/** Selects current valid selection or the first actionbar slot. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectBestActionBarSlot(APlayerController* OwningPlayer);

	/** Selects by zero-based actionbar slot index. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	bool SelectActionBarSlotByIndex(int32 SlotIndex, APlayerController* OwningPlayer);

	/** Clears only the visible ListView selection. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void ClearActionBarSelectionVisual();

	/** Updates mouse-drag hover feedback for the actionbar slot under a screen position. */
	bool PreviewPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);

	/** Commits a mouse-drag payload to the actionbar slot under a screen position. */
	bool CommitPayloadAtScreenPosition(const FRpgInventoryDragPayload& Payload, FVector2D ScreenPosition);

	/** Whether the screen position resolves to one concrete displayed 1-8 slot rather than only the TileView bounds. */
	bool HasActionBarSlotAtScreenPosition(FVector2D ScreenPosition) const;

	/** Clears transient mouse-drag feedback on displayed actionbar slots. */
	void ClearExternalPreviewPayloads();

	/** Marks this actionbar panel as the active controller target. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void SetActionBarPanelActive(bool bInActionBarPanelActive);

	/** Registers this TileView with the screen-local panel navigator so selection changes update active-panel routing. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Navigation")
	void SetPanelNavigationCoordinator(URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator, FName InPanelId);

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual void OnSelectionChangedInternal(UObject* FirstSelectedItem) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;
	void ApplyPanelActiveStateToEntry(UUserWidget* EntryWidget) const;
	URpgActionBarSlotWidget* FindActionBarSlotWidgetAtScreenPosition(FVector2D ScreenPosition) const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	bool bActionBarPanelActive = true;
	bool bSuppressPanelSelectionNotify = false;
};
