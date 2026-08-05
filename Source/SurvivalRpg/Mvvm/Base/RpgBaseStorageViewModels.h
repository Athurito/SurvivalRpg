#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgBaseStorageViewModels.generated.h"

class UTexture2D;
class URpgBaseStorageStationComponent;
class URpgInventoryItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgBaseStorageViewModelListChanged);

/** Local-only ordering used by terminal presentation; it never writes replicated resource SortIndex values. */
UENUM(BlueprintType)
enum class ERpgBaseResourceLocalSortMode : uint8
{
	/** Preserve the replicated order supplied by URpgBaseStorageComponent. */
	ReplicatedOrder,

	/** Sort by localized item display name. */
	Name,

	/** Sort by the broad static item category. */
	Category,

	/** Sort by the currently stored unit count. */
	StoredCount,

	/** Sort by remaining capacity. */
	FreeCapacity,

	/** Sort by normalized storage fill. */
	FillRatio
};

/** Aggregate read-only health summary for the terminal's allowed material domain. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageLocalSummary
{
	GENERATED_BODY()

	/** Number of resource rows allowed by the current station before local search/category filtering. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 TotalResourceCount = 0;

	/** Number of resource rows visible after local search and category filtering. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 VisibleResourceCount = 0;

	/** Sum of stored units across the allowed material domain. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int64 TotalStoredUnits = 0;

	/** Sum of capacity across the allowed material domain. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int64 TotalCapacity = 0;

	/** Shared Materials-domain capacity points currently occupied by authoritative resource rows. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 UsedCapacityPoints = 0;

	/** Shared Materials-domain capacity-point limit after current progression upgrades. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 MaterialCapacityPoints = 0;

	/** Number of rows at or above the local capacity-warning threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 CapacityWarningCount = 0;

	/** Number of rows whose replicated count has reached their capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	int32 FullResourceCount = 0;

	/** Shared used/capacity points when available, otherwise TotalStoredUnits / TotalCapacity; clamped to 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	float AggregateFillRatio = 0.0f;

	/** True when at least one allowed resource has reached the warning threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Summary")
	bool bHasCapacityWarning = false;

	bool operator==(const FRpgBaseStorageLocalSummary& Other) const
	{
		return TotalResourceCount == Other.TotalResourceCount &&
			VisibleResourceCount == Other.VisibleResourceCount &&
			TotalStoredUnits == Other.TotalStoredUnits &&
			TotalCapacity == Other.TotalCapacity &&
			UsedCapacityPoints == Other.UsedCapacityPoints &&
			MaterialCapacityPoints == Other.MaterialCapacityPoints &&
			CapacityWarningCount == Other.CapacityWarningCount &&
			FullResourceCount == Other.FullResourceCount &&
			AggregateFillRatio == Other.AggregateFillRatio &&
			bHasCapacityWarning == Other.bHasCapacityWarning;
	}

	bool operator!=(const FRpgBaseStorageLocalSummary& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * One resource row projected from the shared base storage pool.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgBaseResourceEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this UI row from the latest replicated base-resource entry. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void InitializeFromResourceEntry(const FRpgBaseResourceEntryView& Entry);

	/** Material definition represented by this row. Use this for withdraw/sort commands. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	TSubclassOf<URpgInventoryItemDefinition> GetItemDefinition() const { return ItemDefinition; }

	/** Localized player-facing name used by search and presentation. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	FText GetDisplayName() const { return DisplayName; }

	/** Broad static item category used by local filters. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	ERpgInventoryItemCategory GetItemCategory() const { return ItemCategory; }

	/** Current replicated stored amount projected into this row. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	int32 GetCount() const { return Count; }

	/** Current replicated capacity projected into this row. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	int32 GetCapacity() const { return Capacity; }

	/** Remaining capacity projected into this row. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	int32 GetFreeCapacity() const { return FreeCapacity; }

	/** Normalized 0..1 fill used by progress bars and local sorting. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	float GetFillRatio() const { return FillRatio; }

	/** Replicated order key retained for deterministic local-sort fallback. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	int32 GetSortIndex() const { return SortIndex; }

	/** True at or above 80% capacity; presentation-only and derived from replicated count/capacity. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	bool HasCapacityWarning() const { return bHasCapacityWarning; }

protected:
	/** Material item definition represented by this row. Static definition data; UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Player-facing resource name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Broad category read from static item traits; None means the definition has no traits fragment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryItemCategory ItemCategory = ERpgInventoryItemCategory::None;

	/** Optional icon read from item UIData. Widgets should load it lazily. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current stored amount in the shared base pool. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	int32 Count = 0;

	/** Current capacity for this material after base and station bonuses. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	int32 Capacity = 0;

	/** Remaining free capacity for this resource. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	int32 FreeCapacity = 0;

	/** Normalized Count / Capacity value for progress bars. Zero when capacity is zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	float FillRatio = 0.0f;

	/** Shared replicated order key used by terminal and storage-unit lists. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	int32 SortIndex = 0;

	/** True when no resource units are currently stored. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	bool bIsEmpty = true;

	/** True when Count is at or above Capacity and Capacity is non-zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	bool bIsFull = false;

	/** True when Capacity is positive and FillRatio is at least 0.8; UI-only warning state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	bool bHasCapacityWarning = false;
};

/**
 * ViewModel for terminal and storage-unit resource lists.
 *
 * The base storage component remains the replicated gameplay truth. This VM listens to
 * Rpg.BaseStorage.Message.Changed and rebuilds UI rows when resources change, including
 * crafting-station auto-deposit while the terminal screen is already open.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgBaseStorageViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Observes one base storage component. Empty AllowedResources means terminal/full access. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void BindBaseStorage(URpgBaseStorageComponent* InBaseStorage, const TArray<TSubclassOf<URpgInventoryItemDefinition>>& InAllowedResources);

	/** Convenience binding for terminal or storage-unit payloads. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void BindBaseStorageStation(URpgBaseStorageStationComponent* Station);

	/** Clears bindings and resource rows. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void UnbindBaseStorage();

	/** Rebuilds resource rows from current replicated base storage state. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void RefreshResources();

	/** Updates the local resource filter without changing the observed storage component. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void SetAllowedResources(const TArray<TSubclassOf<URpgInventoryItemDefinition>>& InAllowedResources);

	/** Sets a case-insensitive local name search without changing gameplay or replicated storage order. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void SetSearchText(FText InSearchText);

	/** Sets a local broad-category filter. None shows all allowed resources. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void SetCategoryFilter(ERpgInventoryItemCategory InCategoryFilter);

	/** Sets local row ordering. ReplicatedOrder restores the server-provided order without writing it. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|ViewModel")
	void SetLocalSort(ERpgBaseResourceLocalSortMode InSortMode, bool bInSortDescending);

	/** Resource rows for ListView/TileView SetListItems. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	TArray<URpgBaseResourceEntryViewModel*> GetResources() const;

	/** Observed shared base resource pool. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	URpgBaseStorageComponent* GetBaseStorage() const { return ObservedBaseStorage.Get(); }

	/** Current local search term. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	FText GetSearchText() const { return SearchText; }

	/** Current local broad-category filter. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	ERpgInventoryItemCategory GetCategoryFilter() const { return CategoryFilter; }

	/** Current local ordering mode. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	ERpgBaseResourceLocalSortMode GetLocalSortMode() const { return LocalSortMode; }

	/** True when the current local ordering runs from high to low. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	bool IsLocalSortDescending() const { return bSortDescending; }

	/** Aggregate storage health before local search/category filtering plus the visible-row count. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	FRpgBaseStorageLocalSummary GetSummary() const { return Summary; }

	/** Fired whenever widgets should refresh their resource list. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|ViewModel")
	FRpgBaseStorageViewModelListChanged OnResourcesChanged;

protected:
	/** Shared base storage component this VM observes. UI-read-only; gameplay remains server-authoritative. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgBaseStorageComponent> ObservedBaseStorage = nullptr;

	/** Optional local material filter for storage units. Empty means show every resource row. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<URpgInventoryItemDefinition>> AllowedResources;

	/** Projected, reusable resource row viewmodels. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> Resources;

	/** Number of currently visible resource rows after filtering. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	int32 ResourceCount = 0;

	/** Case-insensitive local display-name search. Presentation-only and retained across pooled rebinds. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	FText SearchText;

	/** Local static-category filter. None shows all allowed resources. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryItemCategory CategoryFilter = ERpgInventoryItemCategory::None;

	/** Local presentation order; never forwarded to URpgBaseStorageComponent::ApplyResourceSort. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	ERpgBaseResourceLocalSortMode LocalSortMode = ERpgBaseResourceLocalSortMode::ReplicatedOrder;

	/** Local sort direction. False is ascending. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	bool bSortDescending = false;

	/** Derived local summary used by terminal capacity and warning presentation. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage", meta = (AllowPrivateAccess = "true"))
	FRpgBaseStorageLocalSummary Summary;

private:
	void RegisterBaseStorageMessageListener();
	void UnregisterBaseStorageMessageListener();
	void RequestRefreshResources();
	void ExecuteQueuedRefreshResources();
	void CancelQueuedRefreshResources();
	void RebuildResources();
	void HandleBaseStorageChanged(FGameplayTag Channel, const FRpgBaseResourceChangeMessage& Message);

	FGameplayMessageListenerHandle BaseStorageChangedHandle;
	FRpgViewModelInvalidationQueue RefreshResourcesQueue;

	/** Unfiltered row cache retaining stable row objects while local filters hide and reveal them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgBaseResourceEntryViewModel>> AllResourceRows;
};
