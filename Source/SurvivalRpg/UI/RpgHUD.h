// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RpgHUD.generated.h"

class AActor;

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API ARpgHUD : public AHUD
{
	GENERATED_BODY()
public:
	explicit ARpgHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void PreInitializeComponents() override;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetDebugActorList(TArray<AActor*>& InOutList) override;
};
