// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameModeBase.h"

#include "RpgWorldSettings.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/AI/RpgAIController.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

void ARpgGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	if (const ARpgWorldSettings* WorldSettings = Cast<ARpgWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		if (const URpgPawnData* PawnData = WorldSettings->GetDefaultPawnData())
		{
			if (ARpgBasePlayerState* PlayerState = NewPlayer->GetPlayerState<ARpgBasePlayerState>())
			{
				PlayerState->SetPawnData(PawnData);
			}
		}
	}

	GetOrCreatePlayerSaveData(NewPlayer);
	GetOrCreatePlayerRespawnState(NewPlayer);
	SyncPlayerCheckpointDataToPlayerState(NewPlayer);
	SyncPlayerRespawnStateToPlayerState(NewPlayer);

	Super::PostLogin(NewPlayer);
}

void ARpgGameModeBase::Logout(AController* Exiting)
{
	// Save data stays in the map so a player can reconnect and keep host-owned progress.
	Super::Logout(Exiting);
}

// ---------------------------------------------------------------------------
// Pawn Spawning
// ---------------------------------------------------------------------------

UClass* ARpgGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const URpgPawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->PawnClass)
		{
			return PawnData->PawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* ARpgGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bDeferConstruction = true;

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams))
		{
			if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			{
				if (const URpgPawnData* PawnData = GetPawnDataForController(NewPlayer))
				{
					PawnExt->SetPawnData(PawnData);
				}
			}

			SpawnedPawn->FinishSpawning(SpawnTransform);
			return SpawnedPawn;
		}
	}

	return nullptr;
}

const URpgPawnData* ARpgGameModeBase::GetPawnDataForController(const AController* InController) const
{
	if (!InController)
	{
		return nullptr;
	}

	if (ARpgBasePlayerState* PlayerState = InController->GetPlayerState<ARpgBasePlayerState>())
	{
		if (const URpgPawnData* PawnData = PlayerState->GetPawnData<URpgPawnData>())
		{
			return PawnData;
		}
	}

	if (const ARpgAIController* AIController = Cast<ARpgAIController>(InController))
	{
		return static_cast<const URpgPawnData*>(AIController->GetDefaultPawnData());
	}

	return nullptr;
}

// ---------------------------------------------------------------------------
// Save Data (host-authoritative, keyed by Steam NetId)
// ---------------------------------------------------------------------------

FUniqueNetIdRepl ARpgGameModeBase::GetNetIdForPC(const APlayerController* PC)
{
	if (PC)
	{
		if (const APlayerState* PlayerState = PC->GetPlayerState<APlayerState>())
		{
			return PlayerState->GetUniqueId();
		}
	}

	return FUniqueNetIdRepl();
}

FRpgPlayerSaveData& ARpgGameModeBase::GetOrCreatePlayerSaveData(APlayerController* PC)
{
	const FUniqueNetIdRepl NetId = GetNetIdForPC(PC);

	if (FRpgPlayerSaveData* Existing = PlayerSaveDataMap.Find(NetId))
	{
		return *Existing;
	}

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Creating save data for player [%s]."), *GetNameSafe(PC));
	return PlayerSaveDataMap.Add(NetId, FRpgPlayerSaveData());
}

const FRpgPlayerSaveData* ARpgGameModeBase::FindPlayerSaveData(APlayerController* PC) const
{
	const FUniqueNetIdRepl NetId = GetNetIdForPC(PC);
	return PlayerSaveDataMap.Find(NetId);
}

FRpgPlayerRespawnState& ARpgGameModeBase::GetOrCreatePlayerRespawnState(APlayerController* PC)
{
	const FUniqueNetIdRepl NetId = GetNetIdForPC(PC);

	if (FRpgPlayerRespawnState* Existing = PlayerRespawnStateMap.Find(NetId))
	{
		return *Existing;
	}

	return PlayerRespawnStateMap.Add(NetId, FRpgPlayerRespawnState());
}

const FRpgPlayerRespawnState* ARpgGameModeBase::FindPlayerRespawnState(APlayerController* PC) const
{
	const FUniqueNetIdRepl NetId = GetNetIdForPC(PC);
	return PlayerRespawnStateMap.Find(NetId);
}

// ---------------------------------------------------------------------------
// Checkpoint
// ---------------------------------------------------------------------------

void ARpgGameModeBase::SetPlayerCheckpoint(APlayerController* PC, const FTransform& CheckpointTransform)
{
	if (!PC)
	{
		return;
	}

	FRpgPlayerSaveData& SaveData = GetOrCreatePlayerSaveData(PC);
	SaveData.CheckpointTransform = CheckpointTransform;
	SaveData.bHasCheckpoint = true;

	SyncPlayerCheckpointDataToPlayerState(PC);

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Checkpoint set for [%s] at %s."),
		*GetNameSafe(PC),
		*CheckpointTransform.GetLocation().ToString());
}

FTransform ARpgGameModeBase::GetPlayerCheckpointTransform(APlayerController* PC) const
{
	if (const FRpgPlayerSaveData* SaveData = FindPlayerSaveData(PC))
	{
		if (SaveData->bHasCheckpoint)
		{
			return SaveData->CheckpointTransform;
		}
	}

	if (AActor* PlayerStart = const_cast<ARpgGameModeBase*>(this)->FindPlayerStart(PC))
	{
		return PlayerStart->GetActorTransform();
	}

	return FTransform::Identity;
}

