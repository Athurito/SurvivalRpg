// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgExperienceDefinition.generated.h"

class UGameFeatureAction;
class URpgExperienceActionSet;
class URpgPawnData;

/**
 * Definition of a gameplay experience.
 *
 * Experiences are the top-level data assets that select the default PawnData,
 * activate GameFeature plugins, and run feature actions for a map or play mode.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URpgExperienceDefinition();

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	//~ UPrimaryDataAsset interface
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
	//~ End UPrimaryDataAsset interface

public:
	/** GameFeature plugins that should be loaded and activated with this experience. */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<FString> GameFeaturesToEnable;

	/** Default pawn configuration used for player spawning unless a PlayerState already provides PawnData. */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TObjectPtr<const URpgPawnData> DefaultPawnData;

	/** Actions executed while this experience is activated, deactivated, or unloaded. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** Additional reusable action sets composed into this experience. */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<TObjectPtr<URpgExperienceActionSet>> ActionSets;
};
