// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "RpgWorldSettings.generated.h"

class URpgExperienceDefinition;

/**
 * World settings extension used to select the default gameplay experience for a map.
 */
UCLASS()
class SURVIVALRPG_API ARpgWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	explicit ARpgWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the default experience to use when the server opens this map. */
	FPrimaryAssetId GetDefaultGameplayExperience() const;

protected:
	/** Default experience to use unless URL, PIE settings, or command line select another one. */
	UPROPERTY(EditDefaultsOnly, Category = "GameMode")
	TSoftClassPtr<URpgExperienceDefinition> DefaultGameplayExperience;
};
