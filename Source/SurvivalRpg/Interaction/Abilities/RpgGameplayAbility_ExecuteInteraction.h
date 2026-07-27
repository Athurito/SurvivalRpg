// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_ExecuteInteraction.generated.h"

/**
 * Shared server-only ability for simple target-owned interactions such as doors and harvest instances.
 * The target is re-gathered and validated before its CommitInteraction implementation may mutate state.
 */
UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_ExecuteInteraction : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_ExecuteInteraction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
