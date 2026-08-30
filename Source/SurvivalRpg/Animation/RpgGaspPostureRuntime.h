// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERpgLocomotionGait : uint8;
struct FRpgGaspLocomotionTuning;

/** Pointer-free cosmetic helpers for gait-aware GASP upper-body posture. */
namespace RpgGaspPostureRuntime
{
	/** Resolves the designer-owned relaxed unarmed correction for one CMC gait, in degrees. */
	SURVIVALRPG_API float ResolveTargetCorrectionDegrees(
		ERpgLocomotionGait Gait,
		const FRpgGaspLocomotionTuning& Tuning);

	/** Advances a finite correction toward the gait target without overshoot, in degrees. */
	SURVIVALRPG_API float AdvanceCorrectionDegrees(
		float CurrentCorrectionDegrees,
		ERpgLocomotionGait Gait,
		float DeltaSeconds,
		const FRpgGaspLocomotionTuning& Tuning);
}
