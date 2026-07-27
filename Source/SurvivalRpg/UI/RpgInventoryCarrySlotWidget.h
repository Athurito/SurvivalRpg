#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"

#include "RpgInventoryCarrySlotWidget.generated.h"

class UBorder;
class UCommonLazyImage;
class UTexture2D;
class URpgInventoryItemInstance;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySlotGroupViewModel;

/** Mutually exclusive, presentation-only state derived from the physical Carry item and active loadout selection. */
UENUM(BlueprintType)
enum class ERpgInventoryCarryPresentationState : uint8
{
	/** No item is stored in the Carry address. */
	Empty,

	/** An item is stored in the Carry address but is not the active MainHand or OffHand selection. */
	Holstered,

	/** The stored item is the active MainHand or OffHand selection. */
	Active
};

/**
 * Gear-like presentation for exactly one semantic ready/carry address such as WeaponSlot1[0,0].
 *
 * The widget deliberately owns no grid and no item state. Its complete designer-authored geometry is one drop and
 * controller-focus target, while the bound address VM and inventory coordinator remain the read/command seams.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventoryCarrySlotWidget : public URpgInventoryAddressSlotWidget
{
	GENERATED_BODY()

public:
	/**
	 * Atomically binds this Carry presenter to its semantic group and the active screen-owned presentation context.
	 * The inherited Address presenter remains the sole observer and injects the exact optional Manual MVVM source.
	 */
	void BindInventoryPresentation(
		URpgInventorySlotGroupViewModel* InGroupViewModel,
		const FRpgInventoryScreenPresentationContext& InContext,
		FName InPanelId);

	/** Releases Carry focus/state and the inherited Address/MVVM presentation for screen pooling. */
	virtual void ReleaseInventoryPresentation() override;

	/** Natively binds the canonical 0,0 carry address from a Weapon1, Weapon2, or Offhand group. */
	void SetCarrySlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Natively registers this single focus target with the screen-local LB/RB panel navigator. */
	void SetPanelNavigationCoordinator(
		URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator,
		FName InPanelId);

	/** Natively refreshes presentation after active Main-/OffHand selection changes without moving the item. */
	void RefreshCarrySlotPresentation();

	/** Group VM that supplies this carry address, or null while the role is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	URpgInventorySlotGroupViewModel* GetCarrySlotGroupViewModel() const { return CarrySlotGroupViewModel.Get(); }

	/** Current mutually exclusive Empty/Holstered/Active presentation state. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	ERpgInventoryCarryPresentationState GetCarryPresentationState() const { return CarryPresentationState; }

	/** Current semantic preview state used for Move/Blocked/Pending/Rejected presentation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot|Drag")
	ERpgInventoryInteractionPreviewState GetCarryInteractionPreviewState() const;

	/**
	 * MVVM-only destination for Icon. Blueprint graphs must not use this as a parallel item-data writer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Slot|MVVM", meta = (BlueprintProtected = "true"))
	void SetCarryItemIcon(TSoftObjectPtr<UTexture2D> Icon);

	/**
	 * MVVM-only destination for bRenderItemVisual. Blueprint graphs must not use this as a parallel item-data writer.
	 * Release calls it with false because clearing an optional MVVM source does not push a default value to targets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Slot|MVVM", meta = (BlueprintProtected = "true"))
	void SetCarryItemVisualVisible(bool bVisible);

protected:
	virtual void NativeOnInitialized() override;
	virtual void RefreshAddressSlotPresentation() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	/** Required item icon target. Its brush and visibility are authored only through the exact Address MVVM source. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonLazyImage> ItemIcon = nullptr;

	/** Required presentation-only indicator for the active MainHand or OffHand selection. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> ActiveIndicator = nullptr;

	/** Required presentation-only indicator for a stored but inactive Carry item. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> HolsteredIndicator = nullptr;

	/**
	 * Opacity applied to the authored Active/Holstered fill colors behind the item icon.
	 * Designer-tuned, cosmetic-only, and never used to derive equipment state.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Carry Slot|Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float StateIndicatorOpacity = 0.08f;

	/**
	 * Presentation-only transition hook for animation and styling.
	 * Stable item data is intentionally absent and remains owned by the Address MVVM source.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Carry Slot", meta = (DisplayName = "On Carry Slot State Changed"))
	void BP_OnCarrySlotStateChanged(
		ERpgInventoryCarryPresentationState NewState,
		ERpgInventoryInteractionPreviewState NewInteractionPreviewState);

private:
	UFUNCTION()
	void HandleFocusedControllerHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	URpgInventoryItemInstance* GetCarryItem() const;
	bool IsCarryItemActive() const;
	ERpgInventoryCarryPresentationState ResolveCarryPresentationState() const;
	void ApplyStateIndicatorOpacity();
	void EnsureItemIconPaintsAboveStateIndicators();
	void ApplyCarryPresentationState(
		ERpgInventoryCarryPresentationState NewState,
		ERpgInventoryInteractionPreviewState NewInteractionPreviewState);
	void BindFocusedControllerInteraction();
	void UnbindFocusedControllerInteraction();
	void RefreshFocusedControllerInteractionTarget();
	void ClearFocusedControllerInteractionTarget(bool bClearExternalPreviewState = true);

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> CarrySlotGroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	/** Coordinator observed only while this carry surface is in the controller focus path. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> FocusedControllerDragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;

	UPROPERTY(Transient)
	ERpgInventoryCarryPresentationState CarryPresentationState = ERpgInventoryCarryPresentationState::Empty;

	UPROPERTY(Transient)
	ERpgInventoryInteractionPreviewState CarryInteractionPreviewState = ERpgInventoryInteractionPreviewState::None;

	bool bCarryPresentationStateInitialized = false;
};
