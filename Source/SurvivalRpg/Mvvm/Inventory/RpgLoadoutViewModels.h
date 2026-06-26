#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgQuickBarComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgLoadoutViewModels.generated.h"

class APlayerController;
class UTexture2D;
class URpgInventoryItemInstance;
class URpgQuickBarSlotViewModel;
class URpgEquipmentSlotViewModel;

/** Broadcast when a fixed quickbar loadout slot changed its represented items or active state. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgQuickBarSlotViewModelChanged, URpgQuickBarSlotViewModel*, SlotViewModel);

/** Broadcast when the quickbar slot list was rebuilt or updated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgQuickBarViewModelSlotsChanged);

/** Broadcast when a fixed dedicated equipment slot changed its represented item. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgEquipmentSlotViewModelChanged, URpgEquipmentSlotViewModel*, SlotViewModel);

/** Broadcast when the dedicated equipment slot list was rebuilt or updated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgEquipmentLoadoutViewModelSlotsChanged);

/**
 * UI projection for one weapon quickbar loadout slot.
 *
 * The slot represents one quickbar index and may contain a main-hand item, an off-hand item, or both.
 * It reads replicated component state only; gameplay assignment remains server-authoritative.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgQuickBarSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this fixed slot from the replicated quickbar loadout data. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|ViewModel")
	void InitializeSlot(int32 InSlotIndex, const FRpgQuickBarLoadoutSlot& InLoadoutSlot, bool bInActiveSlot);

	/** Visual quickbar index, zero-based for commands and one-based when displayed as KeyLabel. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Returns true when this slot is currently active on the quickbar component. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	bool IsActiveSlot() const { return bIsActiveSlot; }

	/** Returns true when either hand has an assignment. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	bool HasAnyItem() const { return bHasAnyItem; }

	/** Display-ready activation key label, usually 1 through 8. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	FText GetKeyLabel() const { return KeyLabel; }

	/** Returns the assigned item for MainHand or OffHand. Other equipment slots return null. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	URpgInventoryItemInstance* GetItemForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns true when the requested hand slot contains an item. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	bool HasItemForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns the icon for the requested hand slot, if the item has UIData. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	TSoftObjectPtr<UTexture2D> GetIconForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns the compact display name for the requested hand slot. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	FText GetShortDisplayNameForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Fired when this slot object keeps its identity but its item or active state changed. */
	UPROPERTY(BlueprintAssignable, Category = "QuickBar|ViewModel")
	FRpgQuickBarSlotViewModelChanged OnSlotChanged;

protected:
	/** Visual quickbar index used by UI and drag/drop payloads. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	/** Display-ready key label, usually 1 through 8 for keyboard activation. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText KeyLabel;

	/** True when this quickbar slot is currently selected as the active weapon loadout. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bIsActiveSlot = false;

	/** True when either hand in this loadout slot has an item assignment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bHasAnyItem = false;

	/** Inventory-owned item assigned to this loadout's main hand. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> MainHandItem = nullptr;

	/** Inventory-owned item assigned to this loadout's off hand. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> OffHandItem = nullptr;

	/** Optional main-hand icon read from the item's UIData fragment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> MainHandIcon;

	/** Optional off-hand icon read from the item's UIData fragment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> OffHandIcon;

	/** Compact main-hand display name for slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText MainHandShortDisplayName;

	/** Compact off-hand display name for slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText OffHandShortDisplayName;
};

/**
 * UI projection for the controller-owned weapon quickbar.
 *
 * Widgets bind this to the local player's URpgQuickBarComponent and render the fixed weapon/offhand loadout slots.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgQuickBarViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Resolves and observes the quickbar component on an RPG player controller. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Starts observing one replicated quickbar component. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|ViewModel")
	void BindQuickBar(URpgQuickBarComponent* InQuickBar);

	/** Stops observing the current quickbar and renders empty default slots. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|ViewModel")
	void UnbindQuickBar();

	/** Rebuilds slot view models from the observed quickbar. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|ViewModel")
	void RefreshSlots();

	/** Current quickbar slot view models in visual order. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	TArray<URpgQuickBarSlotViewModel*> GetSlots() const;

	/** Returns a slot view model by zero-based quickbar index. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	URpgQuickBarSlotViewModel* GetSlotAtIndex(int32 SlotIndex) const;

	/** Currently observed quickbar component, if any. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|ViewModel")
	URpgQuickBarComponent* GetObservedQuickBar() const { return ObservedQuickBar.Get(); }

	/** Fired after Slots has been rebuilt or updated. */
	UPROPERTY(BlueprintAssignable, Category = "QuickBar|ViewModel")
	FRpgQuickBarViewModelSlotsChanged OnSlotsChanged;

