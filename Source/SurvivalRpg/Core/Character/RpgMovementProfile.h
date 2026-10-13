#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgMovementProfile.generated.h"

/**
 * Shared movement feel of a character: speeds, acceleration, braking, friction, turn rates, air control, and jump.
 * Designer-tuned static data assigned on URpgCharacterMovementComponent; identical on owner, server, simulated proxies,
 * and late joiners, so it is never replicated. Equipment load never selects or changes it; load only selects the dodge.
 * The class defaults are the GASP-11 start values for the Relaxed animation set.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgMovementProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Running speed in cm/s, the same in every direction. Keep it within 0.85-1.15 of the run loops' root speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float RunSpeed = 375.f;

	/** Sprint speed in cm/s, copied into each GAS sprint activation. Must exceed RunSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float SprintSpeed = 585.f;

	/** Crouched walking speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float CrouchSpeed = 200.f;

	/**
	 * Ground acceleration in cm/s² up to RunSpeed. Full movement input has this magnitude, so analog input strength
	 * stays input acceleration divided by this value at every speed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acceleration", meta = (ClampMin = "1.0", ForceUnits = "cm/s^2"))
	float Acceleration = 750.f;

	/** Ground acceleration in cm/s² at SprintSpeed. It blends linearly from Acceleration as speed rises from RunSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acceleration", meta = (ClampMin = "1.0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 350.f;

	/** Ground braking in cm/s² without movement input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Braking", meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float BrakingDeceleration = 1300.f;

	/** Ground braking in cm/s² while input is held above the current maximum speed, such as after releasing sprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Braking", meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float BrakingDecelerationWithInput = 500.f;

	/** Scales ground friction while braking. Zero brakes at the constant braking deceleration alone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Braking", meta = (ClampMin = "0.0"))
	float BrakingFrictionFactor = 0.f;

	/** Ground friction up to RunSpeed: the rate per second at which velocity bends toward a new input direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Friction", meta = (ClampMin = "0.0"))
	float GroundFriction = 4.f;

	/** Ground friction at SprintSpeed. It blends linearly from GroundFriction as speed rises from RunSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Friction", meta = (ClampMin = "0.0"))
	float SprintGroundFriction = 3.f;

	/** Free-movement yaw rate in deg/s on the ground. Zero or less turns at once. The combat stance always turns at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation", meta = (Units = "DegreesPerSecond"))
	float GroundRotationRate = 0.f;

	/** Free-movement yaw rate in deg/s while sprinting. Zero or less turns at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation", meta = (Units = "DegreesPerSecond"))
	float SprintRotationRate = 0.f;

	/** Yaw rate in deg/s while falling, in both rotation modes. Zero or less turns at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rotation", meta = (Units = "DegreesPerSecond"))
	float AirRotationRate = 150.f;

	/** Lateral control while falling, from 0 (none) to 1 (as on the ground). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControl = .15f;

	/** Horizontal braking in cm/s² while falling without input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float FallingBrakingDeceleration = 0.f;

	/** Initial upward speed of a jump in cm/s. The apex height is JumpZVelocity² / (2 × gravity), about 103 cm at 450. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "1.0", Units = "cm/s"))
	float JumpZVelocity = 450.f;

	/** Blend from 0 at RunSpeed to 1 at SprintSpeed for a horizontal speed in cm/s. */
	float GetSprintBlend(float Speed) const;

	/** Ground acceleration in cm/s² at a horizontal speed, tapering from Acceleration to SprintAcceleration. */
	float GetAccelerationAtSpeed(float Speed) const;

	/** Ground friction at a horizontal speed, tapering from GroundFriction to SprintGroundFriction. */
	float GetGroundFrictionAtSpeed(float Speed) const;

	/** True when every value is finite and in range and SprintSpeed exceeds RunSpeed. */
	bool ValidateConfiguration(FText& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
