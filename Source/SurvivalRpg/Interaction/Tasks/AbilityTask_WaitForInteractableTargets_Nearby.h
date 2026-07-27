// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilityTask_WaitForInteractableTargets.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"

#include "AbilityTask_WaitForInteractableTargets_Nearby.generated.h"

/** Periodically gathers a bounded set of nearby marker options without using Actor tick. */
UCLASS()
class SURVIVALRPG_API UAbilityTask_WaitForInteractableTargets_Nearby : public UAbilityTask_WaitForInteractableTargets
{
	GENERATED_UCLASS_BODY()

public:
	/** Creates a nearby-overlap task; ScanRange is centimeters and ScanRate is seconds. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitForInteractableTargets_Nearby* WaitForInteractableTargets_Nearby(
		UGameplayAbility* OwningAbility,
		FInteractionQuery InteractionQuery,
		TEnumAsByte<ECollisionChannel> InteractionTraceChannel,
		float ScanRange = 800.0f,
		float ScanRate = 0.25f,
		int32 MaxVisibleOptions = 12,
		bool bShowDebug = false);

	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	void QueryNearby();

	UPROPERTY()
	FInteractionQuery InteractionQuery;

	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_GameTraceChannel1;
	float ScanRange = 800.0f;
	float ScanRate = 0.25f;
	int32 MaxVisibleOptions = 12;
	bool bShowDebug = false;
	FTimerHandle TimerHandle;
};
