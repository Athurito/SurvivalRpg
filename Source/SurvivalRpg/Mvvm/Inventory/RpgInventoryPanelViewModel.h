#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"

#include "RpgInventoryPanelViewModel.generated.h"

class URpgInventoryItemFragment;

/** Broadcast when the panel rebuilt its UI entry list and list widgets should refresh their items. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgInventoryPanelEntriesChanged);

/**
 * Inventory panel model that listens to Lyra-style GameplayMessages and projects replicated entries for widgets.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryPanelViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	URpgInventoryPanelViewModel();

	/** Starts observing one replicated inventory component. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void BindInventory(URpgInventoryManagerComponent* InInventory);

	/** Observes only one exact root or item-owned grid within an inventory graph. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void BindInventoryContainer(URpgInventoryManagerComponent* InInventory, FRpgInventoryContainerHandle InContainerHandle);

	/** Stops observing the current inventory and clears entry models. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void UnbindInventory();

	/** Rebuilds entry models from the current replicated inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void RefreshEntries();

	/** Current entry models, including empty capacity slots for finite inventories. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	TArray<URpgInventoryEntryViewModel*> GetEntries() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	URpgInventoryManagerComponent* GetObservedInventory() const { return ObservedInventory.Get(); }

	/** Exact container filter, or invalid when the panel projects the complete inventory graph. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FRpgInventoryContainerHandle GetContainerFilter() const { return ContainerFilter; }

	/** Fired after Entries has been rebuilt so BP widgets can call SetListItems and RequestRefresh. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel")
	FRpgInventoryPanelEntriesChanged OnEntriesChanged;

protected:
	virtual void BeginDestroy() override;

	/** Current entry models in replicated shared order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventoryEntryViewModel>> Entries;

	/** Number of occupied inventory entries. Stack counts do not increase this unless they create a new stack entry. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	int32 UsedEntries = 0;

	/** Maximum available entries, or INDEX_NONE when the observed inventory is unlimited. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	int32 MaxEntries = 0;

	/** Remaining free entries, or INDEX_NONE when the observed inventory is unlimited. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	int32 FreeEntries = 0;

	/** True when the observed inventory does not enforce an entry limit. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	bool bIsUnlimited = false;

	/** Display-ready capacity text, for example "12 / 24" or "Unlimited". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Capacity", meta = (AllowPrivateAccess = "true"))
	FText CapacityText;

	/** Fragment presenter mapping. Designers may extend this for durability, affixes, sockets, and similar fragments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>> FragmentViewModelClasses;

private:
	void RequestRefreshEntries();
	void ExecuteQueuedRefreshEntries();
	void CancelQueuedRefreshEntries();
	void RefreshCapacityFields(URpgInventoryManagerComponent* Inventory);
	void RegisterInventoryMessageListener();
	void UnregisterInventoryMessageListener();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);

	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedInventory;

	/** Optional exact graph address used by nested-container and storage detail panels. */
	UPROPERTY(Transient)
	FRpgInventoryContainerHandle ContainerFilter;

	FGameplayMessageListenerHandle InventoryChangedHandle;
	FRpgViewModelInvalidationQueue RefreshEntriesQueue;
};
