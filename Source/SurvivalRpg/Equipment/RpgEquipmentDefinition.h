#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "RpgEquipmentDefinition.generated.h"

class AActor;
class UAnimMontage;
class URpgAbilitySet;
class URpgEquipmentInstance;

/** Server-authoritative equipment load tier used to select dodge presentation and root motion. */
UENUM(BlueprintType)
enum class ERpgEquipmentLoadTier : uint8
{
	/** Aggregate Gear and Carry weight is below the medium threshold. */
	Light,

	/** Aggregate Gear and Carry weight is at least the medium threshold but below the heavy threshold. */
	Medium,

	/** Aggregate Gear and Carry weight is at least the heavy threshold. */
	Heavy
};

/** Designer-authored dodge assets selected by the current equipment load tier. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentDodgeProfile
{
	GENERATED_BODY()

	/** Optional dodge montage for this load tier. The dodge ability owns loading and playback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Dodge", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UAnimMontage> Montage;

	/** Semantic root-motion profile consumed by the dodge ability or animation layer. None uses its normal default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Dodge")
	FName RootMotionProfile = NAME_None;
};

UENUM(BlueprintType)
enum class ERpgEquipmentSlot : uint8
{
	// Invalid or unspecified slot.
	None,

	// Main-hand slot. Primary weapon input usually routes here.
	MainHand,

	// Off-hand slot. Secondary and block input can route here when occupied.
	OffHand,

	// Head armor slot. Does not route weapon input.
	Head,

	// Chest armor slot. Does not route weapon input.
	Chest,

	// Hands armor slot. Does not route weapon input.
	Hands,

	// Legs armor slot. Does not route weapon input.
	Legs,

	// Feet armor slot. Does not route weapon input.
	Feet,

	// Backpack equipment slot. Provides inventory slot groups; does not route weapon input.
	Backpack,

	// Belt equipment slot. Provides bindable inventory slot groups; does not route weapon input.
	Belt,

	// Pouch equipment slot. Provides inventory slot groups; does not route weapon input.
	Pouch,

	// Resource bag equipment slot. Provides filtered inventory slot groups; does not route weapon input.
	ResourceBag
};

UENUM(BlueprintType)
enum class ERpgEquipmentHandOccupancy : uint8
{
	// Only the slot the item is equipped into is occupied.
	SelectedSlotOnly,

	// Equipping this item occupies both hands and conflicts with any off-hand item.
	BothHands
};

UENUM(BlueprintType)
enum class ERpgEquipmentAbilityGrantPolicy : uint8
{
	// Grants this AbilitySet whenever the item is equipped in the configured slot.
	AlwaysForSlot,

	// Grants this AbilitySet only to the item currently selected as the block source.
	ActiveBlockSourceOnly
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentSlotAbilitySet
{
	GENERATED_BODY()

	// Slot that must contain this equipment item before the AbilitySet can be granted.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	ERpgEquipmentSlot EquippedSlot = ERpgEquipmentSlot::MainHand;

	// Determines whether the AbilitySet is always granted for the slot or only for the active block source.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	ERpgEquipmentAbilityGrantPolicy GrantPolicy = ERpgEquipmentAbilityGrantPolicy::AlwaysForSlot;

	// AbilitySet granted when this slot grant is active. Keep input-specific grants in small, explicit AbilitySets.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	TObjectPtr<const URpgAbilitySet> AbilitySet = nullptr;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentActorToSpawn
{
	GENERATED_BODY()

	// Actor class spawned when the equipment is equipped, usually the visible weapon or shield actor.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	TSubclassOf<AActor> ActorToSpawn;

	// Fallback socket used when no hand-specific socket is set.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	FName AttachSocket;

	// Socket used when this equipment is equipped in MainHand.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	FName MainHandAttachSocket;

	// Socket used when this equipment is equipped in OffHand.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	FName OffHandAttachSocket;

	// Relative transform applied after the actor is attached to the chosen socket.
	UPROPERTY(EditAnywhere, Category = "Equipment")
	FTransform AttachTransform = FTransform::Identity;

	FName GetAttachSocketForSlot(ERpgEquipmentSlot Slot) const;
};

UCLASS(Blueprintable, Const, Abstract, BlueprintType)
class SURVIVALRPG_API URpgEquipmentDefinition : public UObject
{
	GENERATED_BODY()

public:
	URpgEquipmentDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	bool CanEquipInSlot(ERpgEquipmentSlot Slot) const;
	bool OccupiesSlot(ERpgEquipmentSlot EquippedSlot, ERpgEquipmentSlot QuerySlot) const;
	ERpgEquipmentSlot GetDefaultEquipSlot() const;

	// Runtime instance class created when this equipment is equipped. Use URpgWeaponInstance for weapons/shields.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TSubclassOf<URpgEquipmentInstance> InstanceType;

	// Slots this equipment may be equipped into. Leave empty only for content that should never be equipped directly.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Slots")
	TArray<ERpgEquipmentSlot> AllowedSlots;

	// Defines whether this item occupies only its selected slot or both hands.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Slots")
	ERpgEquipmentHandOccupancy HandOccupancy = ERpgEquipmentHandOccupancy::SelectedSlotOnly;

	/**
	 * Load contributed while the physical item is in a Gear or Carry container, in kilograms.
	 * Contents of equipped bags and pouches never contribute through this value.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Load", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "kg"))
	float EquipLoadWeight = 0.0f;

	// AbilitySets granted whenever this equipment is active. Prefer SlotAbilitySetsToGrant for hand/input actions.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<TObjectPtr<const URpgAbilitySet>> AbilitySetsToGrant;

	// Slot-specific AbilitySets granted by the equipment manager after resolving hand role and block ownership.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment|Slot Grants", meta = (TitleProperty = "AbilitySet"))
	TArray<FRpgEquipmentSlotAbilitySet> SlotAbilitySetsToGrant;

	// Visual or gameplay-presentational actors spawned and attached while this equipment is active.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<FRpgEquipmentActorToSpawn> ActorsToSpawn;
};
