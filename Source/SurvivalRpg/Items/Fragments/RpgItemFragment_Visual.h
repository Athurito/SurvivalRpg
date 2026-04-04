#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Items/RpgEquipmentBase.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Visual.generated.h"

class UAnimInstance;
class UAnimMontage;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWeaponToolCameraSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bEnabled", ClampMin = "5.0", ClampMax = "170.0"))
	float FOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bEnabled"))
	FVector SpringArmSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", ToolTip = "Shared blend time in seconds for both FOV and SpringArm SocketOffset. 0 snaps immediately, values above 0 smooth the visible weapon or tool camera transition."))
	float BlendTime = 0.0f;

	bool operator==(const FRpgWeaponToolCameraSettings& Other) const
	{
		return bEnabled == Other.bEnabled
			&& FMath::IsNearlyEqual(FOV, Other.FOV)
			&& SpringArmSocketOffset.Equals(Other.SpringArmSocketOffset)
			&& FMath::IsNearlyEqual(BlendTime, Other.BlendTime);
	}

	bool operator!=(const FRpgWeaponToolCameraSettings& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgWeaponToolCharacterSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (EditCondition = "bEnabled", ToolTip = "Optional AnimBlueprint class used while this weapon or tool is visibly drawn. Leave empty to keep the character default AnimClass."))
	TSubclassOf<UAnimInstance> AnimClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float MaxWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (EditCondition = "bEnabled"))
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (EditCondition = "bEnabled"))
	bool bUseControllerDesiredRotation = false;

	bool operator==(const FRpgWeaponToolCharacterSettings& Other) const
	{
		return bEnabled == Other.bEnabled
			&& AnimClass == Other.AnimClass
			&& FMath::IsNearlyEqual(MaxWalkSpeed, Other.MaxWalkSpeed)
			&& bOrientRotationToMovement == Other.bOrientRotationToMovement
			&& bUseControllerDesiredRotation == Other.bUseControllerDesiredRotation;
	}

	bool operator!=(const FRpgWeaponToolCharacterSettings& Other) const
	{
		return !(*this == Other);
	}
};

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
	UAnimMontage* GetEquipMontage() const { return EquipMontage; }
	UAnimMontage* GetUnequipMontage() const { return UnequipMontage; }
	const FRpgWeaponToolCameraSettings& GetWeaponToolCameraSettings() const { return WeaponToolCameraSettings; }
	const FRpgWeaponToolCharacterSettings& GetWeaponToolCharacterSettings() const { return WeaponToolCharacterSettings; }
	bool ShouldHideWhenInactiveWithoutStowedSocket() const { return bHideWhenInactiveWithoutStowedSocket; }
	void SetEquippedActorClass(TSubclassOf<ARpgEquipmentBase> InEquippedActorClass) { EquippedActorClass = InEquippedActorClass; }
	void SetEquippedSocketName(FName InEquippedSocketName) { EquippedSocketName = InEquippedSocketName; }
	void SetStowedSocketName(FName InStowedSocketName) { StowedSocketName = InStowedSocketName; }
	void SetEquippedRelativeTransform(const FTransform& InEquippedRelativeTransform) { EquippedRelativeTransform = InEquippedRelativeTransform; }
	void SetStowedRelativeTransform(const FTransform& InStowedRelativeTransform) { StowedRelativeTransform = InStowedRelativeTransform; }
	void SetEquipMontage(UAnimMontage* InEquipMontage) { EquipMontage = InEquipMontage; }
	void SetUnequipMontage(UAnimMontage* InUnequipMontage) { UnequipMontage = InUnequipMontage; }
	void SetWeaponToolCameraSettings(const FRpgWeaponToolCameraSettings& InWeaponToolCameraSettings) { WeaponToolCameraSettings = InWeaponToolCameraSettings; }
	void SetWeaponToolCharacterSettings(const FRpgWeaponToolCharacterSettings& InWeaponToolCharacterSettings) { WeaponToolCharacterSettings = InWeaponToolCharacterSettings; }
	void SetHideWhenInactiveWithoutStowedSocket(bool bInHideWhenInactiveWithoutStowedSocket) { bHideWhenInactiveWithoutStowedSocket = bInHideWhenInactiveWithoutStowedSocket; }

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional montage played when this weapon or tool becomes active from the holstered state. This is presentation-only and does not delay the authoritative equip switch. Add AnimNotify_RpgWeaponToolPresentation notifies if you want to choose the exact frame where the weapon or tool becomes visible or swaps from the previous set."))
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional montage played when this weapon or tool is holstered by pressing the same slot key again. Add AnimNotify_RpgWeaponToolPresentation notifies if you want to choose the exact frame where the visible weapon or tool detaches, stows, or disappears."))
	TObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Inline camera override for this weapon or tool while it is visibly drawn. FOV and SpringArm SocketOffset are applied when the draw state becomes visible, typically at DrawActiveSet or ApplyCurrentState notifies. BlendTime controls whether that visible camera change snaps or smooths over time."))
	FRpgWeaponToolCameraSettings WeaponToolCameraSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Inline character override for this weapon or tool. MaxWalkSpeed and the rotation flags follow the active weapon set immediately, while AnimClass follows the visible draw state and switches at DrawActiveSet or ApplyCurrentState."))
	FRpgWeaponToolCharacterSettings WeaponToolCharacterSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "If true, the item stays hidden whenever no stowed socket is defined and the weapon set is inactive. Disable this only if you want inactive items to remain visible without a stow location."))
	bool bHideWhenInactiveWithoutStowedSocket = true;
};
