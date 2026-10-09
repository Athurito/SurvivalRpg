#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"
#include "TimerManager.h"

#include "RpgCraftingStationWidget.generated.h"

class UCommonLazyImage;
class UCommonListView;
class UCommonTextBlock;
class UUserWidget;
class URpgCraftingActionButtonWidget;
class URpgCraftingCategoryCatalog;
class URpgCraftingRecipeDefinition;
class URpgCraftingStationComponent;
class URpgCraftingStationViewModel;
class URpgInventoryManagerComponent;
class URpgInventorySpatialGridWidget;
class URpgInventoryUiActionComponent;
class URpgPlayerInventoryPaneWidget;
class URpgPlayerInventoryViewModel;

/**
 * Native CommonUI presenter for one crafting-station interaction.
 *
 * This screen validates the explicit station payload, owns the stable read-only crafting view model, and fills the
 * authored lists: categories, recipes with tier sections, materials, preview values, tier options and target chests.
 * Every gameplay mutation is forwarded as a typed intent through the owning controller's URpgInventoryUiActionComponent.
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

	/** Local diagnostic generation incremented once for every complete presentation bind. */
	uint32 GetCraftingPresentationBindGeneration() const
	{
		return CraftingPresentationBindGeneration;
	}

	/** Starts the selected recipe, quantity and target chest as the station's order through the server. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestStartCraftingOrder();

	/** Stops the remaining units of the station's order. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestStopCraftingOrder();

	/** Requests pause or resume according to the latest replicated order state. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestToggleCraftingPause();

	/** Chooses a connected chest as target; while an order runs the server moves its delivery there. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestSelectTargetStorage(FName ContainerId);

	/** Moves the target to the next (Direction > 0) or previous connected chest. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Actions")
	void RequestCycleTargetStorage(int32 Direction);

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
	 * Optional passive player-inventory pane. The canonical crafting screen shows only the station and omits it; output
	 * still reaches the player inventory through quick transfer and Take all. When authored, the pane owns only
	 * read-only presentation state, and this activatable screen retains interaction and input ownership.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgPlayerInventoryPaneWidget> PlayerInventoryPane = nullptr;

	/** Authored recipe list: tier section headers and recipe rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> RecipeList = nullptr;

	/** Authored material rows of the selected recipe. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> IngredientList = nullptr;

	/** Optional category rows: All, collapsible groups and subcategories. Clicks activate a row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> CategoryList = nullptr;

	/** Optional tier filter options, usually inside an authored popup. Clicks set the filter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> TierFilterList = nullptr;

	/** Optional connected chests offered as target, usually inside an authored popup. Clicks choose the target. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> TargetStorageList = nullptr;

	/** Optional key values of the selected recipe next to the preview. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonListView> PreviewStatList = nullptr;

	/** Optional name of the selected recipe; screens may bind the view model instead. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecipeNameText = nullptr;

	/** Optional description of the selected recipe; screens may bind the view model instead. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecipeDescriptionText = nullptr;

	/** Optional chosen quantity. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CraftQuantityText = nullptr;

	/** Optional large preview image of the selected recipe. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> RecipeIcon = nullptr;

	/** Optional header icon of the observed station; collapsed while the station authors none. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> StationIcon = nullptr;

	/** Required main action that starts the selection as the station's order. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> CraftButton = nullptr;

	/** Required pause/resume control of the station's order. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> PauseButton = nullptr;

	/** Optional control that stops the order's remaining units. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> StopOrderButton = nullptr;

	/** Optional toggle that flips the tier order of the recipe list. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> SortDirectionButton = nullptr;

	/** Required control that decreases the chosen quantity by one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMinusButton = nullptr;

	/** Required control that increases the chosen quantity by one. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityPlusButton = nullptr;

	/** Required control that sets the quantity the chests can pay for and the target can hold. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgCraftingActionButtonWidget> QuantityMaxButton = nullptr;

	/** Names, icons and tier names for the category list and tier headers. Presentation data; null uses tag names. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Presentation")
	TObjectPtr<URpgCraftingCategoryCatalog> CategoryCatalog = nullptr;

	/** Entry class for tier section headers in RecipeList; must implement UserObjectListEntry. Recipe rows use the list's entry class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting|Presentation")
	TSubclassOf<UUserWidget> TierSectionEntryClass;

	/** CommonUI action row for the start intent. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input|Crafting",
		meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle CraftInputAction;

	/** CommonUI action row for the pause/resume intent. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input|Crafting",
		meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle TogglePauseInputAction;

	/** Optional CommonUI action row for stopping the order; shown in the action bar while an order exists. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Input|Crafting",
		meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle StopOrderInputAction;

	/** UI-only progress refresh cadence. The order and its completion remain station-owned. */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Crafting|Presentation",
		meta = (ClampMin = "0.05", UIMin = "0.05", Units = "s"))
	float OrderProgressRefreshInterval = 0.25f;

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
	void RefreshCategoryItems();
	void RefreshTierOptionItems();
	void RefreshSelectedRecipePresentation();
	void RefreshStationHeaderPresentation();
	void RefreshCraftingActionAvailability();
	void StartOrderProgressRefresh();
	void StopOrderProgressRefresh();
	void RegisterCraftingActionBindings();
	void UnregisterCraftingActionBindings();
	URpgInventoryUiActionComponent* ResolveInventoryUiActionComponent() const;

	TSubclassOf<UUserWidget> HandleGetRecipeEntryClass(UObject* Item) const;
	bool HandleIsRecipeItemSelectable(UObject* Item) const;
	void HandleRecipeSelectionChanged(UObject* SelectedItem);
	void HandleCategoryItemClicked(UObject* Item);
	void HandleTierOptionClicked(UObject* Item);
	void HandleTargetOptionClicked(UObject* Item);
	void HandleCraftClicked();
	void HandlePauseClicked();
	void HandleStopOrderClicked();
	void HandleSortDirectionClicked();
	void HandleQuantityMinusClicked();
	void HandleQuantityPlusClicked();
	void HandleQuantityMaxClicked();
	void HandleOrderProgressTimer();
	void HandlePlayerInventoryPaneNavigationPanelsChanged();

	UFUNCTION()
	void HandleRecipesChanged();

	UFUNCTION()
	void HandleCategoriesChanged();

	UFUNCTION()
	void HandleTierOptionsChanged();

	UFUNCTION()
	void HandleSelectedRecipeDetailsChanged();

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationScreenPayload> CraftingScreenPayload = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationComponent> CraftingStation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RequestingActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationViewModel> CraftingViewModel = nullptr;

	bool bCraftingContextBound = false;
	uint32 CraftingPresentationBindGeneration = 0;
	FTimerHandle OrderProgressTimer;
	FUIActionBindingHandle CraftActionBinding;
	FUIActionBindingHandle TogglePauseActionBinding;
	FUIActionBindingHandle StopOrderActionBinding;
};
