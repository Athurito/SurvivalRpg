#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "RpgQuickBarComponent.generated.h"

class AActor;
class URpgEquipmentInstance;
class URpgEquipmentManagerComponent;

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

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	URpgInventoryItemInstance* GetActiveSlotItem() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	URpgInventoryItemInstance* GetItemInLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "QuickBar")
	int32 GetNextFreeItemSlot() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void AddItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	void AddItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	URpgInventoryItemInstance* RemoveItemFromSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "QuickBar")
	URpgInventoryItemInstance* RemoveItemFromLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot);

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
	URpgEquipmentInstance* EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const;
	URpgEquipmentManagerComponent* FindEquipmentManager() const;
	void BroadcastSlotsChanged() const;
	void BroadcastActiveIndexChanged() const;

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
