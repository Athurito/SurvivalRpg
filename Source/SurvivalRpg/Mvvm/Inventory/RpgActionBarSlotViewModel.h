#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgActionBarSlotViewModel.generated.h"

class UTexture2D;
class URpgAbilitySystemComponent;
class URpgInventoryItemInstance;
class URpgActionBarSlotViewModel;

/** Broadcast when one general actionbar slot view model changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgActionBarSlotViewModelChanged, URpgActionBarSlotViewModel*, SlotViewModel);

/** UI projection for one 1..8 general actionbar slot. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgActionBarSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this slot from owner-only replicated actionbar state. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void InitializeSlot(int32 InSlotIndex, const FRpgActionBarSlot& InSlot, URpgInventoryItemInstance* ResolvedItem, int32 InStackCount);

	/** Rebuilds this slot and resolves ability presentation from the owning ASC. */
	void InitializeSlotWithAbilitySystem(
		int32 InSlotIndex,
		const FRpgActionBarSlot& InSlot,
		URpgInventoryItemInstance* ResolvedItem,
		int32 InStackCount,
		const URpgAbilitySystemComponent* AbilitySystem,
		FText CarryDisplayName = FText::GetEmpty());

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	ERpgActionBarSlotType GetSlotType() const { return SlotType; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	bool HasContent() const { return bHasContent; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	URpgInventoryItemInstance* GetItemInstance() const { return ItemInstance.Get(); }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FRpgInventorySlotAddress GetSlotAddress() const { return SlotAddress; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	int32 GetStackCount() const { return StackCount; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FText GetStackCountText() const { return StackCountText; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	bool IsAvailable() const { return bAvailable; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	ERpgQuickAccessBlockedReason GetBlockedReason() const { return BlockedReason; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FGameplayTag GetAbilityId() const { return AbilityId; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FGameplayTag GetCarrySemanticRole() const { return CarrySemanticRole; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FText GetShortDisplayName() const { return ShortDisplayName; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FName GetHotkeyActionRowName() const { return HotkeyActionRowName; }

	UPROPERTY(BlueprintAssignable, Category = "Action Bar|ViewModel")
	FRpgActionBarSlotViewModelChanged OnSlotChanged;

protected:
	/** Zero-based slot index used by commands and row-handle lookup. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	/** Whether this slot is empty, bound to a normal inventory slot, or bound to a carry slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgActionBarSlotType SlotType = ERpgActionBarSlotType::Empty;

	/** True when the slot has a source-slot assignment, even if that source slot is currently empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bHasContent = false;

	/** Logical source slot this actionbar entry activates. Empty entries have an invalid address. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventorySlotAddress SlotAddress;

	/** Inventory-owned item currently resolved from SlotAddress. Null when the bound source slot is empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Current stack count for the resolved item, read from the player's inventory when available. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;

	/** Compact stack label for read-only HUD leaves; empty when the resolved stack does not need a count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText StackCountText;

	/** Current server-derived availability for the typed binding. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bAvailable = false;

	/** Stable blocked reason for disabled/empty radial and HUD indicators. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgQuickAccessBlockedReason BlockedReason = ERpgQuickAccessBlockedReason::Empty;

	/** Semantic ability id for Ability bindings; invalid for item/carry bindings. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTag AbilityId;

	/** Stable layout role for Carry bindings; invalid for consumables, abilities, and empty slots. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTag CarrySemanticRole;

	/** Optional item icon read from the item currently resolved from SlotAddress. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Short item name or source-slot fallback text for compact slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** CommonUI action row name expected in CDT_RpgUIActions_All for this slot's hotkey glyph. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FName HotkeyActionRowName;
};
