// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerController.h"

#include "EnhancedInputSubsystems.h"

class UEnhancedInputLocalPlayerSubsystem;

void ARpgPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	// Add Input Mapping Contexts
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}
}
