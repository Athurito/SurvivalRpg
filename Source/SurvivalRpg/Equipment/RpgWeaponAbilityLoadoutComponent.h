#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "RpgWeaponAbilityLoadoutComponent.generated.h"

class ARpgPlayerController;

/** Owner-only replicated state for one Q/E/R weapon ability slot. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWeaponAbilityLoadoutSlot
{
	GENERATED_BODY()

	/** Semantic ability id selected for this slot. The id must match a currently granted ability spec. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	FGameplayTag AbilityIdTag;

	/** True when the selected ability id is currently granted and bound to this slot's runtime input tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	bool bAvailable = false;
};

/** Gameplay message sent to the owning client when Q/E/R weapon ability assignments or availability change. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWeaponAbilityLoadoutChangedMessage
{
	GENERATED_BODY()

	/** Controller that owns the weapon ability loadout. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	TObjectPtr<APlayerController> Owner = nullptr;

	/** Loadout component that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	TObjectPtr<UActorComponent> LoadoutComponent = nullptr;
};

/**
 * Controller-owned Q/E/R weapon ability loadout.
 *
 * Equipment still grants abilities through the normal equipment manager. This component only chooses
 * which granted ability ids are currently bound to InputTag.Weapon.Ability.1..3.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgWeaponAbilityLoadoutComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgWeaponAbilityLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Number of weapon ability slots. V1 is fixed at Q/E/R. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Weapon Abilities")
	int32 GetNumSlots() const { return SlotCount; }

	/** Returns owner-only weapon ability slot state for UI display. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Weapon Abilities")
	const TArray<FRpgWeaponAbilityLoadoutSlot>& GetSlots() const { return Slots; }

	/** Returns one weapon ability slot, or an empty slot for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Weapon Abilities")
	FRpgWeaponAbilityLoadoutSlot GetSlot(int32 SlotIndex) const;

	/** Selects a granted ability id for one Q/E/R slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RequestAssignAbilityToSlot(int32 SlotIndex, FGameplayTag AbilityIdTag);

	/** Clears one Q/E/R weapon ability slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RequestClearSlot(int32 SlotIndex);

	/** Revalidates selected ability ids against currently granted equipment abilities and updates runtime input tags. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RefreshAbilityBindings();

	/** Handles local key/button press for one weapon ability slot. */
	void HandleInputPressed(int32 SlotIndex);

	/** Handles local key/button release for one weapon ability slot. */
	void HandleInputReleased(int32 SlotIndex);

	/** Input tag used by the given weapon ability slot index, or invalid for out-of-range indices. */
	static FGameplayTag GetInputTagForSlotIndex(int32 SlotIndex);

protected:
	UFUNCTION()
	void OnRep_Slots();

private:
	void EnsureSlotCount();
	void BroadcastSlotsChanged() const;
	bool IsValidSlotIndex(int32 SlotIndex) const;
	ARpgPlayerController* GetRpgPlayerController() const;

	/** Owner-only replicated Q/E/R ability selection state. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FRpgWeaponAbilityLoadoutSlot> Slots;

	/** Designer-visible slot count kept fixed at 3 for V1. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon Ability Loadout", meta = (ClampMin = 1, ClampMax = 3))
	int32 SlotCount = 3;
};
