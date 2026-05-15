// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "RpgExperienceManager.generated.h"

/**
 * Small Lyra-style helper that prevents concurrent PIE worlds from unloading
 * GameFeature plugins still requested by another experience.
 */
UCLASS()
class SURVIVALRPG_API URpgExperienceManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	void OnPlayInEditorBegun();
	static void NotifyOfPluginActivation(const FString& PluginURL);
	static bool RequestToDeactivatePlugin(const FString& PluginURL);
#else
	static void NotifyOfPluginActivation(const FString& PluginURL) {}
	static bool RequestToDeactivatePlugin(const FString& PluginURL) { return true; }
#endif

private:
	TMap<FString, int32> GameFeaturePluginRequestCountMap;
};
