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

/** Read-only projection of an active hand or a reconciled physical Gear slot. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentLoadoutSlot
{
	GENERATED_BODY()

	/** Equipment slot represented by this loadout entry. V1 manages MainHand, OffHand, and armor slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;

	/** Projected inventory item; ownership and physical placement remain in the inventory graph. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<URpgInventoryItemInstance> Item = nullptr;
};

/** Authority-owned runtime pairing used to restore an offhand when its main-hand item is activated again. */
USTRUCT()
struct SURVIVALRPG_API FRpgRememberedOffhandForMainHand
{
	GENERATED_BODY()

	/** Main-hand item that owns this transient pairing. */
	UPROPERTY()
	TObjectPtr<URpgInventoryItemInstance> MainHandItem = nullptr;

	/** Offhand item restored while both concrete items remain owned and physically ready. */
	UPROPERTY()
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

	/** Read-only replicated projection of active hands and reconciled physical Gear slots. */
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
 * Controller-owned projection and activation state for player equipment.
 *
 * Inventory graph locations are the sole physical truth. This component reconciles a read-only Gear projection,
 * stores active Main-/OffHand selection plus remembered pairings, and drives pawn runtime equipment/GAS grants.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEquipmentLoadoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the read-only projected item for a hand or reconciled physical Gear slot. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	URpgInventoryItemInstance* GetItemInEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const;

	/**
	 * Side-effect-free preflight for selecting an owned item as an active hand.
	 * Physical Carry placement is planned separately; this query adds current two-hand/offhand conflict rules and
	 * may run against replicated owner state without mutating gameplay state.
	 */
	bool CanActivateItemInEquipmentSlot(
		ERpgEquipmentSlot EquipmentSlot,
		const URpgInventoryItemInstance* Item) const;

	/** Server-authoritatively activates a ready Carry weapon as MainHand, restoring remembered offhand when valid. */
	bool ActivateMainHandItem(URpgInventoryItemInstance* Item);

	/** Server-authoritatively activates a ready offhand item and remembers it for the active one-handed mainhand. */
	bool ActivateOffHandItem(URpgInventoryItemInstance* Item);

	/**
	 * Server-authoritatively and idempotently selects a ready MainHand Carry item.
	 * Retryable inventory equipment commands must use this instead of the player-facing toggle adapter.
	 */
	bool SetMainHandItemActive(URpgInventoryItemInstance* Item);

	/**
	 * Server-authoritatively and idempotently selects a ready OffHand Carry item.
	 * Retryable inventory equipment commands must use this instead of the player-facing toggle adapter.
	 */
	bool SetOffHandItemActive(URpgInventoryItemInstance* Item);

	/** Server-authoritatively clears MainHand while preserving valid OffHand selection and physical Carry placement. */
	bool ClearActiveMainHand();

	/** Server-authoritatively clears active runtime hands without moving their physical Carry items. */
	bool ClearActiveHands();

	/** Server-authoritatively clears OffHand and optionally forgets the active MainHand's pairing. */
	bool ClearActiveOffHand(bool bForgetForActiveMainHand = true);

	/** Server-only lifecycle seam that detaches runtime equipment before the current pawn or ASC becomes unavailable. */
	void DetachRuntimeEquipmentFromCurrentPawn();

	/** Server-only lifecycle seam that reconciles projected slots once the pawn equipment manager and ASC are ready. */
	bool ReconcileRuntimeEquipmentOnCurrentPawn();

	/** Returns the authoritative Gear+Carry equipment load in kilograms. Normal container contents are excluded. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Load")
	float GetEquipmentLoadWeight() const { return CurrentEquipmentLoadWeight; }

	/** Returns the current Light, Medium, or Heavy tier selected from the designer thresholds. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Load")
	ERpgEquipmentLoadTier GetEquipmentLoadTier() const { return CurrentEquipmentLoadTier; }

	/** Returns the GAS tag corresponding to the current load tier. Exactly one tier tag is applied on the pawn ASC. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Load")
	FGameplayTag GetEquipmentLoadTierTag() const;

	/** Returns the designer-authored montage/root-motion seam for the current load tier. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Load")
	FRpgEquipmentDodgeProfile GetDodgeProfileForCurrentLoad() const;

	/** Pure threshold helper used by validation and automation tests; values at a threshold enter the higher tier. */
	static ERpgEquipmentLoadTier ResolveLoadTierForWeight(
		float LoadWeight,
		float MediumThreshold = 13.0f,
		float HeavyThreshold = 23.0f);

	/**
	 * Server-authoritatively recalculates weight from data-driven Gear and Carry groups and updates the pawn GAS tier tag.
	 * Inventory reconciliation calls this once after a committed transaction that changes physical equipment locations.
	 */
	void ReconcileEquipmentLoadFromInventory();

	/**
	 * Server-authoritatively rebuilds non-hand projections and runtime grants from physical Gear.* locations, then resolves
	 * the remembered active-hand selection by persistent item id. Call once after graph reconstruction or transfer.
	 */
	bool ReconcilePhysicalEquipmentFromInventory();

	/** Exports active hand selection and remembered pairs as persistent item ids; Gear/Carry placement is not duplicated. */
	FRpgEquipmentSelectionSaveData ExportEquipmentSelection() const;

	/** Server-only restore after the inventory graph and pawn ASC are ready; invalid ids remain holstered. */
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
	bool CanUseEquipmentSlotForOwnedItem(
		ERpgEquipmentSlot EquipmentSlot,
		const URpgInventoryItemInstance* Item) const;
	URpgEquipmentInstance* EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const;
	void UnequipRuntimeSlot(ERpgEquipmentSlot EquipmentSlot);
	URpgEquipmentManagerComponent* FindEquipmentManager() const;
	URpgInventoryManagerComponent* FindOwnerInventory() const;
	URpgPlayerInventoryLayoutComponent* FindPlayerInventoryLayout() const;
	bool HasReadyEquipmentTarget() const;
	void BroadcastSlotsChanged() const;
	void RefreshWeaponAbilityLoadout() const;
	bool IsTwoHandItem(const URpgInventoryItemInstance* Item) const;
	bool IsItemInCarryEquipmentSlot(
		const URpgInventoryItemInstance* Item,
		ERpgEquipmentSlot EquipmentSlot) const;
	URpgInventoryItemInstance* GetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem) const;
	void ApplyEquipmentSelectionPointers(
		const FRpgEquipmentSelectionSaveData& SaveData);
	void RememberCurrentOffhandForActiveMainhand();
	void SetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem, URpgInventoryItemInstance* OffHandItem);
	void ClearRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem);
	bool AssignRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);
	float CalculateEquipmentLoadWeight() const;
	ERpgEquipmentLoadTier ResolveEquipmentLoadTier(float LoadWeight) const;
	void ApplyEquipmentLoadTierTag() const;
	static const URpgEquipmentDefinition* FindEquipmentDefinition(const URpgInventoryItemInstance* Item);
	static FGameplayTag GetTagForEquipmentLoadTier(ERpgEquipmentLoadTier Tier);
	static bool IsRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Owner-only replicated projection; physical placement remains authoritative in the inventory graph. */
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
