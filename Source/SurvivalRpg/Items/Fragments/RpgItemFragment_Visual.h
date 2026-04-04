#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Items/RpgEquipmentBase.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Visual.generated.h"

class UAnimMontage;
class UDataAsset;

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
	UDataAsset* GetCameraSettings() const { return CameraSettings; }
	bool ShouldHideWhenInactiveWithoutStowedSocket() const { return bHideWhenInactiveWithoutStowedSocket; }
	void SetEquippedActorClass(TSubclassOf<ARpgEquipmentBase> InEquippedActorClass) { EquippedActorClass = InEquippedActorClass; }
	void SetEquippedSocketName(FName InEquippedSocketName) { EquippedSocketName = InEquippedSocketName; }
	void SetStowedSocketName(FName InStowedSocketName) { StowedSocketName = InStowedSocketName; }
	void SetEquippedRelativeTransform(const FTransform& InEquippedRelativeTransform) { EquippedRelativeTransform = InEquippedRelativeTransform; }
	void SetStowedRelativeTransform(const FTransform& InStowedRelativeTransform) { StowedRelativeTransform = InStowedRelativeTransform; }
	void SetEquipMontage(UAnimMontage* InEquipMontage) { EquipMontage = InEquipMontage; }
	void SetUnequipMontage(UAnimMontage* InUnequipMontage) { UnequipMontage = InUnequipMontage; }
	void SetCameraSettings(UDataAsset* InCameraSettings) { CameraSettings = InCameraSettings; }
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional montage played when this weapon set becomes active from the holstered state. This is presentation-only and does not delay the authoritative equip switch. Add AnimNotify_RpgEquipmentPresentation notifies if you want to choose the exact frame where the weapon becomes visible or swaps from the previous set."))
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional montage played when this weapon set is holstered by pressing the same slot key again. Add AnimNotify_RpgEquipmentPresentation notifies if you want to choose the exact frame where the visible weapon detaches, stows, or disappears."))
	TObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "Optional camera profile data exposed by the active weapon set. Blueprint camera logic can read this from the equipment component and apply it however the project needs."))
	TObjectPtr<UDataAsset> CameraSettings = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true", ToolTip = "If true, the item stays hidden whenever no stowed socket is defined and the weapon set is inactive. Disable this only if you want inactive items to remain visible without a stow location."))
	bool bHideWhenInactiveWithoutStowedSocket = true;
};
