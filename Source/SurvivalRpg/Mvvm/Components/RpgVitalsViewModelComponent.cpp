// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgVitalsViewModelComponent.h"

#include "SurvivalRpg/Mvvm/PlayerVitals/PlayerVitalsViewmodel.h"


void URpgVitalsViewModelComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URpgVitalsViewModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ViewModel)
	{
		ViewModel->UnbindASC();
		ViewModel = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void URpgVitalsViewModelComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
}

void URpgVitalsViewModelComponent::BindFromPawn(APawn* Pawn)
{
}
