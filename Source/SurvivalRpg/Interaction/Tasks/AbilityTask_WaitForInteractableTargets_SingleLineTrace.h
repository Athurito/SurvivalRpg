// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityTask_WaitForInteractableTargets.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include "AbilityTask_WaitForInteractableTargets_SingleLineTrace.generated.h"

struct FCollisionProfileName;

class UGameplayAbility;
class UObject;
struct FFrame;

/** Deprecated compatibility task. New abilities should use the deterministic FocusSweep task. */
UCLASS(meta = (DeprecatedNode, DeprecationMessage = "Use WaitForInteractableTargets_FocusSweep."))
class UAbilityTask_WaitForInteractableTargets_SingleLineTrace : public UAbilityTask_WaitForInteractableTargets
{
	GENERATED_UCLASS_BODY()
	
public:

	/** Wait until we trace new set of interactables.  This task automatically loops. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DeprecatedFunction, DeprecationMessage = "Use WaitForInteractableTargets_FocusSweep."))
	static UAbilityTask_WaitForInteractableTargets_SingleLineTrace* WaitForInteractableTargets_SingleLineTrace(UGameplayAbility* OwningAbility, FInteractionQuery InteractionQuery, FCollisionProfileName TraceProfile, FGameplayAbilityTargetingLocationInfo StartLocation, float InteractionScanRange = 100, float InteractionScanRate = 0.100f, bool bShowDebug = false);

	virtual void OnDestroy(bool AbilityEnded) override;
protected:
	virtual void Activate() override;
	
private:
	void PerformTrace();

	UPROPERTY()
	FInteractionQuery InteractionQuery;

	UPROPERTY()
	FGameplayAbilityTargetingLocationInfo StartLocation;

	float InteractionScanRange = 100;
	float InteractionScanRate = 0.100f;
	bool bShowDebug = false;
	float SweepRadius = 12.0f;

	FTimerHandle TimerHandle;
};
