// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerController.h"

#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

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

void ARpgPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	RefreshPlayerStateBindings();
}

void ARpgPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshPlayerStateBindings();
}

void ARpgPlayerController::ServerRequestRespawn_Implementation()
{
	RequestRespawn();
}

void ARpgPlayerController::HandleRespawnStateChanged(bool bIsWaitingForRespawn, float RespawnAvailableServerTime)
{
	K2_OnRespawnStateChanged(bIsWaitingForRespawn, RespawnAvailableServerTime);
}

void ARpgPlayerController::HandleCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform)
{
	K2_OnCheckpointChanged(bHasCheckpoint, CheckpointTransform);
}

void ARpgPlayerController::RefreshPlayerStateBindings()
{
	ARpgPlayerState* CurrentPlayerState = GetPlayerState<ARpgPlayerState>();
	if (BoundPlayerState == CurrentPlayerState)
	{
		return;
	}

	UnbindFromPlayerState();
	BindToPlayerState(CurrentPlayerState);
}

void ARpgPlayerController::BindToPlayerState(ARpgPlayerState* NewPlayerState)
{
	if (!NewPlayerState)
	{
		return;
	}

	BoundPlayerState = NewPlayerState;
	BoundPlayerState->OnRespawnStateChanged.AddDynamic(this, &ThisClass::HandleRespawnStateChanged);
	BoundPlayerState->OnCheckpointChanged.AddDynamic(this, &ThisClass::HandleCheckpointChanged);

	HandleRespawnStateChanged(
		BoundPlayerState->IsWaitingForRespawn(),
		BoundPlayerState->GetRespawnAvailableServerTime());

	HandleCheckpointChanged(
		BoundPlayerState->HasCheckpoint(),
		BoundPlayerState->GetCheckpointTransform());
}

void ARpgPlayerController::UnbindFromPlayerState()
{
	if (!BoundPlayerState)
	{
		return;
	}

	BoundPlayerState->OnRespawnStateChanged.RemoveDynamic(this, &ThisClass::HandleRespawnStateChanged);
	BoundPlayerState->OnCheckpointChanged.RemoveDynamic(this, &ThisClass::HandleCheckpointChanged);
	BoundPlayerState = nullptr;
}
