// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "RpgWorldSettings.generated.h"

class URpgPawnData;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API ARpgWorldSettings : public AWorldSettings
{
	GENERATED_BODY()
public:
	const URpgPawnData* GetDefaultPawnData() const { return DefaultPawnData; }
protected:
	
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TObjectPtr<URpgPawnData> DefaultPawnData;
};
