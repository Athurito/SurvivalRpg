// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgExperienceActionSet.generated.h"

class UGameFeatureAction;

/**
 * A reusable group of GameFeature actions and feature dependencies.
 *
 * Action sets let multiple experiences share the same feature composition without
 * copying every action onto each individual experience asset.
 */
UCLASS(BlueprintType, NotBlueprintable)
class SURVIVALRPG_API URpgExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URpgExperienceActionSet();

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
	/** Actions to apply when an experience using this action set is active. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Actions to Perform")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** GameFeature plugins that must be active for these actions to work. */
	UPROPERTY(EditAnywhere, Category = "Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};
