#pragma once

#include "CoreMinimal.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Visual.generated.h"

class ARpgEquipmentBase;

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemFragment_Visual : public URpgItemFragment
{
	GENERATED_BODY()

public:
	TSubclassOf<ARpgEquipmentBase> GetEquippedActorClass() const { return EquippedActorClass; }
	const FName& GetEquippedSocketName() const { return EquippedSocketName; }
	const FName& GetStowedSocketName() const { return StowedSocketName; }
	const FTransform& GetEquippedRelativeTransform() const { return EquippedRelativeTransform; }
	const FTransform& GetStowedRelativeTransform() const { return StowedRelativeTransform; }
	bool ShouldHideWhenInactiveWithoutStowedSocket() const { return bHideWhenInactiveWithoutStowedSocket; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Actor spawned locally to represent this item on the character. Usually this is your weapon actor blueprint or a lightweight visual actor."))
	TSubclassOf<ARpgEquipmentBase> EquippedActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Socket used while the weapon set is active and the item is in hand. Typical examples are hand_r_socket or hand_l_socket."))
	FName EquippedSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional socket used while the item is equipped but belongs to an inactive weapon set. Leave empty to hide the item unless hiding is disabled below."))
	FName StowedSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Relative transform applied when the visual actor is attached to the equipped socket. Use this to align the weapon correctly in hand."))
	FTransform EquippedRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Relative transform applied when the visual actor is attached to the stowed socket. Use this to align the weapon on the back, hip, or belt."))
	FTransform StowedRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "If true, the item stays hidden whenever no stowed socket is defined and the weapon set is inactive. Disable this only if you want inactive items to remain visible without a stow location."))
	bool bHideWhenInactiveWithoutStowedSocket = true;
};
