// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameFeatureAction_WorldActionBase.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

void URpgGameFeatureAction_WorldActionBase::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	GameInstanceStartHandles.FindOrAdd(Context) = FWorldDelegates::OnStartGameInstance.AddUObject(
		this,
		&ThisClass::HandleGameInstanceStart,
		FGameFeatureStateChangeContext(Context));

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (Context.ShouldApplyToWorldContext(WorldContext))
		{
			AddToWorld(WorldContext, Context);
		}
	}
}

void URpgGameFeatureAction_WorldActionBase::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	if (FDelegateHandle* FoundHandle = GameInstanceStartHandles.Find(Context))
	{
		FWorldDelegates::OnStartGameInstance.Remove(*FoundHandle);
		GameInstanceStartHandles.Remove(Context);
	}
}

void URpgGameFeatureAction_WorldActionBase::HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext)
{
	if (FWorldContext* WorldContext = GameInstance ? GameInstance->GetWorldContext() : nullptr)
	{
		if (ChangeContext.ShouldApplyToWorldContext(*WorldContext))
		{
			AddToWorld(*WorldContext, ChangeContext);
		}
	}
}
