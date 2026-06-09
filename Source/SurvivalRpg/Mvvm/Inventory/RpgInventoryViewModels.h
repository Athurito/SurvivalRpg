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

	/** Whether this item may be assigned to the quickbar according to static traits. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	bool bCanAssignToQuickBar = false;

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

	/** Shared server-authored order key. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SortIndex = 0;

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

	/** Static quickbar hint from ItemTraits; server validation still owns the final answer. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bCanAssignToQuickBar = false;

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

	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	URpgInventoryManagerComponent* GetObservedInventory() const { return ObservedInventory.Get(); }

protected:
	virtual void BeginDestroy() override;

	/** Current entry models in replicated shared order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventoryEntryViewModel>> Entries;

	/** Fragment presenter mapping. Designers may extend this for durability, affixes, sockets, and similar fragments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>> FragmentViewModelClasses;

private:
	void RegisterInventoryMessageListener();
	void UnregisterInventoryMessageListener();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);

	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedInventory;
	FGameplayMessageListenerHandle InventoryChangedHandle;
};
