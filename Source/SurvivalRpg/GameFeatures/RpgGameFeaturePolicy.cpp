// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameFeaturePolicy.h"

#include "AbilitySystemGlobals.h"
#include "GameFeatureData.h"
#include "GameplayCueManager.h"
#include "GameFeaturesSubsystem.h"
#include "RpgGameFeatureAction_AddGameplayCuePath.h"

URpgGameFeaturePolicy::URpgGameFeaturePolicy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

URpgGameFeaturePolicy& URpgGameFeaturePolicy::Get()
{
	return UGameFeaturesSubsystem::Get().GetPolicy<URpgGameFeaturePolicy>();
}

void URpgGameFeaturePolicy::InitGameFeatureManager()
{
	Observers.Add(NewObject<URpgGameFeature_AddGameplayCuePaths>());

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.AddObserver(Observer, UGameFeaturesSubsystem::EObserverPluginStateUpdateMode::CurrentAndFuture);
	}

	Super::InitGameFeatureManager();
}

void URpgGameFeaturePolicy::ShutdownGameFeatureManager()
{
	Super::ShutdownGameFeatureManager();

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.RemoveObserver(Observer);
	}

	Observers.Empty();
}

TArray<FPrimaryAssetId> URpgGameFeaturePolicy::GetPreloadAssetListForGameFeature(const UGameFeatureData* GameFeatureToLoad, bool bIncludeLoadedAssets) const
{
	return Super::GetPreloadAssetListForGameFeature(GameFeatureToLoad, bIncludeLoadedAssets);
}

const TArray<FName> URpgGameFeaturePolicy::GetPreloadBundleStateForGameFeature() const
{
	return Super::GetPreloadBundleStateForGameFeature();
}

void URpgGameFeaturePolicy::GetGameFeatureLoadingMode(bool& bLoadClientData, bool& bLoadServerData) const
{
	bLoadClientData = !IsRunningDedicatedServer();
	bLoadServerData = !IsRunningClientOnly();
}

bool URpgGameFeaturePolicy::IsPluginAllowed(const FString& PluginURL, FString* OutReason) const
{
	return Super::IsPluginAllowed(PluginURL, OutReason);
}

void URpgGameFeature_AddGameplayCuePaths::OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	const FString PluginRootPath = TEXT("/") + PluginName;

	for (const UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddGameplayCuePath* AddGameplayCueAction = Cast<URpgGameFeatureAction_AddGameplayCuePath>(Action);
		if (!AddGameplayCueAction)
		{
			continue;
		}

		if (UGameplayCueManager* GameplayCueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			for (const FDirectoryPath& Directory : AddGameplayCueAction->GetDirectoryPathsToAdd())
			{
				FString MutablePath = Directory.Path;
				UGameFeaturesSubsystem::FixPluginPackagePath(MutablePath, PluginRootPath, false);
				GameplayCueManager->AddGameplayCueNotifyPath(MutablePath, false);
			}

			GameplayCueManager->InitializeRuntimeObjectLibrary();
		}
	}
}

void URpgGameFeature_AddGameplayCuePaths::OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	const FString PluginRootPath = TEXT("/") + PluginName;

	for (const UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddGameplayCuePath* AddGameplayCueAction = Cast<URpgGameFeatureAction_AddGameplayCuePath>(Action);
		if (!AddGameplayCueAction)
		{
			continue;
		}

		if (UGameplayCueManager* GameplayCueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			for (const FDirectoryPath& Directory : AddGameplayCueAction->GetDirectoryPathsToAdd())
			{
				FString MutablePath = Directory.Path;
				UGameFeaturesSubsystem::FixPluginPackagePath(MutablePath, PluginRootPath, false);
				GameplayCueManager->RemoveGameplayCueNotifyPath(MutablePath, false);
			}

			GameplayCueManager->InitializeRuntimeObjectLibrary();
		}
	}
}
