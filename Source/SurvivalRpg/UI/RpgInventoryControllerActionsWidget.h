#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "RpgActivatableWidget.h"

#include "RpgInventoryControllerActionsWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventoryTileView;

/**
 * Activatable inventory screen base that binds stable CommonUI DataTable action rows.
 *
 * Use this for inventory-like screens that need controller panel switching and shortcuts without
 * relying on the experimental CommonUI Enhanced Input bridge. Gameplay mutations still flow
 * through the screen-local drag/drop coordinator and its server-validated UI action component.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryControllerActionsWidget : public URpgActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryControllerActionsWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

	/** Assigns the UI-local coordinators used by the controller action handlers. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Controller")
	void SetInventoryControllerCoordinators(URpgInventoryPanelNavigationCoordinator* InPanelNavigator, URpgInventoryDragDropCoordinator* InDragDropCoordinator);

	/** Current controller panel navigator for this inventory screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Controller")
	URpgInventoryPanelNavigationCoordinator* GetInventoryPanelNavigator() const { return PanelNavigator.Get(); }

	/** Current drag/drop coordinator for this inventory screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Controller")
	URpgInventoryDragDropCoordinator* GetInventoryDragDropCoordinator() const { return DragDropCoordinator.Get(); }

	/** Registers configured CommonUI action rows if this screen is active. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Controller")
	void RegisterInventoryControllerActionBindings();

	/** Unregisters all CommonUI action rows created by this screen. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Controller")
	void UnregisterInventoryControllerActionBindings();

	/** Re-applies focus to the active panel after payload binding or list refreshes. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Controller")
	bool RefreshInventoryControllerFocus();

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Action row for previous inventory panel, usually LB or Q. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle PreviousPanelInputAction;

	/** Action row for next inventory panel, usually RB or E. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle NextPanelInputAction;

	/** Action row for full-stack quick transfer to the logical target panel, usually X. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle QuickTransferInputAction;

	/** Action row for V1 quick split, usually Y. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle QuickSplitInputAction;

	/** Action row for item use/equip convenience, usually F or gamepad right-stick press. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle UseOrEquipInputAction;

	/** Action row for manual item drop, usually D or gamepad left-stick press. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle DropInputAction;

	/** Optional explicit back row. Leave empty to use CommonActivatableWidget's normal back handler. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle BackInputAction;

	/** If true, the registered shortcut rows are shown in the CommonUI action bar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Inventory Controller")
	bool bDisplayInventoryActionsInActionBar = true;

	/** Called after the active panel changes so Blueprint screens can update panel header/border visuals. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Controller", meta = (DisplayName = "On Inventory Active Panel Changed"))
	void BP_OnInventoryActivePanelChanged(FName PanelId, int32 PanelIndex, URpgInventoryTileView* TileView);

private:
	void HandlePreviousPanelAction();
	void HandleNextPanelAction();
	void HandleQuickTransferAction();
	void HandleQuickSplitAction();
	void HandleUseOrEquipAction();
	void HandleDropAction();
	void HandleBackAction();

	UFUNCTION()
	void HandleActivePanelChanged(FName PanelId, int32 PanelIndex, URpgInventoryTileView* TileView, URpgInventoryManagerComponent* Inventory);

	void RegisterActionRow(const FDataTableRowHandle& ActionRow, const FSimpleDelegate& Delegate);
	bool HandleInventoryBackAction();
	static bool IsActionRowValid(const FDataTableRowHandle& ActionRow);

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TArray<FUIActionBindingHandle> InventoryActionBindings;
};
