#pragma once

#include "Components/ControllerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgPlayerInventoryLayoutComponent.generated.h"

class URpgEquipmentLoadoutComponent;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutDefinition;

/** Gameplay message emitted when the player's inventory layout groups or capacity should be refreshed by UI. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPlayerInventoryLayoutChangedMessage
{
	GENERATED_BODY()

	/** Controller that owns the changed player inventory layout. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	TObjectPtr<AActor> Owner = nullptr;

	/** Layout component that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	TObjectPtr<UActorComponent> LayoutComponent = nullptr;

	/** Total grid-cell count after rebuilding fixed and item-provided containers. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	int32 TotalCellCount = 0;
};

/**
 * Controller-owned layout mapper for the player's single inventory manager.
 *
 * The component does not own items. It maps logical containers such as WeaponSlot1, Belt, or Backpack to spatial
 * grid cells on the existing player inventory and validates which items may occupy those addresses.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerInventoryLayoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the immutable layout selected by the owning controller's replicated PlayerState PawnData. */
	const URpgPlayerInventoryLayoutDefinition* GetLayoutDefinition() const;

	/** Re-evaluates PawnData-backed layout state and notifies server gameplay or owning-client presentation. */
	void RefreshLayoutFromPawnData();

	/** Returns all active slot groups in stable visual/global order. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	TArray<FRpgInventorySlotGroupView> GetSlotGroups() const;

	/** Returns the total number of grid cells currently exposed by this layout. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	int32 GetTotalCellCount() const;

	/** Resolves a logical cell address to the authoritative spatial placement used by URpgInventoryManagerComponent. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool ResolveSlotAddress(const FRpgInventorySlotAddress& Address, FRpgInventoryGridPlacement& OutPlacement) const;

	/** Resolves an authoritative spatial placement back to a logical grid-cell address. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool TryMakeSlotAddressFromPlacement(const FRpgInventoryGridPlacement& Placement, FRpgInventorySlotAddress& OutAddress) const;

	/** Returns dimensions for an unambiguous root or item-owned container handle. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool GetGridSizeForContainerHandle(FRpgInventoryContainerHandle ContainerHandle, FRpgInventoryGridSize& OutGridSize) const;

	/** Returns the item currently stored at a logical player-inventory slot address. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	URpgInventoryItemInstance* GetItemInSlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed slot exists and the item satisfies both its group rule and shared Equipment policy. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool CanItemUseSlotAddress(URpgInventoryItemInstance* Item, const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed group may be bound to the 1..8 actionbar. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsSlotAddressActionbarBindable(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed slot and its current item may be bound to the 1..8 actionbar. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool CanBindSlotAddressToActionbar(const FRpgInventorySlotAddress& Address, const URpgInventoryItemInstance* Item) const;

	/** Returns true when the addressed group is a carry slot that activates MainHand or OffHand. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsCarrySlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Returns the data-driven equipment role activated by a carry address, or an invalid tag for non-carry groups. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	FGameplayTag GetCarryActivationRole(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed group is a dedicated gear slot such as Gear.Head or Gear.Backpack. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsGearSlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed group is normal item storage that may receive auto-added inventory items. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsContentSlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Keeps the spatial player inventory grid-driven, then broadcasts that its active layout should be re-evaluated. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Layout")
	void ApplyLayoutCapacityToInventory();

	/** Returns whether this equipment slot is a bag/belt/pouch slot whose item can provide layout groups. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns whether the provider item can leave EquipmentSlot; item-owned contents travel with their provider. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool CanUnequipSlotContainer(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns true only when the complete graph address is one of the built-in root gear containers. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool IsBuiltInGearContainer(FRpgInventoryContainerHandle ContainerHandle);

	/** Builds the logical gear-slot address for an equipment slot such as Head or Backpack. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool TryMakeGearSlotAddress(ERpgEquipmentSlot EquipmentSlot, FRpgInventorySlotAddress& OutAddress);

	/** Resolves a complete built-in root gear address; item-owned containers with the same local id are rejected. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool TryGetEquipmentSlotForGearContainer(FRpgInventoryContainerHandle ContainerHandle, ERpgEquipmentSlot& OutEquipmentSlot);

	/** Built-in group ids used by carry/actionbar UI. */
	static const FName WeaponSlot1GroupId;
	static const FName WeaponSlot2GroupId;
	static const FName ShieldSlotGroupId;
	static const FName PocketsGroupId;
	static const FName GearHeadGroupId;
	static const FName GearChestGroupId;
	static const FName GearHandsGroupId;
	static const FName GearLegsGroupId;
	static const FName GearFeetGroupId;
	static const FName GearBackpackGroupId;
	static const FName GearBeltGroupId;
	static const FName GearPouchGroupId;
	static const FName GearResourceBagGroupId;

private:
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	TArray<FRpgInventorySlotGroupView> BuildSlotGroups() const;
	void AppendGroupViews(const TArray<FRpgInventorySlotGroupDefinition>& GroupDefinitions, bool bProvidedByEquipment, ERpgEquipmentSlot SourceEquipmentSlot, TArray<FRpgInventorySlotGroupView>& OutGroups) const;
	void AppendItemContainerViews(const URpgInventoryItemInstance* ProviderItem, ERpgEquipmentSlot SourceEquipmentSlot, TArray<FRpgInventorySlotGroupView>& OutGroups) const;
	void BroadcastLayoutChanged() const;
	static bool IsBuiltInGearGroupId(FName GroupId);
	static bool TryGetEquipmentSlotForGearGroupId(FName GroupId, ERpgEquipmentSlot& OutEquipmentSlot);
	static FName EquipmentSlotToSourceName(ERpgEquipmentSlot EquipmentSlot);
};
