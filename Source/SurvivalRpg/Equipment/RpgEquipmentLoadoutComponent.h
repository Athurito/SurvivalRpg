#pragma once

#include "Components/ControllerComponent.h"
#include "RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include "RpgEquipmentLoadoutComponent.generated.h"

class AActor;
class URpgEquipmentInstance;
class URpgEquipmentManagerComponent;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
class URpgWeaponAbilityLoadoutComponent;

/** Persistent player-owned assignment for one equipment slot, including hands and armor. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentLoadoutSlot
{
	GENERATED_BODY()

	/** Equipment slot represented by this loadout entry. V1 manages MainHand, OffHand, and armor slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;

	/** Inventory item assigned to the slot. The item stays owned by inventory; equipment runtime state is recreated per pawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgInventoryItemInstance> Item = nullptr;
};

/** Remembered offhand assignment for one mainhand item while the item remains inventory-owned. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRememberedOffhandForMainHand
{
	GENERATED_BODY()

	/** Mainhand item that should restore the remembered offhand when activated again. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgInventoryItemInstance> MainHandItem = nullptr;

	/** Offhand item restored for MainHandItem when still owned and valid. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgInventoryItemInstance> OffHandItem = nullptr;
};

/** Gameplay message emitted when dedicated equipment slot assignments change. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentLoadoutSlotsChangedMessage
{
	GENERATED_BODY()

	/** Controller that owns the equipment loadout. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<AActor> Owner = nullptr;

	/** Current replicated equipment slot assignments. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TArray<FRpgEquipmentLoadoutSlot> Slots;
};

/**
 * Controller-owned loadout for persistent equipment slots such as hands and armor.
 *
 * The component stores persistent item-instance assignments on the controller, while URpgEquipmentManagerComponent
 * remains the runtime authority that equips actors, grants abilities, and replicates equipped state on the pawn.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEquipmentLoadoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment")
	TArray<FRpgEquipmentLoadoutSlot> GetLoadoutSlots() const { return Slots; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment")
	URpgInventoryItemInstance* GetItemInEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/** Client/UI entry point for assigning an owned inventory item to a dedicated equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Client/UI entry point for clearing a dedicated equipment slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment")
	void RequestClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns true when the item is owned by this controller and its EquipmentDefinition permits the slot. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment")
	bool CanAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, const URpgInventoryItemInstance* Item) const;

	/** Server-side assignment used by drag-and-drop and starter/debug flows. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool AssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Server-side clear for one dedicated equipment slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	URpgInventoryItemInstance* ClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Clears every dedicated equipment slot reference to this item. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ClearItemFromAllEquipmentSlots(URpgInventoryItemInstance* Item);

	/** Returns whether the item can be removed from inventory without leaving invalid equipment or bag slots behind. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool CanRemoveItemFromLoadout(URpgInventoryItemInstance* Item) const;

	/** Activates a carry-slot weapon or tool as the current MainHand, restoring remembered offhand when valid. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ActivateMainHandItem(URpgInventoryItemInstance* Item);

	/** Activates a carry-slot shield/offhand item, remembering it for the currently active one-handed mainhand. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ActivateOffHandItem(URpgInventoryItemInstance* Item);

	/** Clears the active runtime hands without moving items out of their inventory carry slots. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ClearActiveHands();

	/** Clears OffHand. When requested by the player, also forgets the active MainHand's offhand memory. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ClearActiveOffHand(bool bForgetForActiveMainHand = true);

	/** Returns the remembered offhand item for a mainhand item, or null. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment")
	URpgInventoryItemInstance* GetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem) const;

	/** Removes this loadout's runtime equipment instances from the current pawn. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	void UnequipLoadoutFromCurrentPawn();

	/** Applies the replicated slot assignments to the current pawn when its equipment target is ready. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool RefreshEquipmentLoadoutOnCurrentPawn();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Slots();

private:
	void EnsureDefaultSlots();
	int32 FindSlotIndex(ERpgEquipmentSlot EquipmentSlot) const;
	URpgEquipmentInstance* EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const;
	void UnequipRuntimeSlot(ERpgEquipmentSlot EquipmentSlot);
	URpgEquipmentManagerComponent* FindEquipmentManager() const;
	URpgInventoryManagerComponent* FindOwnerInventory() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	bool HasReadyEquipmentTarget() const;
	void BroadcastSlotsChanged() const;
	void RefreshWeaponAbilityLoadout() const;
	bool CanClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;
	bool IsTwoHandItem(const URpgInventoryItemInstance* Item) const;
	bool MoveInventoryItemToEquipmentSlotAddress(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item) const;
	FGuid FindInventoryEntryIdForItem(const URpgInventoryManagerComponent* Inventory, const URpgInventoryItemInstance* Item) const;
	void RememberCurrentOffhandForActiveMainhand();
	void SetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem, URpgInventoryItemInstance* OffHandItem);
	void ClearRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem);
	void ClearRememberedOffhandEntriesForItem(URpgInventoryItemInstance* Item);
	bool AssignRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);
	static bool IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	static bool IsRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	static bool IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgEquipmentLoadoutSlot> Slots;

	/** Owner-replicated memory that lets a one-handed mainhand restore the last manually paired offhand. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgRememberedOffhandForMainHand> RememberedOffhands;

	TMap<ERpgEquipmentSlot, TWeakObjectPtr<URpgEquipmentInstance>> EquippedItemsBySlot;
};
