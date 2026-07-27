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
	void ReconcileFocusForTesting(
		URpgIndicatorManagerComponent* IndicatorManager,
		const TArray<FInteractionOption>& FocusedOptions)
	{
		CurrentOptions = FocusedOptions;
		ReconcileInteractionIndicators(IndicatorManager);
	}
};
