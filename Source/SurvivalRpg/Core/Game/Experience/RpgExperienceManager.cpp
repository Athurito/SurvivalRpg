// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgExperienceManager.h"

#include "Engine/Engine.h"

#if WITH_EDITOR
void URpgExperienceManager::OnPlayInEditorBegun()
{
	ensure(GameFeaturePluginRequestCountMap.IsEmpty());
	GameFeaturePluginRequestCountMap.Empty();
}

void URpgExperienceManager::NotifyOfPluginActivation(const FString& PluginURL)
{
	if (GIsEditor)
	{
		URpgExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<URpgExperienceManager>();
		check(ExperienceManagerSubsystem);

		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindOrAdd(PluginURL);
		++Count;
	}
}

bool URpgExperienceManager::RequestToDeactivatePlugin(const FString& PluginURL)
{
	if (GIsEditor)
	{
		URpgExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<URpgExperienceManager>();
		check(ExperienceManagerSubsystem);

		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindChecked(PluginURL);
		--Count;

		if (Count == 0)
		{
			ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.Remove(PluginURL);
			return true;
		}

		return false;
	}

	return true;
}
#endif
