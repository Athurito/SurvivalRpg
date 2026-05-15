// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "RpgDeveloperSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class URpgDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	URpgDeveloperSettings();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Config, Category = "Experience")
	FPrimaryAssetId ExperienceOverride;
};
