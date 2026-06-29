#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"

#include "RpgActionBarComponent.generated.h"

class ARpgPlayerController;

/** Runtime payload type stored by a general actionbar slot. */
UENUM(BlueprintType)
enum class ERpgActionBarSlotType : uint8
{
	/** The slot has no assigned action. */
	Empty,

	/** The slot activates or uses the current item in a bindable player-inventory address such as Belt[0]. */
	InventorySlotBinding,

	/** The slot activates the current item in a carry-slot address such as WeaponSlot1 or ToolSlot1. */
	CarrySlotBinding
};

/** Owner-only replicated state for one general actionbar slot. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgActionBarSlot
{
	GENERATED_BODY()

	/** Current assignment type. This is UI/selection state; gameplay activation is still validated by the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	ERpgActionBarSlotType SlotType = ERpgActionBarSlotType::Empty;

	/** Logical player-inventory slot bound to this actionbar slot. Bindings survive item swaps inside that source slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	FRpgInventorySlotAddress SlotAddress;

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
 * Controller-owned general actionbar for player-configured slot bindings on keys 1..8.
 *
 * The component stores owner-only selection state. Item use, carry activation, and inventory ownership remain
 * validated by the existing server paths; the actionbar never owns item instances directly.
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

	/** Binds this actionbar slot to a bindable non-carry player-inventory slot such as Belt or Pockets. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestBindInventorySlotToSlot(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress);

	/** Binds this actionbar slot to a carry slot such as WeaponSlot1, ShieldSlot, or ToolSlot1. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestBindCarrySlotToSlot(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress);

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
