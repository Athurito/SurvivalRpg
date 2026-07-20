#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"
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

/** Pointer-free saved pairing between one ready main-hand item and its remembered ready offhand item. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRememberedOffhandItemIds
{
	GENERATED_BODY()

	/** Persistent main-hand item identity resolved only after the inventory graph has restored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Equipment|Persistence")
	FRpgInventoryItemId MainHandItemId;

	/** Persistent offhand item identity resolved only after the inventory graph has restored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Equipment|Persistence")
	FRpgInventoryItemId OffHandItemId;
};

/** Saved active hand selection; physical Gear and Carry locations remain owned by the inventory graph. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentSelectionSaveData
{
	GENERATED_BODY()

	/** Currently selected main-hand item id, or invalid when hands were holstered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Equipment|Persistence")
	FRpgInventoryItemId ActiveMainHandItemId;

	/** Currently selected offhand item id, or invalid when no offhand was active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Equipment|Persistence")
	FRpgInventoryItemId ActiveOffHandItemId;

	/** Saved item-id pairings used when switching between ready one-handed weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Equipment|Persistence")
	TArray<FRpgRememberedOffhandItemIds> RememberedOffhands;
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

	/** Authoritative sum of static equipment load from items physically located in Gear and Carry grids, in kilograms. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment|Load")
	float EquipmentLoadWeight = 0.0f;

	/** Authoritative dodge tier derived from EquipmentLoadWeight and the loadout component thresholds. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment|Load")
	ERpgEquipmentLoadTier EquipmentLoadTier = ERpgEquipmentLoadTier::Light;
};

/**
 * Controller-owned loadout for persistent equipment slots such as hands and armor.
 *
 * Inventory graph locations are the physical truth. This component mirrors Gear slots and stores only active
 * Main-/OffHand selection plus remembered pairings, while URpgEquipmentManagerComponent owns runtime actors/grants.
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

	/** Deprecated adapter. New UI must submit the operation through URpgInventoryUiActionComponent. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment", meta = (DeprecatedFunction, DeprecationMessage = "Use InventoryUiActionComponent.RequestAssignItemToEquipmentSlot so inventory location remains authoritative."))
	void RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Deprecated adapter. New UI must submit the operation through URpgInventoryUiActionComponent. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Equipment", meta = (DeprecatedFunction, DeprecationMessage = "Use InventoryUiActionComponent.RequestClearEquipmentSlot so inventory location remains authoritative."))
	void RequestClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns true when this controller owns the item and the shared Inventory Equipment policy permits the slot. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment")
	bool CanAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, const URpgInventoryItemInstance* Item) const;

	/** Internal reconciliation adapter. Gameplay UI should mutate inventory locations instead. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment", meta = (DeprecatedFunction, DeprecationMessage = "Move the item to its Gear/Carry inventory location through InventoryUiActionComponent."))
	bool AssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Internal reconciliation adapter. Gameplay UI should move the physical item to content instead. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment", meta = (DeprecatedFunction, DeprecationMessage = "Move the physical item to content through InventoryUiActionComponent."))
	URpgInventoryItemInstance* ClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Clears every dedicated equipment slot reference to this item. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ClearItemFromAllEquipmentSlots(URpgInventoryItemInstance* Item);

	/** Returns whether the item can be removed from inventory without leaving invalid equipment or bag slots behind. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool CanRemoveItemFromLoadout(URpgInventoryItemInstance* Item) const;

	/** Activates a ready Carry weapon as the current MainHand, restoring remembered offhand when valid. */
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

	/** Returns the authoritative Gear+Carry equipment load in kilograms. Normal container contents are excluded. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment|Load")
	float GetEquipmentLoadWeight() const { return CurrentEquipmentLoadWeight; }

	/** Returns the current Light, Medium, or Heavy tier selected from the designer thresholds. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment|Load")
	ERpgEquipmentLoadTier GetEquipmentLoadTier() const { return CurrentEquipmentLoadTier; }

	/** Returns the GAS tag corresponding to the current load tier. Exactly one tier tag is applied on the pawn ASC. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment|Load")
	FGameplayTag GetEquipmentLoadTierTag() const;

	/** Returns the designer-authored montage/root-motion seam for the current load tier. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment|Load")
	FRpgEquipmentDodgeProfile GetDodgeProfileForCurrentLoad() const;

	/** Pure threshold helper used by validation and automation tests; values at a threshold enter the higher tier. */
	static ERpgEquipmentLoadTier ResolveLoadTierForWeight(
		float LoadWeight,
		float MediumThreshold = 13.0f,
		float HeavyThreshold = 23.0f);

	/**
	 * Recalculates weight exclusively from items in data-driven Gear and Carry groups and updates the pawn GAS tier tag.
	 * Inventory reconciliation calls this once after a committed transaction that changes physical equipment locations.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment|Load")
	void RefreshEquipmentLoadState();

	/**
	 * Rebuilds non-hand equipment pointers and runtime grants from physical Gear.* inventory locations, then resolves
	 * the remembered active-hand selection by persistent item id. Call once after graph reconstruction or transfer.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool ReconcilePhysicalEquipmentFromInventory();

	/** Exports active hand selection and remembered pairs as persistent item ids; Gear/Carry placement is not duplicated. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment|Persistence")
	FRpgEquipmentSelectionSaveData ExportEquipmentSelection() const;

	/** Restores pointer-free hand selection after the inventory graph and pawn ASC are ready. Invalid ids remain holstered. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment|Persistence")
	void RestoreEquipmentSelection(const FRpgEquipmentSelectionSaveData& SaveData);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_EquipmentLoadState();

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
	bool IsItemInCarryActivationRole(const URpgInventoryItemInstance* Item, FGameplayTag ActivationRole) const;
	bool MoveInventoryItemToEquipmentSlotAddress(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item) const;
	void RememberCurrentOffhandForActiveMainhand();
	void SetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem, URpgInventoryItemInstance* OffHandItem);
	void ClearRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem);
	void ClearRememberedOffhandEntriesForItem(URpgInventoryItemInstance* Item);
	bool AssignRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);
	float CalculateEquipmentLoadWeight() const;
	ERpgEquipmentLoadTier ResolveEquipmentLoadTier(float LoadWeight) const;
	void ApplyEquipmentLoadTierTag() const;
	static const URpgEquipmentDefinition* FindEquipmentDefinition(const URpgInventoryItemInstance* Item);
	static FGameplayTag GetTagForEquipmentLoadTier(ERpgEquipmentLoadTier Tier);
	static bool IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	static bool IsRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	static bool IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgEquipmentLoadoutSlot> Slots;

	/** Owner-replicated memory that lets a one-handed mainhand restore the last manually paired offhand. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgRememberedOffhandForMainHand> RememberedOffhands;

	/** First kilogram value classified as Medium. Values below this are Light. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "kg"))
	float MediumLoadThreshold = 13.0f;

	/** First kilogram value classified as Heavy. Must be greater than or equal to MediumLoadThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0", Units = "kg"))
	float HeavyLoadThreshold = 23.0f;

	/** Dodge montage and root-motion profile used while Equipment.Load.Light is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true"))
	FRpgEquipmentDodgeProfile LightDodgeProfile;

	/** Dodge montage and root-motion profile used while Equipment.Load.Medium is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true"))
	FRpgEquipmentDodgeProfile MediumDodgeProfile;

	/** Dodge montage and root-motion profile used while Equipment.Load.Heavy is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true"))
	FRpgEquipmentDodgeProfile HeavyDodgeProfile;

	/** Server-derived Gear+Carry load in kilograms, owner-replicated for HUD and inventory presentation. */
	UPROPERTY(ReplicatedUsing = OnRep_EquipmentLoadState, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true"))
	float CurrentEquipmentLoadWeight = 0.0f;

	/** Server-derived load tier. Public combat state is mirrored through the pawn ASC tier tag. */
	UPROPERTY(ReplicatedUsing = OnRep_EquipmentLoadState, BlueprintReadOnly, Category = "Equipment|Load", meta = (AllowPrivateAccess = "true"))
	ERpgEquipmentLoadTier CurrentEquipmentLoadTier = ERpgEquipmentLoadTier::Light;

	TMap<ERpgEquipmentSlot, TWeakObjectPtr<URpgEquipmentInstance>> EquippedItemsBySlot;
};
