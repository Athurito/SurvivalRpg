// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Value-only playback time conversion shared by cosmetic one-shot animation lifecycles. */
namespace RpgAnimationPlaybackRuntime
{
	/**
	 * Returns seconds of playback until the directional clip end. Paused, non-finite or invalid
	 * playback has no predictable completion and returns MAX_flt so the lifecycle watchdog owns it.
	 */
	inline float RemainingPlaybackSeconds(float AssetTime, float AssetLength, float PlayRate)
	{
		if (!FMath::IsFinite(AssetTime) || !FMath::IsFinite(AssetLength) ||
			!FMath::IsFinite(PlayRate) || AssetLength <= 0.0f ||
			FMath::Abs(PlayRate) <= UE_SMALL_NUMBER)
		{
			return MAX_flt;
		}

		const float ClampedTime = FMath::Clamp(AssetTime, 0.0f, AssetLength);
		return (PlayRate > 0.0f ? AssetLength - ClampedTime : ClampedTime) /
			FMath::Abs(PlayRate);
	}
}
