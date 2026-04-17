// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerController.h"

#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

ARpgPlayerState* ARpgPlayerController::GetRpgPlayerState() const
{
	return CastChecked<ARpgPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

URpgAbilitySystemComponent* ARpgPlayerController::GetRpgAbilitySystemComponent() const
{
	const ARpgPlayerState* RpgPS = GetRpgPlayerState();
	return (RpgPS ? RpgPS->GetRpgAbilitySystemComponent() : nullptr);
}

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

void ARpgPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const UInputMappingContext* MappingContext : DefaultMappingContexts)
		{
			if (MappingContext && !InputSubsystem->HasMappingContext(MappingContext))
			{
				InputSubsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

void ARpgPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshPlayerStateBindings();

	if (GetWorld()->IsNetMode(NM_Client))
	{
		if (ARpgPlayerState* RpgPS = GetPlayerState<ARpgPlayerState>())
		{
			if (URpgAbilitySystemComponent* RpgASC = RpgPS->GetRpgAbilitySystemComponent())
			{
				RpgASC->RefreshAbilityActorInfo();
				RpgASC->TryActivateAbilitiesOnSpawn();
			}
		}
	}
}

void ARpgPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (URpgAbilitySystemComponent* RpgASC = GetRpgAbilitySystemComponent())
	{
		RpgASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	Super::PostProcessInput(DeltaTime, bGamePaused);
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
