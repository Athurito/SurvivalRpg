#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgBaseStorageViewModels.generated.h"

class UTexture2D;
class URpgBaseStorageStationComponent;
class URpgInventoryItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgBaseStorageViewModelListChanged);

/**
 * One resource row projected from the shared base storage pool.
 */
UCLASS(BlueprintType)
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

protected:
	/** Material item definition represented by this row. Static definition data; UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Player-facing resource name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Base Storage|Resource", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

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

	/** Resource rows for ListView/TileView SetListItems. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	TArray<URpgBaseResourceEntryViewModel*> GetResources() const;

	/** Observed shared base resource pool. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|ViewModel")
	URpgBaseStorageComponent* GetBaseStorage() const { return ObservedBaseStorage.Get(); }

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
};
