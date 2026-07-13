#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgPlayerInventoryViewModels.generated.h"

class APlayerController;
class UTexture2D;
class URpgActionBarComponent;
class URpgActionBarSlotViewModel;
class URpgEquipmentLoadoutComponent;
class URpgEquipmentSlotViewModel;
class URpgInventoryAddressSlotViewModel;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
struct FRpgActionBarSlot;
struct FRpgActionBarSlotsChangedMessage;
struct FRpgEquipmentLoadoutSlotsChangedMessage;
struct FRpgInventoryChangeMessage;
struct FRpgPlayerInventoryLayoutChangedMessage;

/** Broadcast when one logical player-inventory slot changes presentation data. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgInventoryAddressSlotViewModelChanged, URpgInventoryAddressSlotViewModel*, SlotViewModel);

/** Broadcast when the player-inventory layout VM rebuilt one or more public lists. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgPlayerInventoryViewModelChanged);

/**
 * UI projection for one logical player-inventory slot address.
 *
 * The item stays owned by URpgInventoryManagerComponent. This object only exposes group/address presentation
 * for CommonUI widgets and drag/drop commands.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryAddressSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this slot from the current layout and inventory state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Layout ViewModel")
	void InitializeSlot(URpgInventoryManagerComponent* InInventory, URpgPlayerInventoryLayoutComponent* InInventoryLayout, const FRpgInventorySlotGroupView& InGroupView, int32 InX, int32 InY);

	/** Inventory component that owns the represented item, or null when the player inventory is unavailable. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	URpgInventoryManagerComponent* GetInventoryManager() const { return Inventory.Get(); }

	/** Layout component used to resolve this logical address to a global inventory slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	URpgPlayerInventoryLayoutComponent* GetInventoryLayout() const { return InventoryLayout.Get(); }

	/** Stable logical slot address such as WeaponSlot1[0] or Belt[2]. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventorySlotAddress GetSlotAddress() const { return SlotAddress; }

	/** Authoritative spatial placement resolved from SlotAddress. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventoryGridPlacement GetPlacement() const { return Placement; }

	/** Authoritative placement for the item occupying this cell, or invalid when the cell is empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventoryGridPlacement GetItemPlacement() const { return ItemPlacement; }

	/** Occupied item width in cells after rotation. Empty cells return zero. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	int32 GetItemOccupiedWidth() const { return ItemOccupiedWidth; }

	/** Occupied item height in cells after rotation. Empty cells return zero. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	int32 GetItemOccupiedHeight() const { return ItemOccupiedHeight; }

	/** Visual linear index derived from Placement for existing selection widgets. Not gameplay truth. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	int32 GetGlobalSlotIndex() const { return Placement.IsValid() ? Placement.Y * 1000 + Placement.X : INDEX_NONE; }

	/** Replicated entry id for the current item, or invalid for an empty slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FGuid GetEntryId() const { return EntryId; }

	/** Item currently stored in this logical slot, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	URpgInventoryItemInstance* GetItemInstance() const { return ItemInstance.Get(); }

	/** Current stack count for ItemInstance. Empty slots return zero. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	int32 GetStackCount() const { return StackCount; }

	/** Designer-facing label for this logical address, for example Weapon 1 or Shield. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FText GetSlotLabel() const { return SlotLabel; }

	/** Short item name for compact slot or overlay presentation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FText GetShortDisplayName() const { return ShortDisplayName; }

	/** Optional item icon for compact slot or overlay presentation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

	/** True when this slot has no item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsEmptySlot() const { return bIsEmptySlot; }

	/** True when this cell is the top-left/origin cell of the occupying item footprint. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsItemOriginCell() const { return bItemOriginCell; }

	/** True when this cell is covered by an item whose origin is another cell. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsItemCoveredCell() const { return bItemCoveredCell; }

	/** True only for the item origin cell that should draw icon/name/stack visuals. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool ShouldRenderItemVisual() const { return bRenderItemVisual; }

	/** True when this slot can start a drag/controller hold. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool CanDrag() const { return bCanDrag; }

	/** True when this address may be bound to the 1..8 actionbar. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsActionbarBindable() const { return bActionbarBindable; }

	/** True when this address activates MainHand or OffHand as a carry slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsCarrySlot() const { return bCarrySlot; }

	/** True when this address belongs to a dedicated gear slot such as Gear.Head or Gear.Backpack. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsGearSlot() const { return bGearSlot; }

	/** Fired when this slot object keeps identity but its visible data changed. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Layout ViewModel")
	FRpgInventoryAddressSlotViewModelChanged OnSlotChanged;

protected:
	/** Player inventory that owns the item instance. UI should read only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryManagerComponent> Inventory = nullptr;

	/** Controller-owned layout mapper that resolves logical addresses. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPlayerInventoryLayoutComponent> InventoryLayout = nullptr;

	/** Logical container id that owns this grid cell. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FName ContainerId = NAME_None;

	/** Stable logical address within the current player layout. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventorySlotAddress SlotAddress;

	/** Zero-based X coordinate inside ContainerId. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 X = INDEX_NONE;

	/** Zero-based Y coordinate inside ContainerId. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 Y = INDEX_NONE;

	/** Spatial placement backing this address. Empty slots use a 1x1 cell placement. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridPlacement Placement;

	/** Spatial placement of the item occupying this cell. Covered cells point back to the same item origin. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridPlacement ItemPlacement;

	/** Occupied item width in cells after rotation. Empty cells use zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 ItemOccupiedWidth = 0;

	/** Occupied item height in cells after rotation. Empty cells use zero. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 ItemOccupiedHeight = 0;

	/** Replicated item entry id for item-aware drag/drop. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FGuid EntryId;

	/** Item currently stored at SlotAddress. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Current stack count for the represented item. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;

	/** Short slot label such as "Weapon 1" or "Belt 3". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FText SlotLabel;

	/** Short item name for compact slot UI. Empty when the slot has no item. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** Optional item icon read from UIData, or empty when the slot has no item. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** True when ItemInstance is null. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bIsEmptySlot = true;

	/** True when this cell is the top-left/origin cell of ItemInstance's spatial footprint. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bItemOriginCell = false;

	/** True when this cell is occupied by ItemInstance but should not render/start actions as the primary item cell. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bItemCoveredCell = false;

	/** True when widgets should draw item visuals for this cell. Covered cells remain occupied but visually empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bRenderItemVisual = false;

	/** True when widgets may start a drag payload for this slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bCanDrag = false;

	/** Whether the owning group can be bound to actionbar hotkeys. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bActionbarBindable = false;

	/** Whether this slot is a weapon/tool/shield carry slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bCarrySlot = false;

	/** Whether this slot is a dedicated gear slot. Gear slots are normally rendered by CUI_GearSlot widgets. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bGearSlot = false;
};

/**
 * UI projection for one visible player-inventory slot group such as Pockets, Backpack, Belt, or WeaponSlot1.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventorySlotGroupViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this group from one runtime layout group and its already-created slot VMs. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Layout ViewModel")
	void InitializeGroup(const FRpgInventorySlotGroupView& InGroupView, const TArray<URpgInventoryAddressSlotViewModel*>& InSlots);

	/** Stable group id used by FRpgInventorySlotAddress. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FName GetGroupId() const { return ContainerId; }

	/** Full graph identity of this root or item-owned container. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventoryContainerHandle GetContainerHandle() const { return ContainerHandle; }

	/** Grid size of this visible container. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventoryGridSize GetGridSize() const { return GridSize; }

	/** Logical slots contained by this group. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	TArray<URpgInventoryAddressSlotViewModel*> GetSlots() const;

	/** True when an equipped backpack, belt, pouch, or resource bag owns this content grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsProvidedByEquipment() const { return bProvidedByEquipment; }

	/** Equipment slot name that currently provides this item-owned content grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FName GetSourceEquipmentSlotName() const { return SourceEquipmentSlotName; }

protected:
	/** Stable graph address. Item-owned containers with the same local id remain distinct. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryContainerHandle ContainerHandle;

	/** Stable container id such as WeaponSlot1, Pockets, Backpack, or Belt. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FName ContainerId = NAME_None;

	/** Player-facing group header text. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional header icon for this group. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Grid dimensions for this visible container. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridSize GridSize;

	/** True when every slot in this group may be bound to the 1..8 actionbar. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bActionbarBindable = false;

	/** True when this is a weapon/tool/shield carry group. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bCarryGroup = false;

	/** True when this is a dedicated gear group. Player inventory screens usually render these separately. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bGearGroup = false;

	/** True when this group is normal item storage such as Pockets or Backpack contents. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bContentGroup = false;

	/** True when this group came from an equipped bag/belt/pouch/resource-bag item. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bProvidedByEquipment = false;

	/** Equipment slot name whose item provided this group. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FName SourceEquipmentSlotName = NAME_None;

	/** Slot VMs in stable visual order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventoryAddressSlotViewModel>> Slots;
};

/**
 * Aggregate player-inventory VM for the Tarkov-like RPG layout.
 *
 * This is the one VM CUI_PlayerInventory should bind. It mirrors replicated gameplay state from inventory,
 * equipment loadout, layout, and actionbar components without owning any gameplay truth.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPlayerInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Resolves all owner components from an RPG player controller and starts observing them. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Stops observing gameplay components and clears presentation lists. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player ViewModel")
	void UnbindPlayerInventory();

	/** Rebuilds gear slots, slot groups, and actionbar slot projections. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Player ViewModel")
	void RefreshAll();

	/** Fixed armor slot VMs in Head, Chest, Hands, Legs, Feet order. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	TArray<URpgEquipmentSlotViewModel*> GetArmorSlots() const;

	/** Fixed bag equipment slot VMs in Backpack, Belt, Pouch, ResourceBag order. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	TArray<URpgEquipmentSlotViewModel*> GetBagSlots() const;

	/** Weapon/tool/shield carry groups. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	TArray<URpgInventorySlotGroupViewModel*> GetCarryGroups() const;

	/** Non-carry inventory groups such as Pockets, Backpack, Belt, Pouch, or ResourceBag. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	TArray<URpgInventorySlotGroupViewModel*> GetInventoryGroups() const;

	/** General 1..8 actionbar slot VMs for the player inventory screen. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	TArray<URpgActionBarSlotViewModel*> GetActionBarSlots() const;

	/** Returns one armor slot VM by equipment slot enum. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	URpgEquipmentSlotViewModel* GetArmorSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns one bag equipment slot VM by equipment slot enum. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	URpgEquipmentSlotViewModel* GetBagSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns the group VM for a group id, if currently visible. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	URpgInventorySlotGroupViewModel* GetSlotGroup(FName GroupId) const;

	/** Returns the exact root or item-owned group addressed by its full graph handle. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	URpgInventorySlotGroupViewModel* GetSlotGroupByHandle(FRpgInventoryContainerHandle ContainerHandle) const;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Player ViewModel")
	FRpgPlayerInventoryViewModelChanged OnGearSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Player ViewModel")
	FRpgPlayerInventoryViewModelChanged OnSlotGroupsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Player ViewModel")
	FRpgPlayerInventoryViewModelChanged OnActionBarSlotsChanged;

protected:
	virtual void BeginDestroy() override;

	/** Dedicated armor slots in stable visual order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Player ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgEquipmentSlotViewModel>> ArmorSlots;

	/** Bag, belt, pouch, and resource-bag equipment slots in stable visual order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Player ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgEquipmentSlotViewModel>> BagSlots;

	/** Built-in carry slot groups for weapons, shield, and tools. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Player ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> CarryGroups;

	/** Non-carry item storage groups, including bag-provided dynamic groups. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Player ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventorySlotGroupViewModel>> InventoryGroups;

	/** 1..8 actionbar slot projections. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Player ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgActionBarSlotViewModel>> ActionBarSlots;

private:
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void RefreshGearSlots();
	void RefreshSlotGroups();
	void RefreshActionBarSlots();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message);
	void HandleEquipmentSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);
	void HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);

	static TConstArrayView<ERpgEquipmentSlot> GetArmorSlotOrder();
	static TConstArrayView<ERpgEquipmentSlot> GetBagSlotOrder();

	TWeakObjectPtr<APlayerController> OwningPlayerController;
	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedPlayerInventory;
	TWeakObjectPtr<URpgPlayerInventoryLayoutComponent> ObservedInventoryLayout;
	TWeakObjectPtr<URpgEquipmentLoadoutComponent> ObservedEquipmentLoadout;
	TWeakObjectPtr<URpgActionBarComponent> ObservedActionBar;

	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle LayoutChangedHandle;
	FGameplayMessageListenerHandle EquipmentChangedHandle;
	FGameplayMessageListenerHandle ActionBarChangedHandle;
};
