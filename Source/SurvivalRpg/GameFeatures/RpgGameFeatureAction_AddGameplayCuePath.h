// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "UObject/SoftObjectPath.h"
#include "RpgGameFeatureAction_AddGameplayCuePath.generated.h"

/**
 * GameFeatureAction that registers additional GameplayCue notify paths.
 *
 * Use this when a feature plugin owns cue notifies that should be discoverable while the feature is active.
 */
UCLASS(meta = (DisplayName = "Add Rpg Gameplay Cue Path"))
class SURVIVALRPG_API URpgGameFeatureAction_AddGameplayCuePath final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	URpgGameFeatureAction_AddGameplayCuePath();

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	const TArray<FDirectoryPath>& GetDirectoryPathsToAdd() const { return DirectoryPathsToAdd; }

private:
	/** Paths to register with the GameplayCue manager. They are relative to the game content directory. */
	UPROPERTY(EditAnywhere, Category = "Game Feature | Gameplay Cues", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> DirectoryPathsToAdd;
};
