#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"

#include "RpgPlayerInventoryPaneWidget.generated.h"

class APlayerController;
class URpgActionBarTileView;
class URpgEquipmentSlotWidget;
class URpgInventoryCarrySlotWidget;
class URpgInventoryPanelNavigationCoordinator;
class URpgInventorySpatialGridWidget;
class URpgInventorySlotGroupViewModel;
class URpgInventorySlotGroupWidget;
class URpgPlayerInventoryViewModel;
class UWidget;

/**
 * Passive, reusable presentation of the complete owning-player inventory.
 *
 * The pane owns one stable read-only aggregate view model and the authored Gear, Carry, Content, and Quickbar
 * leaves. Its activatable host supplies the one screen-scoped interaction context and remains the exclusive owner of
 * payload routing, CommonUI actions, focus policy, modals, back handling, and gameplay mutation.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgPlayerInventoryPaneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryPaneWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact read-only PropertyPath MVVM source name authored in CUI_PlayerInventoryPane. */
	static const FName PlayerInventoryViewModelSourceName;

	/** Native notification requesting a focus-preserving navigation registry rebuild by the activatable host. */
	DECLARE_MULTICAST_DELEGATE(FOnNavigationPanelsChanged);
	FOnNavigationPanelsChanged OnNavigationPanelsChanged;

	/** Stable native-owned aggregate projection used by CUI_PlayerInventoryPane's PropertyPath source. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player Pane")
	URpgPlayerInventoryViewModel* GetPlayerInventoryViewModel() const
	{
		return PlayerInventoryViewModel;
	}

	/** Binds the owning player and complete host-owned interaction context; repeated equivalent calls are safe. */
	void BindPlayerInventory(
		APlayerController* PlayerController,
		const FRpgInventoryScreenPresentationContext& InContext,
		FName InNavigationPanelPrefix = TEXT("Player"));

	/** Releases gameplay listeners, leaf bindings, previews, and host references without destroying the stable VM. */
	void ReleaseInventoryPresentation();

	/** Refreshes the screen-owned interaction objects forwarded to all currently bound leaves. */
	void SetInteractionContext(
		const FRpgInventoryScreenPresentationContext& InContext,
		FName InNavigationPanelPrefix = TEXT("Player"));

	/** Registers Gear, Carry, Content, and Quickbar panels under the configured Player.* namespace. */
	void RegisterNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator);

	/** Appends every currently visible player spatial grid for host-owned pointer routing and cleanup. */
	void AppendSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const;

	/** Resolves the visible Gear, Carry, or Quickbar target under one screen-space ghost center. */
	bool ResolveNonSpatialDropTarget(
		FVector2D GhostCenterScreenPosition,
		UWidget*& OutTarget) const;

	/** Applies preview or commit behavior only to a target previously resolved by this pane. */
	bool ApplyPayloadToNonSpatialDropTarget(
		UWidget* Target,
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit);

	/** Clears all Gear, Carry, and Quickbar external preview state. */
	void ClearExternalDragPreviews();

	/** Returns the active Carry preview center used by the host-owned controller drag ghost. */
	bool ResolveControllerDragVisualAnchor(
		FVector2D& OutAnchorScreenPosition) const;

	/** Refreshes non-spatial semantic presentation after the host interaction session changes. */
	void RefreshInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest);

	/** Preferred standalone focus target, using Pockets whenever its spatial grid is currently available. */
	UWidget* GetPreferredFocusTarget() const;

	/** Stable current Pockets panel id used as the host navigator's initial panel. */
	FName GetPreferredNavigationPanelId() const;

	/** Compact runtime summary for diagnosing authored bindings and projected list counts. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player Pane|Debug")
	FString GetPlayerInventoryWidgetDebugSummary() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** Required freely placed Pockets spatial host. */
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

	/** Required static host for the equipped backpack's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Backpack = nullptr;

	/** Required static host for the equipped belt's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Belt = nullptr;

	/** Required static host for the equipped pouch's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_Pouch = nullptr;

	/** Required static host for the equipped resource bag's optional runtime container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySlotGroupWidget> Content_ResourceBag = nullptr;

	/** Required 1..8 Quickbar preview/drop target. */
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
	void RefreshCarrySlotPresentations();
	void ForwardInteractionContextToChildren();
	void ReleasePlayerInventoryChildPresentations();
	void SetGearSlotViewModel(
		URpgEquipmentSlotWidget* GearSlotWidget,
		ERpgEquipmentSlot EquipmentSlot,
		bool bBagSlot);
	URpgInventorySlotGroupViewModel* FindEquipmentProvidedContentGroup(
		ERpgEquipmentSlot SourceEquipmentSlot) const;
	void CollectStandaloneContentGroupWidgets(
		TArray<URpgInventorySlotGroupWidget*>& OutWidgets) const;
	void CollectCarrySlotWidgets(
		TArray<URpgInventoryCarrySlotWidget*>& OutWidgets) const;
	void CollectGearSlotWidgets(
		TArray<URpgEquipmentSlotWidget*>& OutWidgets) const;
	URpgInventoryCarrySlotWidget* FindControllerPreviewCarrySlot() const;
	FName MakePanelId(const TCHAR* Suffix) const;

	/** Stable presentation-only projection; never authoritative, replicated, or replaced during pooling. */
	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	FRpgInventoryScreenPresentationContext InteractionContext;
	FName NavigationPanelPrefix = TEXT("Player");
	bool bInventoryPresentationBound = false;
	mutable TSet<FName> ReportedInvalidPlayerBindings;
};
