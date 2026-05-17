// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgWorldSettings.h"

#include "Engine/AssetManager.h"
#include "SurvivalRpg/SurvivalRpg.h"

ARpgWorldSettings::ARpgWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FPrimaryAssetId ARpgWorldSettings::GetDefaultGameplayExperience() const
{
	FPrimaryAssetId Result;
	if (!DefaultGameplayExperience.IsNull())
	{
		Result = UAssetManager::Get().GetPrimaryAssetIdForPath(DefaultGameplayExperience.ToSoftObjectPath());

		if (!Result.IsValid())
		{
			UE_LOG(LogRpgExperience, Error, TEXT("%s.DefaultGameplayExperience is %s but failed to resolve into a primary asset id."),
				*GetPathNameSafe(this),
				*DefaultGameplayExperience.ToString());
		}
	}

	return Result;
}
