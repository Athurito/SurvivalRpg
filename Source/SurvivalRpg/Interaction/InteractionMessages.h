// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "InteractionMessages.generated.h"

class AActor;
class UPrimitiveComponent;

/** Server-local GameplayMessage payload describing an authoritative interaction result. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInteractionExecutionMessage
{
	GENERATED_BODY()

	/** Avatar that requested the interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Actor that owned the validated interaction option. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** Concrete hit component, when the interaction addressed one. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPrimitiveComponent> TargetComponent = nullptr;

	/** Stable semantic action id selected by the authoritative query. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InteractionTag;

	/** ISM/HISM instance index, or INDEX_NONE for non-instanced targets. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 InstanceIndex = INDEX_NONE;

	/** Optional machine-readable reason accompanying a rejected or ended interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag ResultTag;

	/** True when the represented phase completed successfully. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSucceeded = false;
};
