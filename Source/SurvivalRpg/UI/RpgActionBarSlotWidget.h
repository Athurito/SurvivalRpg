#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"

#include "RpgActionBarSlotWidget.generated.h"

class UDragDropOperation;
class URpgActionBarSlotViewModel;
class URpgInventoryDragDropCoordinator;

/**
 * Native CommonUI slot entry/drop target for one 1..8 general actionbar slot.
 *
 * It displays URpgActionBarSlotViewModel data and accepts player-inventory SlotAddress payloads.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgActionBarSlotWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgActionBarSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact optional manual MVVM source owned by the canonical actionbar entry Blueprint. */
	static const FName ActionBarSlotViewModelSourceName;

	/** Assigns this widget's actionbar slot VM, useful for manually placed slots outside a ListView. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	void SetActionBarSlotViewModel(URpgActionBarSlotViewModel* InSlotViewModel);

	/** Assigns the screen-local drag/drop coordinator shared by inventory and actionbar widgets. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Marks whether the owning actionbar panel is currently selected by controller navigation. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	void SetActionBarPanelActive(bool bInActionBarPanelActive);

	/** Current actionbar slot VM represented by this widget. */
	UFUNCTION(BlueprintPure, Category = "Action Bar|Slot")
	URpgActionBarSlotViewModel* GetActionBarSlotViewModel() const { return SlotViewModel.Get(); }

	/** Current drag/drop presentation state for this actionbar entry. */
	UFUNCTION(BlueprintPure, Category = "Action Bar|Slot")
	ERpgInventorySlotDragVisualState GetCurrentDragDropVisualState() const { return CurrentDragDropVisualState; }

	/** Controller/CommonUI Accept helper: binds the held SlotAddress payload to this actionbar slot. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	bool HandleSlotAccept();

	/** Recomputes held-payload/drop-target visuals and calls the Blueprint visual hook. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	void RefreshDragDropVisualState();

	/** Updates mouse-drag hover feedback for an explicit payload. Invalid targets are still visibly handled. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	bool PreviewPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Commits an explicit mouse-drag payload to this actionbar slot through the server-authoritative action path. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	bool CommitPayloadDrop(const FRpgInventoryDragPayload& Payload);

	/** Clears transient mouse-drag hover feedback without changing gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|Slot")
	void ClearExternalPreviewPayload();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Blueprint presentation hook called when this entry is released for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action Bar|Slot", meta = (DisplayName = "On Action Bar Slot Released"))
	void BP_OnActionBarSlotReleased();

	/** Blueprint presentation hook for CommonUI selection/focus. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action Bar|Slot", meta = (DisplayName = "On Action Bar Slot Selection Changed"))
	void BP_OnActionBarSlotSelectionChanged(bool bIsSelected);

	/** Blueprint presentation hook for held-payload/drop-target highlights. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action Bar|Slot", meta = (DisplayName = "On Action Bar Slot DragDrop State Changed"))
	void BP_OnActionBarSlotDragDropStateChanged(ERpgInventorySlotDragVisualState NewState);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgActionBarSlotEntryPoolingTest;
#endif

	UFUNCTION()
	void HandleSlotViewModelChanged(URpgActionBarSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	FRpgInventoryDropTarget MakeDropTarget() const;
	bool InjectActionBarSlotViewModelIntoMvvm();
	void ReleaseActionBarSlotState();

	UPROPERTY(Transient)
	TObjectPtr<URpgActionBarSlotViewModel> SlotViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	ERpgInventorySlotDragVisualState CurrentDragDropVisualState = ERpgInventorySlotDragVisualState::Normal;

	bool bSlotSelected = false;
	bool bActionBarPanelActive = true;
	bool bHasExternalPreviewState = false;
	ERpgInventorySlotDragVisualState ExternalPreviewState = ERpgInventorySlotDragVisualState::Normal;
	int32 ExternalPreviewActionBarSlotIndex = INDEX_NONE;
	bool bActionBarSlotStateReleased = false;
};
