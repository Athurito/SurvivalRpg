// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RpgCharacterMovementProfile.generated.h"

/**
 * CMC-owned locomotion gait shared with the animation presentation layer.
 *
 * Walk and Run are reconstructed from CharacterMovement input and the predicted SavedMove gait.
 * Sprint is reserved for the explicit gameplay-owned sprint slice and is never inferred here.
 */
UENUM(BlueprintType)
enum class ERpgLocomotionGait : uint8
{
	Idle,
	Walk,
	Run,
	Sprint,
};

/** Designer-authored standing speed caps for forward, sideways, and backward movement. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgDirectionalGroundSpeeds
{
	GENERATED_BODY()

	FRpgDirectionalGroundSpeeds() = default;

	FRpgDirectionalGroundSpeeds(float InForward, float InSideways, float InBackward)
		: Forward(InForward)
		, Sideways(InSideways)
		, Backward(InBackward)
	{
	}

	/** Forward standing speed cap, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Forward = 500.0f;

	/** Sideways standing speed cap, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Sideways = 350.0f;

	/** Backward standing speed cap, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Backward = 300.0f;
};

/**
 * Static movement contract selected by PawnData and applied identically on every network role.
 *
 * Defaults preserve the existing Lyra-derived character feel. Concrete PawnData assets may tune
 * the CMC physics and gait thresholds without moving gameplay authority into an AnimBlueprint.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCharacterMovementProfile
{
	GENERATED_BODY()

	/**
	 * Whether this PawnData overrides standing-ground CMC response with the values below.
	 * Leave disabled for legacy PawnData so existing Blueprint-authored physics remain untouched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Profile")
	bool bOverrideCharacterMovement = false;

	/** Validated GASP Walk caps; #99 activates forward only while directional prediction remains deferred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed")
	FRpgDirectionalGroundSpeeds WalkSpeeds = {200.0f, 180.0f, 150.0f};

	/** Validated GASP Run caps; #99 activates forward only while directional prediction remains deferred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed")
	FRpgDirectionalGroundSpeeds RunSpeeds = {500.0f, 350.0f, 300.0f};

	/** Minimum ground speed produced by input above the physical deadzone, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinAnalogGroundSpeed = 0.0f;

	/** Maximum standing input acceleration used by prediction and replicated acceleration reconstruction, in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float MaxAcceleration = 800.0f;

	/** Ground friction applied while walking; higher values turn velocity more aggressively. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "0.0"))
	float GroundFriction = 5.0f;

	/** Whether walking braking uses BrakingFriction instead of GroundFriction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response")
	bool bUseSeparateBrakingFriction = false;

	/** Multiplier applied to the effective braking friction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "0.0"))
	float BrakingFrictionFactor = 1.0f;

	/** Separate braking friction used only when bUseSeparateBrakingFriction is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "0.0"))
	float BrakingFriction = 6.0f;

	/** Constant standing-walk deceleration applied while acceleration input is present, in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float BrakingDecelerationWithInput = 500.0f;

	/** Walking deceleration used after movement input is released, in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Response", meta = (ClampMin = "1.0", Units = "cm/s^2"))
	float BrakingDecelerationWithoutInput = 2000.0f;

	/** Free-facing CharacterMovement yaw rate in degrees per second; -1 means effectively immediate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation", meta = (ClampMin = "-1.0", Units = "deg/s"))
	float FreeRotationRateYaw = -1.0f;

	/** Ground speed below which no-input movement resolves to Idle, in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.001", Units = "cm/s"))
	float StationarySpeedThreshold = 3.0f;

	/**
	 * Inclusive standing-ground CMC deadzone in normalized input units.
	 * Input at or below this value is zeroed before ground physics and SavedMove recording.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveIntentThreshold = 0.1f;

	/** Normalized input at or above which Walk enters the predicted Run gait. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RunInputThreshold = 0.65f;

	/**
	 * Normalized input below which the predicted Run gait exits to Walk.
	 * Must not exceed RunInputThreshold; equal values preserve stateless legacy behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RunInputExitThreshold = 0.65f;
};

/** Value-only validation and gait resolution used by CMC runtime code and automation tests. */
namespace RpgCharacterMovementRuntime
{
	/** Checks finite physical values, ordered directional speed bounds, and normalized gait thresholds. */
	SURVIVALRPG_API bool IsProfileRuntimeValid(const FRpgCharacterMovementProfile& Profile);

	/** Resolves normalized input after the profile's inclusive physical deadzone. */
	SURVIVALRPG_API float ResolvePhysicalInputMagnitude(
		float InputMagnitude,
		const FRpgCharacterMovementProfile& Profile);

	/** Returns movement intent from the deadzone-resolved CMC analog-input snapshot. */
	SURVIVALRPG_API bool HasMoveIntent(
		float InputMagnitude,
		const FRpgCharacterMovementProfile& Profile);

	/** Resolves predicted Idle/Walk/Run input gait with opt-in Run hysteresis; never infers Sprint. */
	SURVIVALRPG_API ERpgLocomotionGait ResolveDesiredGait(
		float InputMagnitude,
		ERpgLocomotionGait PreviousGait,
		const FRpgCharacterMovementProfile& Profile);

	/**
	 * Validates the move's recorded gait without repeating its hysteresis decision after base-space
	 * network quantization. Run still requires its minimum input, allowing only Quantize10 roundoff.
	 */
	SURVIVALRPG_API ERpgLocomotionGait ResolveSavedMoveDesiredGait(
		float InputMagnitude,
		bool bSavedRunGait,
		float MaxAcceleration,
		const FRpgCharacterMovementProfile& Profile);

	/** Reproduces GASP's forward/side/back mapping from an absolute local movement angle. */
	SURVIVALRPG_API float ResolveDirectionalSpeed(
		const FRpgDirectionalGroundSpeeds& Speeds,
		float AbsoluteDirectionAngleDegrees);

	/** Resolves the physical standing speed cap for the CMC-owned gait. */
	SURVIVALRPG_API float ResolveGroundSpeedCap(
		ERpgLocomotionGait Gait,
		bool bUseDirectionalSpeeds,
		float AbsoluteDirectionAngleDegrees,
		const FRpgCharacterMovementProfile& Profile);

	/** Resolves GASP-style walking braking from whether any finite CMC input is present. */
	SURVIVALRPG_API float ResolveGroundBrakingDeceleration(
		bool bHasMovementInput,
		const FRpgCharacterMovementProfile& Profile);

	/**
	 * Resolves a stable grounded Idle/Walk/Run gait.
	 * A server gait hint reconstructs active input and coast on newly relevant simulated proxies.
	 */
	SURVIVALRPG_API ERpgLocomotionGait ResolveGroundGait(
		bool bIsMovingOnGround,
		float GroundSpeed,
		float InputMagnitude,
		ERpgLocomotionGait DesiredGait,
		ERpgLocomotionGait PreviousGait,
		ERpgLocomotionGait AuthoritativeGaitHint,
		const FRpgCharacterMovementProfile& Profile);
}
