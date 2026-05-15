// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RpgAsyncAction_ExperienceReady.generated.h"

class AGameStateBase;
class URpgExperienceDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgExperienceReadyAsyncDelegate);

/**
 * Blueprint async helper that fires once the active gameplay experience is loaded.
 *
 * Useful for UI or Blueprint setup that must wait until GameFeature actions and PawnData
 * selection have completed.
 */
UCLASS()
class SURVIVALRPG_API URpgAsyncAction_ExperienceReady : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Waits for the current world's experience manager to report that the experience is ready. */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static URpgAsyncAction_ExperienceReady* WaitForExperienceReady(UObject* WorldContextObject);

	virtual void Activate() override;

	/** Broadcast after the experience is ready. */
	UPROPERTY(BlueprintAssignable)
	FRpgExperienceReadyAsyncDelegate OnReady;

private:
	void Step1_HandleGameStateSet(AGameStateBase* GameState);
	void Step2_ListenToExperienceLoading(AGameStateBase* GameState);
	void Step3_HandleExperienceLoaded(const URpgExperienceDefinition* CurrentExperience);

private:
	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;
};
