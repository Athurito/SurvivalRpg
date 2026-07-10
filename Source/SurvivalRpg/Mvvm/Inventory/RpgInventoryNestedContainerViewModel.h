#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgInventoryNestedContainerViewModel.generated.h"

class URpgInventoryEntryViewModel;
class URpgInventoryManagerComponent;
class URpgInventoryPanelViewModel;

/** One navigable segment from an inventory root to the currently open item-owned compartment. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryContainerBreadcrumb
{
	GENERATED_BODY()

	/** Player-facing root, item, and compartment label. Presentation-only and safe for breadcrumb buttons. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container")
	FText Label;

	/** Exact graph address opened when this breadcrumb is selected. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container")
	FRpgInventoryContainerHandle Handle;

	/** Item that owns this compartment, or invalid for a root breadcrumb. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container")
	FRpgInventoryItemId OwnerItemId;

	/** True only for the currently displayed breadcrumb. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container")
	bool bCurrent = false;
};

/** Search presentation for one entry; filtering never removes it or changes its replicated grid placement. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryEntryFilterPresentation
{
	GENERATED_BODY()

	/** Persistent item identity represented by this presentation row. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container|Filter")
	FRpgInventoryItemId ItemId;

	/** Current replicated entry id used to address the existing spatial overlay widget. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container|Filter")
	FGuid EntryId;

	/** True when display name, short name, definition, category, description, or tags match every query token. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container|Filter")
	bool bMatchesFilter = true;

	/** UI hint for rendering a non-match with reduced opacity while preserving its exact grid coordinates. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Nested Container|Filter")
	bool bDimmed = false;
};

/** Fired after open/close, breadcrumb, entry, or search presentation changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgInventoryNestedContainerPresentationChanged);

/**
 * Reusable read-only presenter for one root or item-owned inventory graph container.
 *
 * It resolves persistent item identities through the authoritative manager, owns exactly one filtered panel view
 * model, and derives breadcrumbs/search hints without ever mutating inventory entries or grid coordinates.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryNestedContainerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Opens one compartment owned by ItemId. None selects the first valid compartment declared by the item fragment. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool OpenContainerByItemId(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemId ItemId,
		FName LocalContainerId = NAME_None);

	/** Opens one validated root or item-owned handle. Root handles are supported for breadcrumb navigation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool OpenContainerHandle(
		URpgInventoryManagerComponent* Inventory,
		FRpgInventoryContainerHandle ContainerHandle);

	/** Navigates to a previously derived breadcrumb while keeping the same inventory and filter query. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	bool NavigateToBreadcrumb(int32 BreadcrumbIndex);

	/** Stops observing gameplay state and clears the current detail presentation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container")
	void CloseContainer();

	/** Updates case-insensitive token matching. Non-matches remain in place and are only marked dimmed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container|Filter")
	void SetFilterQuery(const FText& NewFilterQuery);

	/** Clears search dimming without rebuilding or moving inventory entries. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Nested Container|Filter")
	void ClearFilterQuery();

	/** Returns whether one replicated entry should be dimmed by the current query. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container|Filter")
	bool IsEntryDimmed(FGuid EntryId) const;

	/** Entry ids currently marked as non-matches for direct spatial-grid presentation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container|Filter")
	TArray<FGuid> GetDimmedEntryIds() const;

	/** The single panel VM filtered to the active container handle. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	URpgInventoryPanelViewModel* GetPanelViewModel() const { return PanelViewModel.Get(); }

	/** True while this presenter has a validated inventory/container binding. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	bool IsOpen() const { return bIsOpen; }

	/** Exact handle currently projected by the panel and spatial grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	FRpgInventoryContainerHandle GetOpenContainerHandle() const { return ActiveContainerHandle; }

	/** Current player-facing detail title. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	FText GetTitle() const { return Title; }

	/** Current ordered root-to-detail breadcrumb path. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	TArray<FRpgInventoryContainerBreadcrumb> GetBreadcrumbs() const { return Breadcrumbs; }

	/** Replicated inventory currently observed by this UI-only presenter. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Nested Container")
	URpgInventoryManagerComponent* GetObservedInventory() const { return ObservedInventory.Get(); }

	/** Fired after any presentation field changes so native/BP widgets can refresh binding and opacity. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Nested Container")
	FRpgInventoryNestedContainerPresentationChanged OnPresentationChanged;

protected:
	virtual void BeginDestroy() override;

	/** True while a validated container is bound. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	bool bIsOpen = false;

	/** Persistent owner item id, or invalid while a root breadcrumb is displayed. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryItemId OpenOwnerItemId;

	/** Exact graph address currently projected by PanelViewModel and the spatial grid. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryContainerHandle ActiveContainerHandle;

	/** Player-facing title derived from the current breadcrumb. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	FText Title;

	/** Ordered root-to-current navigation path. Handles are derived from current authoritative graph state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgInventoryContainerBreadcrumb> Breadcrumbs;

	/** Current UI-only case-insensitive query. It never changes Entries or their placement. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container|Filter", meta = (AllowPrivateAccess = "true"))
	FText FilterQuery;

	/** Match/dim presentation for the entries still present in PanelViewModel. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container|Filter", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgInventoryEntryFilterPresentation> EntryFilterPresentation;

	/** Reused panel presenter bound to exactly ActiveContainerHandle while open. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Nested Container", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryPanelViewModel> PanelViewModel = nullptr;

private:
	UFUNCTION()
	void HandlePanelEntriesChanged();

	void EnsurePanelViewModel();
	bool RevalidateOpenContainerBinding();
	void RefreshFilterPresentation();
	bool BuildBreadcrumbs(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryContainerHandle& ContainerHandle,
		TArray<FRpgInventoryContainerBreadcrumb>& OutBreadcrumbs,
		FText& OutTitle) const;
	FText ResolveBreadcrumbLabel(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryContainerHandle& ContainerHandle) const;
	bool DoesEntryMatchQuery(const URpgInventoryEntryViewModel* Entry, const TArray<FString>& QueryTokens) const;
	void BroadcastPresentationFields();

	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedInventory;
	bool bSuppressPanelEntryCallback = false;
};
