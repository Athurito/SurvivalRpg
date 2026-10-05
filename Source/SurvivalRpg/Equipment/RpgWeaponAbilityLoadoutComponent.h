#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"
#include "RpgAbilityBindingResolver.h"

#include "RpgWeaponAbilityLoadoutComponent.generated.h"

class ARpgPlayerController;
class URpgAbilitySystemComponent;

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

	/** Why this binding is available or blocked. Ambiguous ids are content errors and never activate a random spec. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	ERpgAbilityBindingResolveResult ResolveResult = ERpgAbilityBindingResolveResult::InvalidAbilityId;

	/**
	 * True when AbilityIdTag comes from the skill tree of the weapon in use or the default of a granted ability set
	 * rather than a player selection. Server-derived on every refresh; UI read-only.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon Ability Loadout")
	bool bDefaultSelection = false;
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
 *
 * A slot without a player selection takes the ability the skill tree of the weapon in use assigns to it (see
 * URpgSkillTreeComponent), or else the default its granted ability sets declare: an ability set entry whose InputTag
 * is InputTag.Weapon.Ability.N marks its spec as the default of slot N. Player selections always win, and a default
 * slot follows the current weapon and granted abilities on every refresh.
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

	/**
	 * Selects an ability id for one Q/E/R slot.
	 * Missing grants remain selected but blocked so progression or equipment can make them available later.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RequestAssignAbilityToSlot(int32 SlotIndex, FGameplayTag AbilityIdTag);

	/** Clears the player selection of one Q/E/R slot; a declared default of a granted ability set takes it over. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RequestClearSlot(int32 SlotIndex);

	/** Revalidates selected ability ids against currently granted equipment abilities and updates runtime input tags. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Weapon Abilities")
	void RefreshAbilityBindings();

	/**
	 * Server-side refresh against an explicit ability system: adopts declared defaults for slots without a player
	 * selection, resolves every slot to exactly one granted spec and moves the runtime input tags accordingly.
	 */
	void ApplyAbilityBindings(URpgAbilitySystemComponent& AbilitySystem);

	/** Handles local key/button press for one weapon ability slot. */
	void HandleInputPressed(int32 SlotIndex);

	/** Handles local key/button release for one weapon ability slot. */
	void HandleInputReleased(int32 SlotIndex);

	/** Input tag used by the given weapon ability slot index, or invalid for out-of-range indices. */
	static FGameplayTag GetInputTagForSlotIndex(int32 SlotIndex);

	/** Slot index driven by InputTag (InputTag.Weapon.Ability.1..3), or INDEX_NONE for any other tag. */
	static int32 GetSlotIndexForInputTag(FGameplayTag InputTag);

	/** Spec-source marker that declares an ability as the default occupant of a slot, or invalid for out-of-range indices. */
	static FGameplayTag GetDefaultSelectionTagForSlotIndex(int32 SlotIndex);

	/**
	 * Resolves the ability id that granted ability sets declare as the default of SlotIndex.
	 * Returns Unique with OutAbilityIdTag set, Missing when nothing declares the slot, Ambiguous when different
	 * granted abilities claim it, and InvalidAbilityId when the declaring entry has no usable ability id.
	 */
	static ERpgAbilityBindingResolveResult ResolveDefaultAbilityId(
		const URpgAbilitySystemComponent& AbilitySystem,
		int32 SlotIndex,
		FGameplayTag& OutAbilityIdTag);

protected:
	UFUNCTION()
	void OnRep_Slots();

private:
	/** Ability id the active weapon's skill tree places on SlotIndex, when that ability is granted; otherwise empty. */
	FGameplayTag ResolveSkillTreeAbilityId(const URpgAbilitySystemComponent& AbilitySystem, int32 SlotIndex) const;

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
