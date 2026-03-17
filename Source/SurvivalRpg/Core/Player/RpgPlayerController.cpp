// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerController.h"

#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"

void ARpgPlayerController::RequestRespawn()
{
	if (!HasAuthority())
	{
		ServerRequestRespawn();
		return;
	}

	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->RequestPlayerRespawn(this);
	}
}

void ARpgPlayerController::ServerRequestRespawn_Implementation()
{
	RequestRespawn();
}
