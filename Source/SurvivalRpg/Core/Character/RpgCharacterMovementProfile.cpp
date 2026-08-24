// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgCharacterMovementProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCharacterMovementProfile)

bool RpgCharacterMovementRuntime::IsProfileRuntimeValid(
	const FRpgCharacterMovementProfile& Profile)
{
	const float Values[] = {
		Profile.WalkSpeeds.Forward,
		Profile.WalkSpeeds.Sideways,
		Profile.WalkSpeeds.Backward,
		Profile.RunSpeeds.Forward,
		Profile.RunSpeeds.Sideways,
		Profile.RunSpeeds.Backward,
		Profile.MinAnalogGroundSpeed,
		Profile.MaxAcceleration,
		Profile.GroundFriction,
		Profile.BrakingFrictionFactor,
		Profile.BrakingFriction,
		Profile.BrakingDecelerationWithInput,
		Profile.BrakingDecelerationWithoutInput,
		Profile.FreeRotationRateYaw,
		Profile.StationarySpeedThreshold,
		Profile.MoveIntentThreshold,
		Profile.RunInputThreshold,
		Profile.RunInputExitThreshold,
	};
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			return false;
		}
	}

	return Profile.WalkSpeeds.Forward > 0.0f &&
		Profile.WalkSpeeds.Sideways > 0.0f &&
		Profile.WalkSpeeds.Backward > 0.0f &&
		Profile.RunSpeeds.Forward >= Profile.WalkSpeeds.Forward &&
		Profile.RunSpeeds.Sideways >= Profile.WalkSpeeds.Sideways &&
		Profile.RunSpeeds.Backward >= Profile.WalkSpeeds.Backward &&
		Profile.WalkSpeeds.Forward >= Profile.WalkSpeeds.Sideways &&
		Profile.WalkSpeeds.Sideways >= Profile.WalkSpeeds.Backward &&
		Profile.RunSpeeds.Forward >= Profile.RunSpeeds.Sideways &&
		Profile.RunSpeeds.Sideways >= Profile.RunSpeeds.Backward &&
		Profile.MinAnalogGroundSpeed >= 0.0f &&
		Profile.MinAnalogGroundSpeed <= FMath::Min3(
			Profile.WalkSpeeds.Forward,
			Profile.WalkSpeeds.Sideways,
			Profile.WalkSpeeds.Backward) &&
		Profile.MaxAcceleration > 0.0f &&
		Profile.GroundFriction >= 0.0f &&
		Profile.BrakingFrictionFactor >= 0.0f &&
		Profile.BrakingFriction >= 0.0f &&
		Profile.BrakingDecelerationWithInput > 0.0f &&
		Profile.BrakingDecelerationWithoutInput > 0.0f &&
		(Profile.FreeRotationRateYaw == -1.0f || Profile.FreeRotationRateYaw > 0.0f) &&
		Profile.StationarySpeedThreshold > 0.0f &&
		Profile.MoveIntentThreshold >= 0.0f &&
		Profile.MoveIntentThreshold < Profile.RunInputExitThreshold &&
		Profile.RunInputExitThreshold <= Profile.RunInputThreshold &&
		Profile.RunInputThreshold <= 1.0f;
}

float RpgCharacterMovementRuntime::ResolveDirectionalSpeed(
	const FRpgDirectionalGroundSpeeds& Speeds,
	float AbsoluteDirectionAngleDegrees)
{
	if (!FMath::IsFinite(AbsoluteDirectionAngleDegrees))
	{
		return Speeds.Forward;
	}

	const float Angle = FMath::Clamp(FMath::Abs(AbsoluteDirectionAngleDegrees), 0.0f, 180.0f);
	float DirectionMap = 0.0f;
	if (Angle > 45.0f && Angle < 80.0f)
	{
		DirectionMap = FMath::GetMappedRangeValueClamped(
			FVector2D(45.0f, 80.0f),
			FVector2D(0.0f, 1.0f),
			Angle);
	}
	else if (Angle >= 80.0f && Angle <= 100.0f)
	{
		DirectionMap = 1.0f;
	}
	else if (Angle > 100.0f && Angle < 135.0f)
	{
		DirectionMap = FMath::GetMappedRangeValueClamped(
			FVector2D(100.0f, 135.0f),
			FVector2D(1.0f, 2.0f),
			Angle);
	}
	else if (Angle >= 135.0f)
	{
		DirectionMap = 2.0f;
	}

	return DirectionMap <= 1.0f
		? FMath::Lerp(Speeds.Forward, Speeds.Sideways, DirectionMap)
		: FMath::Lerp(Speeds.Sideways, Speeds.Backward, DirectionMap - 1.0f);
}

float RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
	ERpgLocomotionGait Gait,
	bool bUseDirectionalSpeeds,
	float AbsoluteDirectionAngleDegrees,
	const FRpgCharacterMovementProfile& Profile)
{
	const FRpgDirectionalGroundSpeeds& Speeds = Gait == ERpgLocomotionGait::Walk
		? Profile.WalkSpeeds
		: Profile.RunSpeeds;
	return ResolveDirectionalSpeed(
		Speeds,
		bUseDirectionalSpeeds ? AbsoluteDirectionAngleDegrees : 0.0f);
}

float RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(
	bool bHasMovementInput,
	const FRpgCharacterMovementProfile& Profile)
{
	return bHasMovementInput
		? Profile.BrakingDecelerationWithInput
		: Profile.BrakingDecelerationWithoutInput;
}

float RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
	float InputMagnitude,
	const FRpgCharacterMovementProfile& Profile)
{
	if (!IsProfileRuntimeValid(Profile) || !FMath::IsFinite(InputMagnitude))
	{
		return 0.0f;
	}

	const float SafeInputMagnitude = FMath::Clamp(InputMagnitude, 0.0f, 1.0f);
	return SafeInputMagnitude > Profile.MoveIntentThreshold
		? SafeInputMagnitude
		: 0.0f;
}

bool RpgCharacterMovementRuntime::HasMoveIntent(
	float InputMagnitude,
	const FRpgCharacterMovementProfile& Profile)
{
	return ResolvePhysicalInputMagnitude(InputMagnitude, Profile) > 0.0f;
}

ERpgLocomotionGait RpgCharacterMovementRuntime::ResolveDesiredGait(
	float InputMagnitude,
	ERpgLocomotionGait PreviousGait,
	const FRpgCharacterMovementProfile& Profile)
{
	const float SafeInputMagnitude = ResolvePhysicalInputMagnitude(
		InputMagnitude,
		Profile);
	if (SafeInputMagnitude <= 0.0f)
	{
		return ERpgLocomotionGait::Idle;
	}

	if (Profile.bOverrideCharacterMovement &&
		PreviousGait == ERpgLocomotionGait::Run &&
		SafeInputMagnitude >= Profile.RunInputExitThreshold)
	{
		return ERpgLocomotionGait::Run;
	}

	return SafeInputMagnitude >= Profile.RunInputThreshold
		? ERpgLocomotionGait::Run
		: ERpgLocomotionGait::Walk;
}

ERpgLocomotionGait RpgCharacterMovementRuntime::ResolveGroundGait(
	bool bIsMovingOnGround,
	float GroundSpeed,
	float InputMagnitude,
	ERpgLocomotionGait DesiredGait,
	ERpgLocomotionGait PreviousGait,
	ERpgLocomotionGait CoastGaitHint,
	const FRpgCharacterMovementProfile& Profile)
{
	if (!bIsMovingOnGround ||
		!IsProfileRuntimeValid(Profile) ||
		!FMath::IsFinite(GroundSpeed) ||
		GroundSpeed < 0.0f ||
		!FMath::IsFinite(InputMagnitude))
	{
		return ERpgLocomotionGait::Idle;
	}

	const float SafeInputMagnitude = FMath::Clamp(InputMagnitude, 0.0f, 1.0f);
	const bool bHasMoveIntent = HasMoveIntent(SafeInputMagnitude, Profile);
	if (GroundSpeed < Profile.StationarySpeedThreshold && !bHasMoveIntent)
	{
		return ERpgLocomotionGait::Idle;
	}

	if (bHasMoveIntent)
	{
		return DesiredGait == ERpgLocomotionGait::Run
			? ERpgLocomotionGait::Run
			: ERpgLocomotionGait::Walk;
	}

	// The authority publishes only the current Walk/Run coast classification. Prefer it
	// over local history so a newly relevant proxy and an existing proxy converge alike.
	if (Profile.bOverrideCharacterMovement &&
		(CoastGaitHint == ERpgLocomotionGait::Walk ||
		 CoastGaitHint == ERpgLocomotionGait::Run))
	{
		return CoastGaitHint;
	}

	// Preserve the moving database while physical deceleration finishes after input release.
	if (PreviousGait == ERpgLocomotionGait::Walk)
	{
		return ERpgLocomotionGait::Walk;
	}
	if (PreviousGait == ERpgLocomotionGait::Run || !Profile.bOverrideCharacterMovement)
	{
		return ERpgLocomotionGait::Run;
	}

	// A speed fallback keeps malformed or delayed initial snapshots deterministic. The
	// authoritative coast hint replaces this ambiguity as soon as it arrives.
	return GroundSpeed <= Profile.WalkSpeeds.Forward
		? ERpgLocomotionGait::Walk
		: ERpgLocomotionGait::Run;
}
