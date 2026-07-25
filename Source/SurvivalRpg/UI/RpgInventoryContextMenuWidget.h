#pragma once

#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionSource.h"

#include "RpgInventoryContextMenuWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UVerticalBox;
class UWidget;
class URpgInventoryContextActionEntryWidget;
class URpgQuickAccessSlotPickerEntryWidget;
struct FUIInputConfig;

/**
 * Mouse-first authored inventory context menu.
 *
 * The widget captures one exact capability snapshot, builds one button per queried semantic action, positions
 * the menu at an absolute Slate screen position, and delegates every click through the shared native source seam.
 * Clicking outside the menu or invoking Back/Escape closes it without a gameplay mutation.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryContextMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContextMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Initializes and populates the context menu from one Content, Address/Carry, or Gear capability source.
	 *
	 * @param InContextSource Native source that queries and revalidates its exact current item snapshot.
	 * @param InScreenPosition Absolute Slate screen position at which the menu should open.
	 * @return True when the source snapshot and authored controls are valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	bool InitializeContextMenu(
		UWidget* InContextSource,
		FVector2D InScreenPosition);

	/** Executes one displayed semantic action after stable-entry revalidation, then closes the menu. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	bool ExecuteContextAction(ERpgInventoryContextAction Action);

	/** Opens the shared eight-slot picker for the captured item without mutating gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	bool ShowQuickAccessSlotPicker();

	/** Returns from the 1..8 picker to the normal context-action page. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access")
	void ShowContextActionPage();

	/** Binds the captured item to a zero-based actionbar slot, then closes the menu on dispatch. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Access", meta = (ClampMin = "0", ClampMax = "7"))
	bool SelectQuickAccessSlot(int32 SlotIndex);

	/** Converts an internal 0..7 index to its player-facing 1..8 number, or INDEX_NONE when invalid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	static int32 ToQuickAccessDisplayNumber(int32 SlotIndex);

	/** Converts a player-facing 1..8 number to its internal 0..7 index, or INDEX_NONE when invalid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	static int32 ToQuickAccessSlotIndex(int32 DisplaySlotNumber);

	/** Closes the context menu without executing an inventory action. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Context Menu")
	void CloseContextMenu();

	/** Stable replicated entry id captured when this context menu was initialized. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FGuid GetContextEntryId() const { return ContextEntryId; }

	/** Persistent gear-item identity captured when an equipment-source menu was initialized. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Context Menu")
	FRpgInventoryItemId GetContextItemId() const { return ContextItemId; }

	//~UCommonActivatableWidget interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Full-screen authored dismiss hit target placed behind the menu. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UButton> Button_Dismiss = nullptr;

	/** Canvas that positions the menu independently from the inventory screen's designer-authored layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UCanvasPanel> ContextMenuCanvas = nullptr;

	/** Visual menu panel whose desired size is used to clamp the menu inside the viewport. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UBorder> ContextMenuBorder = nullptr;

	/** Vertical host rebuilt with one authored row per supplied semantic inventory action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UVerticalBox> ActionsBox = nullptr;

	/** Authored host for the eight shared Quick Access slot-picker rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UVerticalBox> QuickAccessSlotsBox = nullptr;

	/** Authored back button shown only on the Quick Access picker page. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Context Menu|Controls")
	TObjectPtr<UButton> Button_QuickAccessBack = nullptr;

	/** Exact editor-authored CommonButton row class used for semantic actions. Missing configuration fails closed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Entries")
	TSubclassOf<URpgInventoryContextActionEntryWidget> ActionEntryWidgetClass;

	/** Editor-authored CommonButton row class used for each 1..8 picker destination. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Context Menu|Entries")
	TSubclassOf<URpgQuickAccessSlotPickerEntryWidget> QuickAccessSlotEntryWidgetClass;

private:
	void BindDismissControl();
	void RebuildActionButtons();
	void RebuildQuickAccessSlotButtons();
	FText ResolveContextActionLabel(ERpgInventoryContextAction Action) const;
	int32 ResolveCurrentQuickAccessSlotIndex() const;
	FText ResolveQuickAccessBindingLabel(int32 SlotIndex, bool& bOutOccupied) const;
	void UpdateContextMenuPosition();
	void ResetContextState();
	void HandleContextActionClicked(ERpgInventoryContextAction Action);
	friend class URpgInventoryContextActionEntryWidget;
	friend class URpgQuickAccessSlotPickerEntryWidget;

	/** Handles clicks on the full-screen area outside the action panel. */
	UFUNCTION()
	void HandleDismissClicked();

	UFUNCTION()
	void HandleQuickAccessBackClicked();

	/** Weak native capability source so a pooled menu cannot retain the screen that opened it. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWidget> ContextSource;

	/** Exact represented state captured before the modal is activated. */
	UPROPERTY(Transient)
	FRpgInventoryContextActionSnapshot ContextSnapshot;

	/** Stable selected entry identity captured before any menu click can change focus. */
	UPROPERTY(Transient)
	FGuid ContextEntryId;

	/** Persistent item identity used to reject stale gear-menu actions after slot replication changes. */
	UPROPERTY(Transient)
	FRpgInventoryItemId ContextItemId;

	/** Quick Access binding captured with the menu so an old Unbind row cannot clear a newly moved binding. */
	int32 ContextQuickAccessSlotIndex = INDEX_NONE;

	/** Deduplicated action order supplied by the presenter and rendered through authored row classes. */
	UPROPERTY(Transient)
	TArray<ERpgInventoryContextAction> ContextActions;

	/** Runtime-created buttons retained for focus selection and safe rebuilds. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonButtonBase>> ActionButtons;

	/** Runtime-created picker entries retained for focus and safe page rebuilds. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgQuickAccessSlotPickerEntryWidget>> QuickAccessSlotButtons;

	FVector2D RequestedScreenPosition = FVector2D::ZeroVector;
	bool bContextPositionPending = false;
	bool bShowingQuickAccessPicker = false;
};
