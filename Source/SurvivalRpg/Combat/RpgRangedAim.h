#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "RpgRangedAim.generated.h"

class URpgCameraMode;

/**
 * The aim stance of a ranged weapon, such as the bow: what holding the aim input or drawing a shot does to movement and
 * camera. Designer-tuned static data on the weapon instance; Hold Ranged Aim From Weapon reads it once per activation.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRangedAimDefinition
{
	GENERATED_BODY()

	/**
	 * Movement speed cap in cm/s while the stance is held, before Ability.Tuning.Ranged.AimMovement; zero for none. The
	 * owner predicts it and the server applies its own token, so it is never sent by the client.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MovementSpeedLimit = 180.0f;

	/** Local camera while the stance is held, such as the bow's shoulder camera; null keeps the current camera. Cosmetic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	TSubclassOf<URpgCameraMode> CameraMode;
};
