// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "RpgGameFeatureAction_WorldActionBase.generated.h"

struct FWorldContext;

/**
 * Base class for GameFeatureActions that need to apply work to active game worlds.
 *
 * This keeps feature activation world-aware, so actions can ignore editor/preview worlds
 * and safely run once for each relevant runtime world.
 */
UCLASS(Abstract)
class SURVIVALRPG_API URpgGameFeatureAction_WorldActionBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

private:
	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

	/** Override with action-specific logic for one eligible world. */
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) PURE_VIRTUAL(URpgGameFeatureAction_WorldActionBase::AddToWorld, );

private:
	/** Handles used to unregister from delayed GameInstance-start callbacks per activation context. */
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;
};
