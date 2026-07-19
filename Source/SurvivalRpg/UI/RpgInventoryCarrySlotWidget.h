#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"

#include "RpgInventoryCarrySlotWidget.generated.h"

class UTexture2D;
class URpgInventoryItemInstance;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySlotGroupViewModel;

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
	/** Binds the canonical 0,0 carry address from a Weapon1, Weapon2, or Offhand group. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Slot")
	void SetCarrySlotGroupViewModel(URpgInventorySlotGroupViewModel* InGroupViewModel);

	/** Registers this single focus target with the screen-local LB/RB panel navigator. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Slot|Navigation")
	void SetPanelNavigationCoordinator(
		URpgInventoryPanelNavigationCoordinator* InPanelNavigationCoordinator,
		FName InPanelId);

	/** Refreshes Blueprint presentation after active Main-/OffHand selection changes without moving the item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Carry Slot")
	void RefreshCarrySlotPresentation();

	/** Group VM that supplies this carry address, or null while the role is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	URpgInventorySlotGroupViewModel* GetCarrySlotGroupViewModel() const { return CarrySlotGroupViewModel.Get(); }

	/** Designer-facing semantic label, for example Weapon 1, Weapon 2, or Shield. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	FText GetCarrySlotLabel() const;

	/** Item physically stored in this ready/carry address, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	URpgInventoryItemInstance* GetCarryItem() const;

	/** Icon of the physically stored item; presentation-only and safe to load asynchronously in Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	TSoftObjectPtr<UTexture2D> GetCarryItemIcon() const;

	/** Current stack count of the represented item, or zero when empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	int32 GetCarryStackCount() const;

	/** True when this carry address contains an item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	bool IsCarrySlotOccupied() const;

	/** True when the carried item is currently selected as the runtime MainHand or OffHand item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	bool IsCarryItemActive() const;

	/** True when an item is ready in this slot but is not the currently active hand selection. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot")
	bool IsCarryItemHolstered() const { return IsCarrySlotOccupied() && !IsCarryItemActive(); }

	/** Current semantic preview state used for Move/Blocked/Pending/Rejected presentation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Carry Slot|Drag")
	ERpgInventoryInteractionPreviewState GetCarryInteractionPreviewState() const;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	/**
	 * Presentation-only hook for icon, stack, empty/occupied, and active/holstered visuals.
	 * Gameplay mutations must continue through the inherited address-slot actions.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Carry Slot", meta = (DisplayName = "On Carry Slot Presentation Changed"))
	void BP_OnCarrySlotPresentationChanged(
		URpgInventoryAddressSlotViewModel* AddressSlotViewModel,
		URpgInventoryItemInstance* Item,
		bool bOccupied,
		bool bActive,
		bool bHolstered);

private:
	UFUNCTION()
	void HandleCarryAddressSlotChanged(URpgInventoryAddressSlotViewModel* ChangedSlotViewModel);

	UFUNCTION()
	void HandleFocusedControllerHeldPayloadChanged(bool bHasHeldPayload, const FRpgInventoryDragPayload& HeldPayload);

	void UnbindCarryAddressObserver();
	void BindFocusedControllerInteraction();
	void UnbindFocusedControllerInteraction();
	void RefreshFocusedControllerInteractionTarget();
	void ClearFocusedControllerInteractionTarget();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventorySlotGroupViewModel> CarrySlotGroupViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryAddressSlotViewModel> ObservedCarryAddress = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelNavigationCoordinator> PanelNavigationCoordinator = nullptr;

	/** Coordinator observed only while this carry surface is in the controller focus path. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> FocusedControllerDragDropCoordinator = nullptr;

	UPROPERTY(Transient)
	FName PanelNavigationId = NAME_None;
};
