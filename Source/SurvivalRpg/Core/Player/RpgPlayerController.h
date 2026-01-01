// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RpgPlayerController.generated.h"

class UInputMappingContext;
/**
 * 
 */
UCLASS(Abstract)
class SURVIVALRPG_API ARpgPlayerController : public APlayerController
{
	GENERATED_BODY()
	
	
protected:
	/** Input mapping context setup */
	virtual void SetupInputComponent() override;
protected:
	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category ="Input", meta = (AllowPrivateAccess = "true"))
	TArray<UInputMappingContext*> DefaultMappingContexts;
};
