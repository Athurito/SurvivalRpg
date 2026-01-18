// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RpgUiSubsystem.generated.h"

class UPlayerVitalsViewmodel;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
	
	
public:
	UPlayerVitalsViewmodel* GetOrCreateVitalsVM();

private:
	UPROPERTY(Transient)
	TObjectPtr<UPlayerVitalsViewmodel> VitalsVM;
};
