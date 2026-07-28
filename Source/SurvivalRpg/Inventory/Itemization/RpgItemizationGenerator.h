#pragma once

#include "CoreMinimal.h"
#include "RpgItemizationTypes.h"

class URpgItemizationProfile;

/** Stateless deterministic generator for concrete Diablo-lite item rolls. */
struct SURVIVALRPG_API FRpgItemizationGenerator
{
	/**
	 * Generates one complete item state from the profile and caller-owned random stream.
	 * SourceLevel is authoritative context; the profile applies its offset and clamps the final item level.
	 */
	static bool GenerateItemization(
		const URpgItemizationProfile* Profile,
		int32 SourceLevel,
		FRandomStream& RandomStream,
		FRpgItemizationState& OutState);
};
