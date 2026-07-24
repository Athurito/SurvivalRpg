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
struct FRpgInventoryScreenPresentationContext;

/**
 * Native base for the player inventory screen.
 *
 * The native presenter owns exactly one screen-scoped player-inventory view model. The authored MVVM source reads
 * that stable instance through GetPlayerInventoryViewModel and cannot replace it. The presenter then wires the
 * read-only projection into named Blueprint widgets so the screen does not need manual slot loops or per-entry
 * coordinator setup. Gameplay state stays in inventory, layout, equipment, and actionbar components; this widget
 * only connects view models to CommonUI views.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgPlayerInventoryWidget : public URpgInventoryInteractionScreenWidget
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact read-only PropertyPath MVVM source name authored in CUI_PlayerInventory. */
	static const FName PlayerInventoryViewModelSourceName;

	/**
	 * Native-owned aggregate MVVM projection and the only valid source for CUI_PlayerInventory's PropertyPath.
	 * Its UObject Outer is this widget, it exists before MVVM initializes, and it remains stable across pooling.
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

	/** Required freely placed Pockets spatial host; its projected group remains runtime read-only state. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pockets = nullptr;

	/** Required primary-weapon carry presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventoryCarrySlotWidget> Carry_Weapon1 = nullptr;

	/** Required secondary-weapon carry presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventoryCarrySlotWidget> Carry_Weapon2 = nullptr;

	/** Required offhand carry presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventoryCarrySlotWidget> Carry_Offhand = nullptr;

	/** Required static host for the currently equipped backpack's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Backpack = nullptr;

	/** Required static host for the currently equipped belt's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Belt = nullptr;

	/** Required static host for the currently equipped pouch's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pouch = nullptr;

	/** Required static host for the currently equipped resource bag's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_ResourceBag = nullptr;

	/** Required 1..8 actionbar preview/drop target inside the inventory screen. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgActionBarTileView> ActionBarTileView = nullptr;

	/** Required fixed head armor slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Head = nullptr;

	/** Required fixed chest armor slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Chest = nullptr;

	/** Required fixed hands armor slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Hands = nullptr;

	/** Required fixed legs armor slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Legs = nullptr;

	/** Required fixed feet armor slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Feet = nullptr;

	/** Required backpack equipment slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Backpack = nullptr;

	/** Required belt equipment slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Belt = nullptr;

	/** Required pouch equipment slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_Pouch = nullptr;

	/** Required resource-bag equipment slot presenter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgEquipmentSlotWidget> Gear_ResourceBag = nullptr;

private:
	UFUNCTION()
	void HandleGearSlotsChanged();

	UFUNCTION()
	void HandleSlotGroupsChanged();

	UFUNCTION()
	void HandleActionBarSlotsChanged();

	void EnsurePlayerInventoryViewModel();
	bool ValidatePlayerInventoryViewModelMvvmContract() const;
	void BindViewModelDelegates();
	void RefreshPlayerInventoryViews();
	void RefreshSlotGroups();
	void RefreshActionBar();
	void RefreshGearSlots();
	FRpgInventoryScreenPresentationContext MakeInventoryScreenPresentationContext();
	void ReleasePlayerInventoryChildPresentations();
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
	void CollectCarrySlotWidgets(TArray<URpgInventoryCarrySlotWidget*>& OutWidgets) const;
	void CollectGearSlotWidgets(TArray<URpgEquipmentSlotWidget*>& OutWidgets) const;
	void UpdateControllerCarryDragVisual(
		const FRpgInventoryDragPayload& Payload,
		URpgInventoryCarrySlotWidget* CarrySlotWidget);
	URpgInventoryCarrySlotWidget* FindControllerPreviewCarrySlot() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	mutable TSet<FName> ReportedInvalidPlayerBindings;
};