// ---------------------------------------------------------------------------
// Respawn (server-authoritative)
// ---------------------------------------------------------------------------

void ARpgGameModeBase::NotifyPlayerDeath(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	FRpgPlayerRespawnState& RespawnState = GetOrCreatePlayerRespawnState(PC);
	if (RespawnState.bIsWaitingForRespawn)
	{
		return;
	}

	const AGameStateBase* WorldGameState = GetGameState<AGameStateBase>();
	const float ServerWorldTime = WorldGameState ? WorldGameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	RespawnState.bIsWaitingForRespawn = true;
	RespawnState.RespawnAvailableServerTime = ServerWorldTime + RespawnDelay;
	RespawnState.PendingRespawnTransform = GetPlayerCheckpointTransform(PC);

	if (APawn* ExistingPawn = PC->GetPawn())
	{
		PC->UnPossess();
		ExistingPawn->Destroy();
	}

	if (ARpgPlayerState* PlayerState = PC->GetPlayerState<ARpgPlayerState>())
	{
		PlayerState->SetRespawnState(true, RespawnState.RespawnAvailableServerTime);

		if (URpgAbilitySystemComponent* ASC = PlayerState->GetRpgAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Dead, 1);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death, 1);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 1);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Dead_WaitingForRespawn, 1);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
		}
	}

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: [%s] entered respawn wait until %.2f."),
		*GetNameSafe(PC),
		RespawnState.RespawnAvailableServerTime);
}

bool ARpgGameModeBase::CanRespawnPlayer(APlayerController* PC) const
{
	if (!PC || !HasAuthority())
	{
		return false;
	}

	const FRpgPlayerRespawnState* RespawnState = FindPlayerRespawnState(PC);
	if (!RespawnState || !RespawnState->bIsWaitingForRespawn)
	{
		return false;
	}

	const AGameStateBase* WorldGameState = GetGameState<AGameStateBase>();
	const float ServerWorldTime = WorldGameState ? WorldGameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return (ServerWorldTime >= RespawnState->RespawnAvailableServerTime);
}

void ARpgGameModeBase::RequestPlayerRespawn(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	const FRpgPlayerRespawnState* RespawnState = FindPlayerRespawnState(PC);
	if (!RespawnState || !RespawnState->bIsWaitingForRespawn)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameMode: RequestPlayerRespawn ignored for [%s] because no respawn is pending."), *GetNameSafe(PC));
		return;
	}

	if (!CanRespawnPlayer(PC))
	{
		UE_LOG(LogRpg, Verbose, TEXT("RpgGameMode: RequestPlayerRespawn ignored for [%s] because the delay has not elapsed yet."), *GetNameSafe(PC));
		return;
	}

	const FTransform SpawnPoint = RespawnState->PendingRespawnTransform.Equals(FTransform::Identity)
		? GetPlayerCheckpointTransform(PC)
		: RespawnState->PendingRespawnTransform;

	ExecuteRespawn(PC, SpawnPoint);
}

void ARpgGameModeBase::ExecuteRespawn(APlayerController* PC, const FTransform& SpawnPoint)
{
	if (!PC)
	{
		return;
	}

	ARpgPlayerState* PlayerState = PC->GetPlayerState<ARpgPlayerState>();
	URpgAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetRpgAbilitySystemComponent() : nullptr;

	if (ASC)
	{
		ASC->ResetForRespawn();
	}

	RestartPlayerAtTransform(PC, SpawnPoint);

	if (!PC->GetPawn())
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: RestartPlayerAtTransform failed for [%s]."), *GetNameSafe(PC));
		return;
	}

	if (ASC)
	{
		FGameplayEventData Payload;
		Payload.EventTag = RpgGameplayTags::GameplayEvent_Respawn;
		Payload.Instigator = PC->GetPawn();
		Payload.Target = PC->GetPawn();
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	PC->SetIgnoreMoveInput(false);
	ResetPlayerRespawnState(PC);

	OnPlayerRespawned.Broadcast(PC, SpawnPoint);

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: [%s] respawned at %s."),
		*GetNameSafe(PC),
		*SpawnPoint.GetLocation().ToString());
}

void ARpgGameModeBase::SyncPlayerCheckpointDataToPlayerState(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	if (ARpgPlayerState* PlayerState = PC->GetPlayerState<ARpgPlayerState>())
	{
		const FRpgPlayerSaveData& SaveData = GetOrCreatePlayerSaveData(PC);
		PlayerState->SetCheckpointData(SaveData.bHasCheckpoint, SaveData.CheckpointTransform);
	}
}

void ARpgGameModeBase::SyncPlayerRespawnStateToPlayerState(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	if (ARpgPlayerState* PlayerState = PC->GetPlayerState<ARpgPlayerState>())
	{
		const FRpgPlayerRespawnState& RespawnState = GetOrCreatePlayerRespawnState(PC);
		PlayerState->SetRespawnState(RespawnState.bIsWaitingForRespawn, RespawnState.RespawnAvailableServerTime);
	}
}

void ARpgGameModeBase::ResetPlayerRespawnState(APlayerController* PC)
{
	FRpgPlayerRespawnState& RespawnState = GetOrCreatePlayerRespawnState(PC);
	RespawnState = FRpgPlayerRespawnState();
	SyncPlayerRespawnStateToPlayerState(PC);
}

void ARpgGameModeBase::ClearRespawnGameplayState(URpgAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	ASC->SetLooseGameplayTagCount(RpgGameplayTags::State_Dead, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Dead_WaitingForRespawn, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
	ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
}
