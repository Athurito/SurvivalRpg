#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventorySlotGroupViewModel.generated.h"

class UTexture2D;

/**
 * UI projection for one visible player-inventory slot group such as Pockets, Backpack, Belt, or WeaponSlot1.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
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

	/** Explicit static-group role used by authored presenters; invalid for generic and item-owned groups. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FGameplayTag GetSemanticRole() const { return SemanticRole; }

	/** Grid size of this visible container. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	FRpgInventoryGridSize GetGridSize() const { return GridSize; }

	/** Logical slots contained by this group. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	TArray<URpgInventoryAddressSlotViewModel*> GetSlots() const;

	/** True when an equipped backpack, belt, pouch, or resource bag owns this content grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsProvidedByEquipment() const { return bProvidedByEquipment; }

	/** Typed equipment slot that currently provides this item-owned content grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	ERpgEquipmentSlot GetSourceEquipmentSlot() const { return SourceEquipmentSlot; }

	/** Explicit gameplay equipment role for this group, or None for regular/item-owned content storage. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	ERpgEquipmentSlot GetEquipmentSlotRole() const { return EquipmentSlotRole; }

	/** True only for a dedicated authored Carry group with a MainHand or OffHand role. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsCarryGroup() const { return bCarryGroup; }

	/** True only for a regular content-storage group. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Layout ViewModel")
	bool IsContentGroup() const { return bContentGroup; }

protected:
	/** Stable graph address. Item-owned containers with the same local id remain distinct. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryContainerHandle ContainerHandle;

	/** Stable container id such as WeaponSlot1, Pockets, Backpack, or Belt. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FName ContainerId = NAME_None;

	/** Exact semantic singleton role copied from static layout definition data. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTag SemanticRole;

	/** Player-facing group header text. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional header icon for this group. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Grid dimensions for this visible container. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventoryGridSize GridSize;

	/** Explicit gameplay equipment role projected from immutable layout definition data. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgEquipmentSlot EquipmentSlotRole = ERpgEquipmentSlot::None;

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

	/** Typed equipment slot whose item provided this group. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgEquipmentSlot SourceEquipmentSlot = ERpgEquipmentSlot::None;

	/** Slot VMs in stable visual order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Layout ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgInventoryAddressSlotViewModel>> Slots;
};
