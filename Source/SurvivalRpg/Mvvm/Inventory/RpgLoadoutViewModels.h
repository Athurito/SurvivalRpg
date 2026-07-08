#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgLoadoutViewModels.generated.h"

class APlayerController;
class UTexture2D;
class URpgInventoryItemInstance;
class URpgEquipmentSlotViewModel;

/** Broadcast when a fixed dedicated equipment slot changed its represented item. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgEquipmentSlotViewModelChanged, URpgEquipmentSlotViewModel*, SlotViewModel);

/** Broadcast when the dedicated equipment slot list was rebuilt or updated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgEquipmentLoadoutViewModelSlotsChanged);

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
