// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SurvivalRpg/Interaction/InteractionTypes.h"

class UIndicatorDescriptor;
struct FInteractionOption;

/** Local-only helpers that map interaction state and target context into projected UI. */
namespace RpgInteractionPresentation
{
	/** Returns whether the state owns the single expanded key-and-text prompt. */
	SURVIVALRPG_API bool IsFullPromptState(ERpgInteractionPromptState State);

	/**
	 * Dedupe nearby options by their projected world slot and select a bounded, deterministic set.
	 * The first pass favors distinct actors so a dense HISM provider cannot consume every marker.
	 */
	SURVIVALRPG_API void SelectNearbyOptionsForDisplay(
		TArray<FInteractionOption>& InOutOptions,
		const FVector& ViewOrigin,
		int32 MaxVisibleOptions);

	/**
	 * Resolves the cosmetic prompt anchor and configures an existing descriptor in place.
	 * Gameplay distance, line of sight, payloads, and authority never read this placement.
	 */
	SURVIVALRPG_API bool ConfigureDescriptorPlacement(
		UIndicatorDescriptor& Descriptor,
		const FInteractionOption& Option);
}
