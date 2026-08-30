// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERpgLocomotionGait : uint8;
enum class ERpgCharacterRotationMode : uint8;
struct FRpgGaspLocomotionTuning;

/** Pointer-free cosmetic helpers for gait-aware GASP upper-body posture. */
namespace RpgGaspPostureRuntime
{
	/**
	 * Keeps the relaxed baseline for fallback presentation and all Free locomotion.
	 * A resolved authored profile owns its torso posture in CombatStrafe/Aim.
	 */
	SURVIVALRPG_API bool ShouldApplyCorrection(
		ERpgCharacterRotationMode RotationMode,
		bool bUsesUnarmedFallback);

	/** Resolves the designer-owned relaxed correction baseline for one CMC gait, in degrees. */
	SURVIVALRPG_API float ResolveTargetCorrectionDegrees(
		ERpgLocomotionGait Gait,
		const FRpgGaspLocomotionTuning& Tuning);

	/**
	 * Advances a finite correction toward the gait target without overshoot, in degrees.
	 * When correction is disallowed, the same bounded interpolation decays toward zero.
	 */
	SURVIVALRPG_API float AdvanceCorrectionDegrees(
		float CurrentCorrectionDegrees,
		ERpgLocomotionGait Gait,
		bool bAllowCorrection,
		float DeltaSeconds,
		const FRpgGaspLocomotionTuning& Tuning);
}
