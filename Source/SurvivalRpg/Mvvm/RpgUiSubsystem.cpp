// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgUiSubsystem.h"

#include "PlayerVitals/PlayerVitalsViewmodel.h"

UPlayerVitalsViewmodel* URpgUiSubsystem::GetOrCreateVitalsVM()
{
	if (!VitalsVM)
	{
		VitalsVM = NewObject<UPlayerVitalsViewmodel>(this); // Outer = LocalPlayerSubsystem (stabil)
	}
	return VitalsVM;
}
