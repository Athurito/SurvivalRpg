#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViewTypes.h"

#include "RpgInventorySpatialCellWidget.generated.h"

class URpgInventoryAddressSlotViewModel;
class URpgInventoryEntryViewModel;
class URpgInventorySpatialGridWidget;
class UDragDropOperation;
class UImage;

/**
 * Designable background cell for one spatial inventory grid coordinate.
 *
 * Cells are presentation and hit-test widgets only. They never own item truth; placement validation still routes
 * through the owning grid and server-authoritative inventory actions.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventorySpatialCellWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventorySpatialCellWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the owning grid and fixed grid coordinate represented by this cell. */
	void SetOwningSpatialGrid(URpgInventorySpatialGridWidget* InOwningGrid, int32 InCellX, int32 InCellY);

	/** Assigns optional player-layout or storage state represented underneath this cell. */
	void SetCellViewModels(URpgInventoryAddressSlotViewModel* InAddressSlotViewModel, URpgInventoryEntryViewModel* InEntryViewModel);

	/** Updates the visual state sent to the Blueprint styling hook. */
	void SetCellVisualState(ERpgInventorySpatialCellVisualState InVisualState);

	/** X coordinate represented by this cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	int32 GetCellX() const { return CellX; }

	/** Y coordinate represented by this cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	int32 GetCellY() const { return CellY; }

	/** Player address VM represented by this cell, if this grid is bound to the player layout. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	URpgInventoryAddressSlotViewModel* GetAddressSlotViewModel() const { return AddressSlotViewModel.Get(); }

	/** Storage entry VM occupying this cell, if any. Empty storage cells have no entry VM. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Current visual state after hover/selection/preview resolution. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Spatial Cell")
	ERpgInventorySpatialCellVisualState GetCurrentCellVisualState() const { return CurrentVisualState; }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Called after this cell receives its grid coordinate and optional backing VM references. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Cell", meta = (DisplayName = "On Spatial Cell Set"))
	void BP_OnSpatialCellSet(int32 NewCellX, int32 NewCellY, URpgInventoryAddressSlotViewModel* NewAddressSlotViewModel, URpgInventoryEntryViewModel* NewEntryViewModel);

	/** Called when selection, hover, occupancy, or drop preview changes how this cell should look. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Spatial Cell", meta = (DisplayName = "On Spatial Cell State Changed"))
	void BP_OnSpatialCellStateChanged(ERpgInventorySpatialCellVisualState NewState);

	/** Optional background styled natively before Blueprint animation runs, guaranteeing stale tint reset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|Spatial Cell|Style")
	TObjectPtr<UImage> Image_Background = nullptr;

	/** Neutral tint used for Normal, Occupied, and Covered states. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor NeutralTint = FLinearColor::White;

	/** Subtle mouse-hover tint when no stronger state is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor HoveredTint = FLinearColor(0.36f, 0.36f, 0.36f, 1.0f);

	/** Logical controller-selection tint; pointer hover never writes this state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor SelectedTint = FLinearColor(0.85f, 0.78f, 0.42f, 1.0f);

	/** Tint shared by locally valid Move, Merge, Swap, and Equip footprints. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor ValidPreviewTint = FLinearColor(0.08f, 0.82f, 0.18f, 0.82f);

	/** Tint for blocked and out-of-bounds footprints. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor InvalidPreviewTint = FLinearColor(0.82f, 0.06f, 0.05f, 0.88f);

	/** Tint while waiting for a server acknowledgement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor PendingPreviewTint = FLinearColor(1.0f, 0.48f, 0.04f, 0.9f);

	/** Tint used briefly after authoritative rejection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Spatial Cell|Style")
	FLinearColor RejectedPreviewTint = FLinearColor(1.0f, 0.02f, 0.02f, 1.0f);

private:
	void ApplyResolvedVisualState();
	void ApplyNativeVisualStyle(ERpgInventorySpatialCellVisualState NewState);
	ERpgInventorySpatialCellVisualState ResolveHoveredVisualState() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySpatialGridWidget> OwningGrid = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> AddressSlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	int32 CellX = INDEX_NONE;
	int32 CellY = INDEX_NONE;
	bool bHovered = false;
	bool bPendingLeftClickAccept = false;
	bool bHasAppliedVisualState = false;
	ERpgInventorySpatialCellVisualState BaseVisualState = ERpgInventorySpatialCellVisualState::Normal;
	ERpgInventorySpatialCellVisualState CurrentVisualState = ERpgInventorySpatialCellVisualState::Normal;
};
