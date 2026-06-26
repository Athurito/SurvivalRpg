#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "RpgActionBarComponent.generated.h"

class ARpgPlayerController;
class URpgInventoryItemInstance;

/** Runtime payload type stored by a general actionbar slot. */
UENUM(BlueprintType)
enum class ERpgActionBarSlotType : uint8
{
	/** The slot has no assigned action. */
	Empty,

	/** The slot uses an item instance owned by the player's inventory. */
	InventoryItem,

	/** The slot activates a currently granted gameplay ability identified by AbilityIdTag. */
	Ability
};

/** Owner-only replicated state for one general actionbar slot. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgActionBarSlot
{
	GENERATED_BODY()

	/** Current assignment type. This is UI/selection state; gameplay activation is still validated by the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	ERpgActionBarSlotType SlotType = ERpgActionBarSlotType::Empty;

	/** Inventory item assigned to this slot when SlotType is InventoryItem. The item remains owned by the player inventory. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Semantic ability id assigned to this slot when SlotType is Ability. Runtime input tags are rebound from this id. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	FGameplayTag AbilityIdTag;

	bool IsEmpty() const { return SlotType == ERpgActionBarSlotType::Empty; }
};

/** Gameplay message sent to the owning client when the general actionbar assignment changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgActionBarSlotsChangedMessage
{
	GENERATED_BODY()

	/** Controller that owns the actionbar. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<APlayerController> Owner = nullptr;

	/** Actionbar component that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<UActorComponent> ActionBarComponent = nullptr;
};

/**
 * Controller-owned general actionbar for player-configured items and abilities on keys 1..8.
 *
 * The component stores owner-only selection state. Item use, ability activation, and inventory ownership
 * remain validated by the existing inventory and ability system server paths.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgActionBarComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgActionBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Number of general actionbar slots. V1 is fixed at eight slots. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	int32 GetNumSlots() const { return SlotCount; }

	/** Returns a copy of all owner-only actionbar slots for UI display. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	const TArray<FRpgActionBarSlot>& GetSlots() const { return Slots; }

	/** Returns one actionbar slot, or an empty slot for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	FRpgActionBarSlot GetSlot(int32 SlotIndex) const;

	/** Assigns a player-inventory item to a general actionbar slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestAssignItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* ItemInstance);

	/** Assigns a granted ability id to a general actionbar slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestAssignAbilityToSlot(int32 SlotIndex, FGameplayTag AbilityIdTag);

	/** Clears one general actionbar slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestClearSlot(int32 SlotIndex);

	/** Handles local key/button press for one actionbar slot. */
	void ActivateSlot(int32 SlotIndex);

	/** Handles local key/button release for one actionbar slot. */
	void ReleaseSlot(int32 SlotIndex);

	/** Input tag used by the given actionbar slot index, or invalid for out-of-range indices. */
	static FGameplayTag GetInputTagForSlotIndex(int32 SlotIndex);

protected:
	UFUNCTION()
	void OnRep_Slots();

private:
	void EnsureSlotCount();
	void BroadcastSlotsChanged() const;
	bool IsValidSlotIndex(int32 SlotIndex) const;
	ARpgPlayerController* GetRpgPlayerController() const;

	/** Owner-only replicated actionbar state. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgActionBarSlot> Slots;

	/** Designer-visible slot count kept fixed at 8 for V1. */
	UPROPERTY(EditDefaultsOnly, Category = "Action Bar", meta = (ClampMin = 1, ClampMax = 8))
	int32 SlotCount = 8;
};
