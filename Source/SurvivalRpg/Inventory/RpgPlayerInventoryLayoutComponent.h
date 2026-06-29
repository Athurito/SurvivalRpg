#pragma once

#include "Components/ControllerComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgPlayerInventoryLayoutComponent.generated.h"

class URpgEquipmentLoadoutComponent;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

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

	/** Total slot count after rebuilding fixed and bag-provided groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	int32 TotalSlotCount = 0;
};

/**
 * Controller-owned layout mapper for the player's single inventory manager.
 *
 * The component does not own items. It maps logical slot groups such as WeaponSlot1, Belt, or Backpack to global
 * SortIndex slots on the existing player inventory and validates which items may occupy those addresses.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerInventoryLayoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgPlayerInventoryLayoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/** Returns all active slot groups in stable visual/global order. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	TArray<FRpgInventorySlotGroupView> GetSlotGroups() const;

	/** Returns the total number of global slots currently exposed by this layout. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	int32 GetTotalSlotCount() const;

	/** Resolves a logical slot address to the global SortIndex slot used by URpgInventoryManagerComponent. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool ResolveSlotAddress(const FRpgInventorySlotAddress& Address, int32& OutGlobalSlotIndex) const;

	/** Resolves a global SortIndex slot back to its logical group address. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool TryMakeSlotAddressFromGlobalSlotIndex(int32 GlobalSlotIndex, FRpgInventorySlotAddress& OutAddress) const;

	/** Returns the item currently stored at a logical player-inventory slot address. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	URpgInventoryItemInstance* GetItemInSlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed slot exists and the item satisfies the group's server-side rule. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool CanItemUseSlotAddress(URpgInventoryItemInstance* Item, const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed group may be bound to the 1..8 actionbar. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsSlotAddressActionbarBindable(const FRpgInventorySlotAddress& Address) const;

	/** Returns true when the addressed group is a carry slot that activates MainHand or OffHand. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool IsCarrySlotAddress(const FRpgInventorySlotAddress& Address) const;

	/** Applies the current layout slot count to the player inventory as a fixed entry capacity on the server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Layout")
	void ApplyLayoutCapacityToInventory();

	/** Returns whether this equipment slot is a bag/belt/pouch slot whose item can provide layout groups. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);

	/** Returns true when the provider item in EquipmentSlot can be removed without hiding occupied slots. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Layout")
	bool CanUnequipSlotContainer(ERpgEquipmentSlot EquipmentSlot) const;

	/** Returns true when a logical group id is one of the built-in carry slot groups. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Layout")
	static bool IsBuiltInCarryGroupId(FName GroupId);

	/** Built-in group ids used by carry/actionbar UI. */
	static const FName WeaponSlot1GroupId;
	static const FName WeaponSlot2GroupId;
	static const FName ShieldSlotGroupId;
	static const FName ToolSlot1GroupId;
	static const FName ToolSlot2GroupId;
	static const FName PocketsGroupId;

private:
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	URpgEquipmentLoadoutComponent* FindEquipmentLoadout() const;
	TArray<FRpgInventorySlotGroupView> BuildSlotGroups() const;
	void AppendGroupViews(const TArray<FRpgInventorySlotGroupDefinition>& GroupDefinitions, bool bProvidedByEquipment, ERpgEquipmentSlot SourceEquipmentSlot, TArray<FRpgInventorySlotGroupView>& OutGroups, int32& InOutFirstGlobalSlotIndex) const;
	void BroadcastLayoutChanged() const;
	static FName EquipmentSlotToSourceName(ERpgEquipmentSlot EquipmentSlot);
	static FRpgInventorySlotGroupDefinition MakeStaticGroup(FName GroupId, const FText& DisplayName, int32 SlotCount, const TArray<ERpgInventoryItemCategory>& AllowedCategories, bool bActionbarBindable, bool bCarrySlot);

	/** Built-in body/carry slot groups. Runtime bag/provider groups are appended after these. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (AllowPrivateAccess = "true", TitleProperty = "GroupId"))
	TArray<FRpgInventorySlotGroupDefinition> StaticSlotGroups;
};
