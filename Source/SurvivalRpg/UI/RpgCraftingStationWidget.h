#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"
#include "TimerManager.h"

#include "RpgCraftingStationWidget.generated.h"

class UCheckBox;
class UCommonLazyImage;
class UCommonListView;
class UCommonTextBlock;
class UUserWidget;
class URpgCraftingActionButtonWidget;
class URpgCraftingJobViewModel;
class URpgCraftingRecipeDefinition;
class URpgCraftingStationComponent;
class URpgCraftingStationViewModel;
class URpgInventoryManagerComponent;
class URpgInventorySlotGroupPanelWidget;
class URpgInventorySpatialGridWidget;
class URpgInventorySpatialPaneWidget;
class URpgInventoryUiActionComponent;
class URpgPlayerInventoryViewModel;

/**
 * Native CommonUI presenter for one crafting-station interaction.
 *
 * This screen validates the explicit station payload, owns stable read-only crafting and player-layout view models,
 * and connects the authored recipe/details/player/output leaves. Every gameplay mutation is forwarded as a typed
 * intent through the owning controller's URpgInventoryUiActionComponent.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgCraftingStationWidget
	: public URpgInventoryInteractionScreenWidget
	, public IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	/** Current validated payload, or null after rejection/deactivation. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Screen")
	URpgCraftingStationScreenPayload* GetCraftingScreenPayload() const
	{
		return CraftingScreenPayload.Get();
	}

	/** Stable screen-owned crafting projection retained across CommonUI pooling. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Screen")
	URpgCraftingStationViewModel* GetCraftingViewModel() const
	{
		return CraftingViewModel.Get();
	}

	/** Stable screen-owned player-layout projection used by the authored groups panel. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Screen")
	URpgPlayerInventoryViewModel* GetCraftingPlayerInventoryViewModel() const
	{
		return PlayerInventoryViewModel.Get();
	}

	/** Exact station-output pane used by the canonical authored screen. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Screen")
	URpgInventorySpatialPaneWidget* GetOutputInventoryPane() const
	{
		return OutputInventoryPane.Get();
	}

	/** Exact root container projected by OutputInventoryPane. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Screen")
	FRpgInventoryContainerHandle GetOutputPaneContainerHandle() const
	{
		return OutputPaneContainerHandle;
	}

	/** Local diagnostic generation incremented once for every complete presentation bind. */
	uint32 GetCraftingPresentationBindGeneration() const
	{
		return CraftingPresentationBindGeneration;
	}

	/** Queues the currently selected recipe and quantity through the server-authoritative action component. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestCraftSelectedRecipe();

	/** Cancels the exact replicated job represented by one screen-owned job row. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestCancelCraftJob(URpgCraftingJobViewModel* JobViewModel);

	/** Requests pause or resume according to the latest replicated station state. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestToggleCraftingPause();

	/** Requests the station's server-authoritative output auto-deposit toggle. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestSetCraftingOutputAutoDepositEnabled(bool bEnabled);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	//~IRpgUIScreenPayloadReceiver interface
	virtual void ReceiveScreenPayload_Implementation(UObject* Payload) override;
	//~End of IRpgUIScreenPayloadReceiver interface

	virtual void BindInventoryScreenPresentation() override;
	virtual void UnbindInventoryScreenPresentation() override;
	virtual void ForwardInventoryInteractionContextToChildren() override;
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;

	/**
	 * Authored player-side spatial groups. Dynamic group children reflect equipped containers, while this host and
	 * its placement remain static and visible in CUI_CraftingStationSpatial.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> PlayerGroupsPanel = nullptr;

	/** Authored reusable pane bound to the station output root. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialPaneWidget> OutputInventoryPane = nullptr;

	/** Authored recipe rows, populated from stable recipe view models. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> RecipeList = nullptr;

	/** Authored ingredient rows for the selected quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> IngredientList = nullptr;

	/** Authored replicated queue rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> CraftingJobsList = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecipeNameText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecipeDescriptionText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CraftTimeText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CraftQuantityText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> RecipeIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> CraftButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> PauseButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMinusButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityPlusButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityFiveButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityTenButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMaxButton = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> AutoDepositCheckBox = nullptr;

	/** CommonUI action row for the selected recipe submit intent. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input|Crafting",
		meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle CraftInputAction;

	/** CommonUI action row for the station pause/resume intent. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input|Crafting",
		meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TogglePauseInputAction;

	/** UI-only progress refresh cadence. Replicated job identity and authoritative completion remain station-owned. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Crafting|Presentation",
		meta = (ClampMin = "0.05", UIMin = "0.05", Units = "s"))
	float JobProgressRefreshInterval = 0.25f;

private:
	void ApplyCraftingScreenPayload(UObject* Payload);
	bool IsPayloadCoherent(
		const URpgCraftingStationScreenPayload* Payload) const;
	bool BindCraftingContext();
	void ResetCraftingContext();
	void EnsureCraftingViewModels();
	void BindViewModelDelegates();
	void UnbindViewModelDelegates();
	void BindAuthoredControlEvents();
	void UnbindAuthoredControlEvents();
	void RefreshPlayerGroups();
	void RefreshRecipeItems();
	void RefreshSelectedRecipePresentation();
	void RefreshJobItems();
	void RefreshCraftingActionAvailability();
	void ConfigureQuickTransferRoutes();
	void StartJobProgressRefresh();
	void StopJobProgressRefresh();
	void EnsureDefaultCraftingActionRows();
	void RegisterCraftingActionBindings();
	void UnregisterCraftingActionBindings();
	URpgInventoryUiActionComponent* ResolveInventoryUiActionComponent() const;

	void HandleRecipeSelectionChanged(UObject* SelectedItem);
	void HandleJobEntryGenerated(UUserWidget& EntryWidget);
	void HandleJobEntryReleased(UUserWidget& EntryWidget);
	void HandleCraftClicked();
	void HandlePauseClicked();
	void HandleQuantityMinusClicked();
	void HandleQuantityPlusClicked();
	void HandleQuantityFiveClicked();
	void HandleQuantityTenClicked();
	void HandleQuantityMaxClicked();
	void HandleJobProgressTimer();

	UFUNCTION()
	void HandleAutoDepositCheckStateChanged(bool bChecked);

	UFUNCTION()
	void HandleRecipesChanged();

	UFUNCTION()
	void HandleSelectedRecipeDetailsChanged();

	UFUNCTION()
	void HandleJobsChanged();

	UFUNCTION()
	void HandlePlayerSlotGroupsChanged();

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationScreenPayload> CraftingScreenPayload = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> OutputInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationComponent> CraftingStation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RequestingActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationViewModel> CraftingViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> PlayerInventoryViewModel = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle OutputPaneContainerHandle;

	bool bCraftingContextBound = false;
	bool bApplyingAutoDepositCheckState = false;
	uint32 CraftingPresentationBindGeneration = 0;
	FTimerHandle JobProgressTimer;
	FUIActionBindingHandle CraftActionBinding;
	FUIActionBindingHandle TogglePauseActionBinding;
};
