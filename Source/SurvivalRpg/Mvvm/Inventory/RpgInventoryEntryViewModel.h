#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryEntryViewModel.generated.h"

class UTexture2D;
class URpgInventoryItemFragment;
class URpgInventoryItemInstance;
class URpgInventoryEntryViewModel;
class URpgInventoryItemizationFragmentViewModel;

/** Broadcast when one slot view model changed its represented item, stack, or empty state. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgInventoryEntryViewModelChanged, URpgInventoryEntryViewModel*, EntryViewModel);

/**
 * One UI row/slot generated from an inventory entry.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
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

	/** Persistent item identity used to keep this child VM stable across entry reconstruction. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FRpgInventoryItemId GetItemId() const { return ItemId; }

	/** Inventory-local replicated entry id used for shared manual ordering and stale-request validation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FGuid GetEntryId() const { return EntryId; }

	/** Current replicated stack count. Empty capacity slots return 0. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	int32 GetStackCount() const { return StackCount; }

	/** Full item display name from the definition. Empty capacity slots return empty text. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FText GetDisplayName() const { return DisplayName; }

	/** Compact UI label from UIData, falling back to DisplayName. Empty capacity slots return empty text. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FText GetShortDisplayName() const { return ShortDisplayName; }

	/** Optional item icon from UIData. Empty capacity slots return no icon. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

	/** Optional localized tooltip description from the static UIData fragment. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FText GetDescription() const { return Description; }

	/** Generated-item presenter for this concrete item, or null for ordinary materials and legacy items. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	URpgInventoryItemizationFragmentViewModel* GetItemizationViewModel() const;

	/** Server-authored grid placement represented by this entry or empty capacity cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|ViewModel")
	FRpgInventoryGridPlacement GetPlacement() const { return Placement; }

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

	/** Persistent item identity. UI list reconciliation uses this instead of the inventory-local EntryId. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryItemId ItemId;

	/** Inventory-local replicated entry id for manual ordering and stale-request validation. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGuid EntryId;

	/** Current replicated stack count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;

	/** Server-authored spatial placement. UI may preview it, but gameplay mutations are server-validated. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridPlacement Placement;

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
