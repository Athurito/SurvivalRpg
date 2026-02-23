// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedFriendsGameInstance.h"
#include "Engine/GameInstance.h"
#include "RpgGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgGameInstance : public UAdvancedFriendsGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
};
