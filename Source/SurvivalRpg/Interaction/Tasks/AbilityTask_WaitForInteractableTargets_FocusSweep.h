// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilityTask_WaitForInteractableTargets.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include "AbilityTask_WaitForInteractableTargets_FocusSweep.generated.h"

/** Periodically selects exactly one camera-focused interaction using a small multi sphere sweep. */
UCLASS()
class SURVIVALRPG_API UAbilityTask_WaitForInteractableTargets_FocusSweep : public UAbilityTask_WaitForInteractableTargets
{
	GENERATED_UCLASS_BODY()

public:
	/** Creates an event-driven focus task; ranges are in centimeters and ScanRate is in seconds. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitForInteractableTargets_FocusSweep* WaitForInteractableTargets_FocusSweep(
		UGameplayAbility* OwningAbility,
		FInteractionQuery InteractionQuery,
		FGameplayAbilityTargetingLocationInfo StartLocation,
		TEnumAsByte<ECollisionChannel> InteractionTraceChannel,
		float MaxFocusRange = 500.0f,
		float ScanRate = 0.05f,
		float SweepRadius = 12.0f,
		int32 MaxCandidates = 32,
		bool bShowDebug = false);

	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;

	/** Performs an immediate query, used by authority when interaction input arrives. */
	void ScanNow();

	/** Copies the selected option after the most recent query. */
	bool GetFocusedOption(FInteractionOption& OutOption) const;

private:
	void PerformSweep();

	UPROPERTY()
	FInteractionQuery InteractionQuery;

	UPROPERTY()
	FGameplayAbilityTargetingLocationInfo StartLocation;

	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_GameTraceChannel1;
	float MaxFocusRange = 500.0f;
	float ScanRate = 0.05f;
	float SweepRadius = 12.0f;
	int32 MaxCandidates = 32;
	bool bShowDebug = false;
	FTimerHandle TimerHandle;
};
