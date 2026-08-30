// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGaspPostureRuntime.h"

#include "RpgGaspLocomotionConfig.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementProfile.h"

namespace
{
float ResolveValidTargetCorrectionDegrees(
	ERpgLocomotionGait Gait,
	const FRpgGaspLocomotionTuning& Tuning)
{
	switch (Gait)
	{
	case ERpgLocomotionGait::Walk:
		return Tuning.UnarmedWalkPostureCorrectionDegrees;
	case ERpgLocomotionGait::Run:
	case ERpgLocomotionGait::Sprint:
		return Tuning.UnarmedRunPostureCorrectionDegrees;
	case ERpgLocomotionGait::Idle:
	default:
		return Tuning.UnarmedIdlePostureCorrectionDegrees;
	}
}
}

float RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
	ERpgLocomotionGait Gait,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (!RpgGaspLocomotionConfig::IsPostureTuningRuntimeValid(Tuning))
	{
		return 0.0f;
	}

	return ResolveValidTargetCorrectionDegrees(Gait, Tuning);
}

float RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
	float CurrentCorrectionDegrees,
	ERpgLocomotionGait Gait,
	float DeltaSeconds,
	const FRpgGaspLocomotionTuning& Tuning)
{
	if (!RpgGaspLocomotionConfig::IsPostureTuningRuntimeValid(Tuning))
	{
		return 0.0f;
	}
	const float TargetCorrectionDegrees = ResolveValidTargetCorrectionDegrees(Gait, Tuning);

	const float SafeCurrentCorrection = FMath::IsFinite(CurrentCorrectionDegrees)
		? FMath::Clamp(
			CurrentCorrectionDegrees,
			0.0f,
			Tuning.UnarmedRunPostureCorrectionDegrees)
		: 0.0f;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
	{
		return SafeCurrentCorrection;
	}

	return FMath::FInterpConstantTo(
		SafeCurrentCorrection,
		TargetCorrectionDegrees,
		DeltaSeconds,
		Tuning.UnarmedPostureCorrectionSpeed);
}
