#pragma once

#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryNestedContainerViewModel.h"

#include "RpgInventoryNestedContainerDetailWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryManagerComponent;
class URpgInventorySpatialGridWidget;

/**
 * Reusable detail panel that binds one nested-container VM and one existing spatial grid to the same graph handle.
 *
 * Blueprint supplies chrome, breadcrumb buttons, search input, and open/close animation. Gameplay truth remains in
 * URpgInventoryManagerComponent; this widget only forwards view-model bindings and render-opacity hints.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryNestedContainerDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryNestedContainerDetailWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Uses an externally supplied VM, for example an MVVM source configured on a screen Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	void SetNestedContainerViewModel(URpgInventoryNestedContainerViewModel* InViewModel);

	/** Opens one declared compartment by persistent owner item id. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool OpenContainerByItemId(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemId ItemId,
		FName LocalContainerId = NAME_None);

	/** Opens one validated graph address and binds SpatialGrid exactly to that handle. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool OpenContainerHandle(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryContainerHandle ContainerHandle);

	/** Navigates to one current breadcrumb index without changing the filter query. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool NavigateToBreadcrumb(int32 BreadcrumbIndex);

	/** Clears the detail binding. Blueprint may use the presentation event to play its close animation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	void CloseContainer();

	/** Applies a presentation-only query; spatial items remain at their original replicated coordinates. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container|Filter")
	void SetFilterQuery(const FText& NewFilterQuery);

	/** Assigns the screen-local interaction coordinator also used by the surrounding inventory screen. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current nested-container VM, created lazily when no external source was supplied. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	URpgInventoryNestedContainerViewModel* GetNestedContainerViewModel() const { return NestedContainerViewModel.Get(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Notifies Blueprint after the native grid, breadcrumb, and dimming presentation has been synchronized. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Nested Container", meta = (DisplayName = "On Nested Container Detail Changed"))
	void BP_OnNestedContainerDetailChanged(URpgInventoryNestedContainerViewModel* ViewModel);

	/** Existing spatial presenter reused for this exact item-owned container. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialGridWidget> SpatialGrid = nullptr;

	/** Opacity applied only to non-matching item overlays. Coordinates, hit targets, and entry VMs remain unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Nested Container|Filter", meta = (ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float NonMatchOpacity = 0.25f;

private:
	UFUNCTION()
	void HandlePresentationChanged();

	void EnsureViewModel();
	void RefreshNativePresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryNestedContainerViewModel> NestedContainerViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	bool bOwnsViewModel = false;
};
