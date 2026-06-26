#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "RpgQuickBarComponent.generated.h"

class AActor;
class URpgEquipmentInstance;
class URpgEquipmentManagerComponent;
class URpgInventoryManagerComponent;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgQuickBarLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TObjectPtr<URpgInventoryItemInstance> MainHandItem = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TObjectPtr<URpgInventoryItemInstance> OffHandItem = nullptr;

	URpgInventoryItemInstance* GetItemForSlot(ERpgEquipmentSlot Slot) const;
	void SetItemForSlot(ERpgEquipmentSlot Slot, URpgInventoryItemInstance* Item);
	bool HasAnyItem() const;
};

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgQuickBarComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	URpgQuickBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void CycleActiveSlotForward();

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void CycleActiveSlotBackward();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "QuickBar")
	void SetActiveSlotIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	TArray<URpgInventoryItemInstance*> GetSlots() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	TArray<FRpgQuickBarLoadoutSlot> GetLoadoutSlots() const { return Slots; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	int32 GetActiveSlotIndex() const { return ActiveSlotIndex; }

	/** Number of loadout slots the quickbar should expose to HUD and inventory UI. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	int32 GetNumSlots() const { return NumSlots; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	URpgInventoryItemInstance* GetActiveSlotItem() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	URpgInventoryItemInstance* GetItemInLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	int32 GetNextFreeItemSlot() const;

	/** Client/UI entry point for assigning an owned item to a quickbar hand slot. Server validates inventory ownership and item rules. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "QuickBar")
	void RequestAssignItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Client/UI entry point for swapping two quickbar hand slots. Server validates slot indices and hand-slot usage. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "QuickBar")
	void RequestSwapLoadoutSlots(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot);

	/** Client/UI entry point for clearing one quickbar hand slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "QuickBar")
	void RequestClearLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot);

	/** Returns true when the item is owned by this controller and may be assigned to the requested quickbar hand slot. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	bool CanAssignItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, const URpgInventoryItemInstance* Item) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void AddItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void AddItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Server-side assignment used by UI actions and starter grants. Replaces the target slot after validation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	bool AssignItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	/** Server-side swap used by drag-and-drop quickbar rearranging. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	bool SwapLoadoutSlots(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	URpgInventoryItemInstance* RemoveItemFromSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	URpgInventoryItemInstance* RemoveItemFromLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot);

	/** Clears every quickbar reference to the item, useful when moving an equipped item into storage. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void ClearItemFromAllLoadoutSlots(URpgInventoryItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void UnequipActiveLoadoutFromCurrentPawn();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	bool RefreshActiveLoadoutOnCurrentPawn();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QuickBar", meta = (ClampMin = "1"))
	int32 NumSlots = 8;

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlotIndex();

private:
	void EnsureSlotCount();
	void UnequipItemInSlot();
	void EquipItemInSlot();
	bool IsActiveLoadoutAppliedToCurrentPawn() const;
	void ClearEquippedItemReferences();
	URpgEquipmentInstance* EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const;
	URpgEquipmentManagerComponent* FindEquipmentManager() const;
	URpgInventoryManagerComponent* FindOwnerInventory() const;
	bool HasReadyEquipmentTarget() const;
	void BroadcastSlotsChanged() const;
	void BroadcastActiveIndexChanged() const;
	static bool IsQuickBarEquipmentSlot(ERpgEquipmentSlot EquipmentSlot);
	static bool IsItemAllowedInQuickBarSlot(const URpgInventoryItemInstance* Item, ERpgEquipmentSlot EquipmentSlot);

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgQuickBarLoadoutSlot> Slots;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlotIndex)
	int32 ActiveSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentInstance> MainHandEquippedItem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgEquipmentInstance> OffHandEquippedItem = nullptr;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgQuickBarSlotsChangedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TObjectPtr<AActor> Owner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TArray<TObjectPtr<URpgInventoryItemInstance>> Slots;

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TArray<FRpgQuickBarLoadoutSlot> LoadoutSlots;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgQuickBarActiveIndexChangedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	TObjectPtr<AActor> Owner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "QuickBar")
	int32 ActiveIndex = INDEX_NONE;
};
