// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureStateChangeObserver.h"
#include "GameFeaturesProjectPolicies.h"
#include "RpgGameFeaturePolicy.generated.h"

class UGameFeatureData;

/**
 * Project policy for discovering, loading, and activating GameFeature plugins.
 *
 * This is the central hook used by the GameFeatures subsystem for preload bundles,
 * client/server data decisions, and feature-state observers.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgGameFeaturePolicy : public UDefaultGameFeaturesProjectPolicies
{
	GENERATED_BODY()

public:
	static URpgGameFeaturePolicy& Get();

	explicit URpgGameFeaturePolicy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UGameFeaturesProjectPolicies interface
	virtual void InitGameFeatureManager() override;
	virtual void ShutdownGameFeatureManager() override;
	virtual TArray<FPrimaryAssetId> GetPreloadAssetListForGameFeature(const UGameFeatureData* GameFeatureToLoad, bool bIncludeLoadedAssets = false) const override;
	virtual const TArray<FName> GetPreloadBundleStateForGameFeature() const override;
	virtual void GetGameFeatureLoadingMode(bool& bLoadClientData, bool& bLoadServerData) const override;
	virtual bool IsPluginAllowed(const FString& PluginURL, FString* OutReason) const override;
	//~ End UGameFeaturesProjectPolicies interface

private:
	/** Observer objects registered with the GameFeatures subsystem for project-specific feature behavior. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Observers;
};

/** Observer that registers GameplayCue notify paths declared by active GameFeature plugins. */
UCLASS()
class URpgGameFeature_AddGameplayCuePaths : public UObject, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL) override;
	virtual void OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL) override;
};
