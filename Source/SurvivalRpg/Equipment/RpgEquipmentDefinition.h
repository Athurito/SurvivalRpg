#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "RpgEquipmentDefinition.generated.h"

class AActor;
class URpgAbilitySet;
class URpgEquipmentInstance;

UENUM(BlueprintType)
enum class ERpgEquipmentSlot : uint8
{
	// Invalid or unspecified slot.
	None,

	// Main-hand slot. Primary weapon input usually routes here.
	MainHand,

	// Off-hand slot. Secondary and block input can route here when occupied.
	OffHand
};

UENUM(BlueprintType)
enum class ERpgEquipmentHandOccupancy : uint8
{
	// Only the slot the item is equipped into is occupied.
	SelectedSlotOnly,

	// Equipping this item occupies both hands and conflicts with any off-hand item.
	BothHands
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

	// Ability sets granted while this equipment is active. Use these for weapon attacks, block, and passive effects.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<TObjectPtr<const URpgAbilitySet>> AbilitySetsToGrant;

	// Visual or gameplay-presentational actors spawned and attached while this equipment is active.
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<FRpgEquipmentActorToSpawn> ActorsToSpawn;
};
