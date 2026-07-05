#pragma once

#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryViewModels.generated.h"

class UTexture2D;
class URpgInventoryFragment_ItemTraits;
class URpgInventoryFragment_UIData;
class URpgInventoryItemFragment;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventoryEntryViewModel;

/** Broadcast when one slot view model changed its represented item, stack, or empty state. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgInventoryEntryViewModelChanged, URpgInventoryEntryViewModel*, EntryViewModel);

/** Broadcast when the panel rebuilt its UI entry list and list widgets should refresh their items. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgInventoryPanelEntriesChanged);

/**
 * Base class for optional item-fragment presenters used by inventory widgets.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryFragmentViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this presenter from one replicated inventory entry. UI-only; never mutates gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry);

protected:
	/** Replicated item instance this presenter reads from. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Entry id represented by this presenter. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGuid EntryId;
};

/**
 * Presenter for replicated stack data.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryStackFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Current replicated stack count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Stack", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;
};

/**
 * Presenter for gameplay-facing item traits that UI may display or use for filters.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryTraitsFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Broad item category used for UI grouping and sorting. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryItemCategory ItemCategory = ERpgInventoryItemCategory::Misc;

	/** Gameplay tags exposed for UI filters and recipe previews. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ItemTags;

	/** Whether this item is treated as a material for UI grouping and death-drop previews. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	bool bIsMaterial = false;
};

/**
 * Presenter for static UI data such as icon and tooltip text.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryUiDataFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Optional item icon used by inventory-style widgets. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Compact display name for slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** Tooltip text shown in inventory panels. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** UI-only presentation tags such as rarity or item family. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer PresentationTags;
};

/**
 * One UI row/slot generated from an inventory entry.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this entry from replicated inventory data. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void InitializeFromEntry(const FRpgInventoryEntryView& Entry, const TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& FragmentViewModelClasses);

	/** Initializes a UI-only empty slot. It does not represent a replicated inventory entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	void InitializeEmptySlot(UActorComponent* InInventoryOwner, FRpgInventoryGridPlacement InPlacement);

	/** Inventory component that owns this entry or empty capacity slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	UActorComponent* GetInventoryOwner() const { return InventoryOwner.Get(); }

	/** Inventory manager that owns this entry or empty capacity slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	URpgInventoryManagerComponent* GetInventoryManager() const { return Cast<URpgInventoryManagerComponent>(InventoryOwner.Get()); }

	/** Concrete item instance represented by this entry, or nullptr for empty capacity slots. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	URpgInventoryItemInstance* GetItemInstance() const { return ItemInstance.Get(); }

	/** Stable replicated entry id used for shared manual ordering. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FGuid GetEntryId() const { return EntryId; }

	/** Current replicated stack count. Empty capacity slots return 0. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	int32 GetStackCount() const { return StackCount; }

	/** Server-authored grid placement represented by this entry or empty capacity cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FRpgInventoryGridPlacement GetPlacement() const { return Placement; }

	/** Visual index used only by legacy list selection helpers, derived from Placement for current widgets. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Returns true for UI-only placeholder slots that do not contain an item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	bool IsEmptySlot() const { return bIsEmptySlot; }

	/** Returns true when this entry can start a drag or controller hold. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	bool CanDrag() const { return bCanDrag; }

	/** Fired when this slot object keeps its identity but its visual item/stack data changed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|ViewModel")
	FRpgInventoryEntryViewModelChanged OnEntryChanged;

protected:
	/** Inventory component that owns this entry. Drag payloads should pass this back to server RPCs. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	/** Concrete replicated item instance for command payloads. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Stable replicated entry id for manual ordering. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGuid EntryId;

	/** Current replicated stack count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;

	/** Server-authored spatial placement. UI may preview it, but gameplay mutations are server-validated. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridPlacement Placement;

	/** Visual index used by existing TileView selection helpers; not gameplay placement truth. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	/** Full display name from the item definition. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional short name from UIData, falling back to DisplayName. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** Optional tooltip text from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** Optional item icon from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Broad item category from ItemTraits. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryItemCategory ItemCategory = ERpgInventoryItemCategory::Misc;

	/** Gameplay tags from ItemTraits. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ItemTags;

	/** Presentation-only tags from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer PresentationTags;

	/** True when widgets may start a drag payload for this entry. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bCanDrag = false;

	/** True for UI-only placeholder slots that can receive drops but do not contain an item. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bIsEmptySlot = true;

	/** Optional per-fragment presenters generated for this entry. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventoryFragmentViewModel>> FragmentViewModels;
};

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
	void RefreshCapacityFields(URpgInventoryManagerComponent* Inventory);
	void RegisterInventoryMessageListener();
	void UnregisterInventoryMessageListener();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);

	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedInventory;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	bool bRefreshEntriesQueued = false;
};
