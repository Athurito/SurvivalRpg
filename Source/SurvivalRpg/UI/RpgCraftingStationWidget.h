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
class URpgInventorySpatialGridWidget;
class URpgInventorySpatialPaneWidget;
class URpgInventoryUiActionComponent;
class URpgPlayerInventoryPaneWidget;
class URpgPlayerInventoryViewModel;

/**
 * Native CommonUI presenter for one crafting-station interaction.
 *
 * This screen validates the explicit station payload, owns the stable read-only crafting view model, and connects the
 * authored recipe/details/player-pane/output leaves. Every gameplay mutation is forwarded as a typed intent through
 * the owning controller's URpgInventoryUiActionComponent.
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

	/** Deprecated compatibility accessor; the passive player pane now owns the stable aggregate projection. */
	UFUNCTION(
		BlueprintPure,
		Category = "Crafting|Screen",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use PlayerInventoryPane.GetPlayerInventoryViewModel instead."))
	URpgPlayerInventoryViewModel* GetCraftingPlayerInventoryViewModel() const;

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
#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

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
	virtual FText ResolveQuickTransferDisplayName() const override;

	/**
	 * Complete passive player-inventory pane authored in CUI_CraftingStationSpatial.
	 * The pane owns only read-only presentation state; this activatable screen retains interaction and input ownership.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgPlayerInventoryPaneWidget> PlayerInventoryPane = nullptr;

	/** Authored reusable pane bound to the station output root. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySpatialPaneWidget> OutputInventoryPane = nullptr;

	/** Authored recipe rows, populated from stable recipe view models. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> RecipeList = nullptr;

	/** Authored ingredient rows for the selected quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> IngredientList = nullptr;

	/** Authored replicated queue rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> CraftingJobsList = nullptr;

	/** Required read-only title for the currently selected recipe. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RecipeNameText = nullptr;

	/** Required read-only description for the currently selected recipe. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RecipeDescriptionText = nullptr;

	/** Required read-only formatted craft duration for the current selection. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CraftTimeText = nullptr;

	/** Required read-only formatted requested craft quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CraftQuantityText = nullptr;

	/** Required cosmetic icon for the currently selected recipe. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonLazyImage> RecipeIcon = nullptr;

	/** Required pointer-facing submit control; authority remains in the crafting station. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> CraftButton = nullptr;

	/** Required pointer-facing pause/resume control for the selected server-owned job. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> PauseButton = nullptr;

	/** Required pointer control that decreases the local requested quantity by one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMinusButton = nullptr;

	/** Required pointer control that increases the local requested quantity by one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityPlusButton = nullptr;

	/** Required pointer shortcut that requests five crafts before server validation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityFiveButton = nullptr;

	/** Required pointer shortcut that requests ten crafts before server validation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityTenButton = nullptr;

	/** Required pointer shortcut that selects the locally projected maximum quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMaxButton = nullptr;

	/** Required local preference controlling whether completed output targets base storage. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgCraftingScreenPayloadLifecycleTest;
#endif

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
	void RefreshRecipeItems();
	void RefreshSelectedRecipePresentation();
	void RefreshJobItems();
	void RefreshCraftingActionAvailability();
	void ConfigureQuickTransferRoutes();
	void StartJobProgressRefresh();
	void StopJobProgressRefresh();
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
	void HandlePlayerInventoryPaneNavigationPanelsChanged();

	UFUNCTION()
	void HandleAutoDepositCheckStateChanged(bool bChecked);

	UFUNCTION()
	void HandleRecipesChanged();

	UFUNCTION()
	void HandleSelectedRecipeDetailsChanged();

	UFUNCTION()
	void HandleJobsChanged();

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
	FRpgInventoryContainerHandle OutputPaneContainerHandle;

	bool bCraftingContextBound = false;
	bool bApplyingAutoDepositCheckState = false;
	uint32 CraftingPresentationBindGeneration = 0;
	FTimerHandle JobProgressTimer;
	FUIActionBindingHandle CraftActionBinding;
	FUIActionBindingHandle TogglePauseActionBinding;
};
