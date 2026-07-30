#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "RpgActivatableWidget.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"

#include "RpgInventoryControllerActionsWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryPanelNavigationCoordinator;

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

#if WITH_EDITOR
	//~UWidget interface
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
	//~End of UWidget interface
#endif

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
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/**
	 * Native presentation hook invoked whenever the shared navigator changes its active panel.
	 * Derived screens may update cosmetic source indicators but must not mutate inventory state here.
	 */
	virtual void NativeOnInventoryActivePanelChanged(FName PanelId, int32 PanelIndex);

	/**
	 * Resolves the visible quick-transfer label for the current valid selection.
	 * The default preserves the authored CommonUI action-row display name.
	 */
	virtual FText ResolveQuickTransferDisplayName() const;

	/** Re-evaluates action visibility and the contextual transfer label after native interaction-state changes. */
	void RefreshInventoryActionBindingVisibility();

	/** True only when this handle resolves to a CommonUI action row suitable for runtime binding. */
	static bool IsActionRowValid(const FDataTableRowHandle& ActionRow);

#if WITH_EDITOR
	/**
	 * Emits an actionable Widget Blueprint compiler error for an unset or malformed CommonUI action row.
	 * Optional rows may be completely empty, but partial or stale references still fail compilation.
	 */
	static void ValidateCommonInputActionRow(
		class IWidgetCompilerLog& CompileLog,
		const FDataTableRowHandle& ActionRow,
		const FText& PropertyLabel,
		bool bRequired);
#endif

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
	void BP_OnInventoryActivePanelChanged(FName PanelId, int32 PanelIndex);

private:
	void HandlePreviousPanelAction();
	void HandleNextPanelAction();
	void HandleQuickTransferAction();
	void HandleQuickSplitAction();
	void HandleUseOrEquipAction();
	void HandleDropAction();
	void HandleBackAction();

	UFUNCTION()
	void HandleActivePanelChanged(FName PanelId, int32 PanelIndex);

	UFUNCTION()
	void HandleActiveSelectionChanged();

	UFUNCTION()
	void HandleHeldPayloadChanged(bool bHasPayload, const FRpgInventoryDragPayload& Payload);

	FUIActionBindingHandle RegisterActionRow(const FDataTableRowHandle& ActionRow, const FSimpleDelegate& Delegate);
	void RestoreQuickTransferActionDisplayName();
	bool HandleInventoryBackAction();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigator = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	TArray<FUIActionBindingHandle> InventoryActionBindings;

	/** Tracks the CommonUI F binding so the native key fallback can never execute the same action twice. */
	FUIActionBindingHandle UseOrEquipActionBinding;
	FUIActionBindingHandle QuickTransferActionBinding;
	FUIActionBindingHandle QuickSplitActionBinding;
	FUIActionBindingHandle DropActionBinding;

	/** Exact authored display name captured from the registered quick-transfer action row. */
	FText CachedQuickTransferActionDisplayName;
	FText AppliedQuickTransferActionDisplayName;
	bool bHasCachedQuickTransferActionDisplayName = false;
};
