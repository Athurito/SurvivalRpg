#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

#include "RpgPlayerInventoryWidget.generated.h"

class URpgActionBarTileView;
class URpgEquipmentSlotWidget;
class URpgInventoryCarrySlotWidget;
class URpgInventoryDragDropCoordinator;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySpatialGridWidget;
class URpgInventorySlotGroupWidget;
class URpgInventorySlotGroupViewModel;
class URpgPlayerInventoryViewModel;
class UWidget;

/**
 * Native base for the player inventory screen.
 *
 * The native presenter owns exactly one screen-scoped player-inventory view model and injects that instance into
 * the authored manual MVVM source. It then wires the read-only projection into named Blueprint widgets so the
 * screen does not need manual slot loops or per-entry coordinator setup. Gameplay state stays in inventory,
 * layout, equipment, and actionbar components; this widget only connects view models to CommonUI views.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgPlayerInventoryWidget : public URpgInventoryInteractionScreenWidget
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact manual MVVM source name authored in CUI_PlayerInventory and populated by this native presenter. */
	static const FName PlayerInventoryViewModelSourceName;

	/**
	 * Native-owned aggregate MVVM projection used by this screen.
	 * Its UObject Outer is this widget and the same instance is retained across CommonUI pooling.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player")
	URpgPlayerInventoryViewModel* GetPlayerInventoryViewModel() const { return PlayerInventoryViewModel; }

	/** Returns a compact runtime summary for debugging Blueprint widget binding and VM list counts. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player|Debug")
	FString GetPlayerInventoryWidgetDebugSummary() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void BindInventoryScreenPresentation() override;
	virtual void UnbindInventoryScreenPresentation() override;
	virtual void ForwardInventoryInteractionContextToChildren() override;
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed) override;
	virtual void ClearInventoryScreenSpecificDragPreviews() override;
	virtual bool UpdateInventoryScreenSpecificControllerDragVisual(
		const FRpgInventoryDragPayload& Payload) override;
	virtual void RefreshInventoryScreenSpecificInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest) override;

	/** Freely placed Pockets spatial host; player screens no longer require a shared group-list layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pockets = nullptr;

	/**
	 * Migration-safe binding for the gear-like Weapon1 carry host. Canonical content uses RpgInventoryCarrySlotWidget;
	 * legacy SlotGroup widgets remain loadable but are ignored with a clear warning until reparented in the editor.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Carry_Weapon1 = nullptr;

	/** Migration-safe binding for the gear-like Weapon2 carry host; reparent it to RpgInventoryCarrySlotWidget. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Carry_Weapon2 = nullptr;

	/** Migration-safe binding for the gear-like offhand/shield carry host. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Carry_Offhand = nullptr;

	/** Optional freely placed content host for the currently equipped backpack. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Backpack = nullptr;

	/** Optional freely placed content host for the currently equipped belt. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Belt = nullptr;

	/** Optional freely placed content host for the currently equipped pouch. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pouch = nullptr;

	/** Optional freely placed content host for the currently equipped resource bag. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_ResourceBag = nullptr;

	/** Optional 1..8 actionbar preview/drop target inside the inventory screen. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgActionBarTileView> ActionBarTileView = nullptr;

	/** Optional fixed armor slot widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Head = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Chest = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Hands = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Legs = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Feet = nullptr;

	/** Optional bag/provider equipment slot widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Backpack = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Belt = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Pouch = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_ResourceBag = nullptr;

private:
	UFUNCTION()
	void HandleGearSlotsChanged();

	UFUNCTION()
	void HandleSlotGroupsChanged();

	UFUNCTION()
	void HandleActionBarSlotsChanged();

	void EnsurePlayerInventoryViewModel();
	bool InjectPlayerInventoryViewModelIntoMvvm();
	void BindViewModelDelegates();
	void RefreshPlayerInventoryViews();
	void RefreshSlotGroups();
	void RefreshActionBar();
	void RefreshGearSlots();
	void SetGearSlotViewModel(
		URpgEquipmentSlotWidget* GearSlotWidget,
		ERpgEquipmentSlot EquipmentSlot,
		bool bBagSlot);
	bool RoutePayloadToGearSlot(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed);
	bool RoutePayloadToCarrySlot(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed);
	bool RoutePayloadToActionBar(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed);
	URpgInventorySlotGroupViewModel* FindEquipmentProvidedContentGroup(ERpgEquipmentSlot SourceEquipmentSlot) const;
	void CollectStandaloneContentGroupWidgets(TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const;
	URpgInventoryCarrySlotWidget* ResolveCarrySlotWidget(UWidget* BoundWidget, FName BindingName, bool bLogFailure) const;
	void UpdateControllerCarryDragVisual(
		const FRpgInventoryDragPayload& Payload,
		URpgInventoryCarrySlotWidget* CarrySlotWidget);
	URpgInventoryCarrySlotWidget* FindControllerPreviewCarrySlot() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	mutable TSet<FName> ReportedInvalidPlayerBindings;
};