protected:
	virtual void BeginDestroy() override;

	/** Slot view models in fixed visual order. HUD/List widgets should render these directly. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgQuickBarSlotViewModel>> Slots;

	/** Active quickbar slot index replicated by the quickbar component. INDEX_NONE means no active loadout. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 ActiveSlotIndex = INDEX_NONE;

	/** Fallback number of slots to render before the quickbar component has replicated its slot array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickBar|ViewModel", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 DefaultSlotCount = 8;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void HandleQuickBarSlotsChanged(FGameplayTag Channel, const FRpgQuickBarSlotsChangedMessage& Message);
	void HandleQuickBarActiveIndexChanged(FGameplayTag Channel, const FRpgQuickBarActiveIndexChangedMessage& Message);

	TWeakObjectPtr<URpgQuickBarComponent> ObservedQuickBar;
	FGameplayMessageListenerHandle SlotsChangedHandle;
	FGameplayMessageListenerHandle ActiveIndexChangedHandle;
};

/**
 * UI projection for one dedicated armor/equipment slot.
 *
 * The item stays owned by inventory; this view model only represents the controller-owned slot assignment.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgEquipmentSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this fixed equipment slot from replicated loadout data. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
	void InitializeSlot(ERpgEquipmentSlot InEquipmentSlot, URpgInventoryItemInstance* InItem);

	/** Dedicated equipment slot represented by this view model. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	ERpgEquipmentSlot GetEquipmentSlot() const { return EquipmentSlot; }

	/** Inventory-owned item assigned to this slot, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	URpgInventoryItemInstance* GetItemInstance() const { return ItemInstance.Get(); }

	/** True when this dedicated slot contains an item assignment. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	bool HasItem() const { return bHasItem; }

	/** Fired when this slot object keeps its identity but its item changed. */
	UPROPERTY(BlueprintAssignable, Category = "Equipment|ViewModel")
	FRpgEquipmentSlotViewModelChanged OnSlotChanged;

protected:
	/** Dedicated slot represented by this UI projection. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;

	/** Display-ready slot label such as Head, Chest, or Feet. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText SlotLabel;

	/** Inventory-owned item assigned to this slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** True when ItemInstance is set. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bHasItem = false;

	/** Optional item icon read from the item's UIData fragment. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Compact item display name for slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;
};

/**
 * UI projection for controller-owned dedicated equipment slots.
 *
 * V1 renders Head, Chest, Hands, Legs, and Feet as fixed slots. Runtime equipment remains owned by
 * URpgEquipmentManagerComponent after the loadout component applies these assignments to the pawn.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgEquipmentLoadoutViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Resolves and observes the equipment loadout component on an RPG player controller. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Starts observing one replicated equipment loadout component. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
	void BindEquipmentLoadout(URpgEquipmentLoadoutComponent* InEquipmentLoadout);

	/** Stops observing the current loadout and renders empty fixed equipment slots. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
	void UnbindEquipmentLoadout();

	/** Rebuilds slot view models from the observed equipment loadout. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|ViewModel")
	void RefreshSlots();

	/** Current dedicated equipment slot view models in stable visual order. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	TArray<URpgEquipmentSlotViewModel*> GetSlots() const;

	/** Returns the view model for a dedicated equipment slot. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	URpgEquipmentSlotViewModel* GetSlotForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Currently observed equipment loadout component, if any. */
	UFUNCTION(BlueprintPure, Category = "Equipment|ViewModel")
	URpgEquipmentLoadoutComponent* GetObservedEquipmentLoadout() const { return ObservedEquipmentLoadout.Get(); }

	/** Fired after Slots has been rebuilt or updated. */
	UPROPERTY(BlueprintAssignable, Category = "Equipment|ViewModel")
	FRpgEquipmentLoadoutViewModelSlotsChanged OnSlotsChanged;

protected:
	virtual void BeginDestroy() override;

	/** Dedicated equipment slot view models in fixed visual order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Equipment|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgEquipmentSlotViewModel>> Slots;

private:
	void RegisterMessageListener();
	void UnregisterMessageListener();
	void HandleEquipmentLoadoutSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);
	static TConstArrayView<ERpgEquipmentSlot> GetDefaultEquipmentSlotOrder();

	TWeakObjectPtr<URpgEquipmentLoadoutComponent> ObservedEquipmentLoadout;
	FGameplayMessageListenerHandle SlotsChangedHandle;
};
