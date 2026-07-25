#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "RpgActionBarSlotViewModel.h"
#include "RpgInventorySlotGroupViewModel.h"
#include "RpgLoadoutViewModels.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"

#include "RpgPlayerInventoryViewModel.generated.h"

class APlayerController;
class URpgEquipmentLoadoutComponent;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
struct FRpgEquipmentLoadoutSlotsChangedMessage;
struct FRpgInventoryChangeMessage;
struct FRpgPlayerInventoryLayoutChangedMessage;

/** Broadcast when the player-inventory layout VM rebuilt one or more public lists. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgPlayerInventoryViewModelChanged);

/**
 * Aggregate player-inventory VM for the Tarkov-like RPG layout.
 *
 * This is the one VM CUI_PlayerInventory should bind. It mirrors replicated gameplay state from inventory,
 * equipment loadout, layout, and actionbar components without owning any gameplay truth.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "PropertyPath"))
class SURVIVALRPG_API URpgPlayerInventoryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Native presenter lifecycle entry point that resolves and observes the owning player's gameplay components.
	 * Rebinding the same controller is supported and refreshes the read-only projection without replacing child VMs.
	 */
	void BindPlayerController(APlayerController* InPlayerController);

	/**
	 * Native presenter lifecycle exit point that releases gameplay observation and resets read-only projections.
	 * The aggregate VM itself remains screen-owned so CommonUI can safely reuse it after deactivation.
	 */
	void UnbindPlayerInventory();

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

	/** Returns exactly one group VM for an explicit semantic role; missing or duplicate roles fail closed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Player ViewModel")
	URpgInventorySlotGroupViewModel* GetSlotGroupBySemanticRole(FGameplayTag SemanticRole) const;

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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgPlayerStorageInventoryLifecycleIntegrationTest;
#endif

	/** Rebuilds every aggregate projection; lifecycle entry points and gameplay-message handlers own refresh timing. */
	void RefreshAll();
	void RegisterMessageListeners();
	void UnregisterMessageListeners();
	void RequestRefresh(uint8 RefreshDomains);
	void ExecuteQueuedRefresh();
	void FlushPendingRefreshes();
	void CancelQueuedRefresh();
	void RefreshGearSlots();
	void RefreshSlotGroups();
	void RefreshActionBarSlots();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message);
	void HandleEquipmentSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);
	void HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);

	static TConstArrayView<ERpgEquipmentSlot> GetArmorSlotOrder();
	static TConstArrayView<ERpgEquipmentSlot> GetBagSlotOrder();

	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedPlayerInventory;
	TWeakObjectPtr<URpgPlayerInventoryLayoutComponent> ObservedInventoryLayout;
	TWeakObjectPtr<URpgEquipmentLoadoutComponent> ObservedEquipmentLoadout;
	TWeakObjectPtr<URpgActionBarComponent> ObservedActionBar;

	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle LayoutChangedHandle;
	FGameplayMessageListenerHandle EquipmentChangedHandle;
	FGameplayMessageListenerHandle ActionBarChangedHandle;
	FRpgViewModelInvalidationQueue RefreshQueue;
	uint8 PendingRefreshDomains = 0;
};
