// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Interact.h"

#include "RpgInteractionPromptAutomationTestTypes.generated.h"

class URpgIndicatorManagerComponent;

/** Concrete interaction ability fixture used to verify local prompt reconciliation. */
UCLASS(NotBlueprintable, Transient)
class URpgInteractionPromptAutomationAbility final : public URpgGameplayAbility_Interact
{
	GENERATED_BODY()

public:
	void ReconcileForTesting(
		URpgIndicatorManagerComponent* IndicatorManager,
		const TArray<FInteractionOption>& FocusedOptions,
		const TArray<FInteractionOption>& NearbyOptions)
	{
		CurrentOptions = FocusedOptions;
		CurrentNearbyOptions = NearbyOptions;
		ReconcileInteractionIndicators(IndicatorManager);
	}

	void ReconcileFocusForTesting(
		URpgIndicatorManagerComponent* IndicatorManager,
		const TArray<FInteractionOption>& FocusedOptions)
	{
		ReconcileForTesting(
			IndicatorManager,
			FocusedOptions,
			TArray<FInteractionOption>());
	}
};
