// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RpgGameModeBase.generated.h"

class UBasePawnData;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API ARpgGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	
	const UBasePawnData* GetPawnDataForController(const AController* InController) const;
	
};
