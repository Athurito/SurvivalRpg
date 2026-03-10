// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameModeBase.h"

#include "AbilitySystemInterface.h"
#include "RpgWorldSettings.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Core/Character/BasePawnData.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgRespawnComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

void ARpgGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	if (const ARpgWorldSettings* WorldSettings = Cast<ARpgWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		if (const UBasePawnData* PawnData = WorldSettings->GetDefaultPawnData())
		{
			if (ARpgPlayerState* PlayerState = NewPlayer->GetPlayerState<ARpgPlayerState>())
			{
				PlayerState->SetPawnData(PawnData);
			}
		}
	}

	// Ensure save data entry exists for this player.
	GetOrCreatePlayerSaveData(NewPlayer);

	Super::PostLogin(NewPlayer);
}

void ARpgGameModeBase::Logout(AController* Exiting)
{
	// Save data stays in the map — player can rejoin and keep their progress.
	// Only cleaned up on explicit host action or session end.
	Super::Logout(Exiting);
}

// ---------------------------------------------------------------------------
// Pawn Spawning
// ---------------------------------------------------------------------------

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
				PawnExt->SetPawnData(GetPawnDataForController(NewPlayer));
			}

			SpawnedPawn->FinishSpawning(SpawnTransform);
			return SpawnedPawn;
		}
	}
	return nullptr;
}

const UBasePawnData* ARpgGameModeBase::GetPawnDataForController(const AController* InController) const
{
	if (!InController) return nullptr;
	if (ARpgPlayerState* Ps = InController->GetPlayerState<ARpgPlayerState>())
	{
		if (const UBasePawnData* PawnData = Ps->GetPawnData<UBasePawnData>())
		{
			return PawnData;
		}
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
		if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			return PS->GetUniqueId();
		}
	}
	return FUniqueNetIdRepl();
}

FRpgPlayerSaveData& ARpgGameModeBase::GetOrCreatePlayerSaveData(APlayerController* PC)
{
	FUniqueNetIdRepl NetId = GetNetIdForPC(PC);

	if (FRpgPlayerSaveData* Existing = PlayerSaveDataMap.Find(NetId))
	{
		return *Existing;
	}

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Creating save data for player [%s]."),
		*GetNameSafe(PC));

	return PlayerSaveDataMap.Add(NetId, FRpgPlayerSaveData());
}

const FRpgPlayerSaveData* ARpgGameModeBase::FindPlayerSaveData(APlayerController* PC) const
{
	FUniqueNetIdRepl NetId = GetNetIdForPC(PC);
	return PlayerSaveDataMap.Find(NetId);
}

// ---------------------------------------------------------------------------
// Checkpoint
// ---------------------------------------------------------------------------

void ARpgGameModeBase::SetPlayerCheckpoint(APlayerController* PC, const FTransform& CheckpointTransform)
{
	if (!PC) return;

	FRpgPlayerSaveData& Data = GetOrCreatePlayerSaveData(PC);
	Data.CheckpointTransform = CheckpointTransform;
	Data.bHasCheckpoint = true;

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Checkpoint set for [%s] at %s."),
		*GetNameSafe(PC), *CheckpointTransform.GetLocation().ToString());
}

FTransform ARpgGameModeBase::GetPlayerCheckpointTransform(APlayerController* PC) const
{
	if (const FRpgPlayerSaveData* Data = FindPlayerSaveData(PC))
	{
		if (Data->bHasCheckpoint)
		{
			return Data->CheckpointTransform;
		}
	}

	// Fallback: default player start.
	AActor* PlayerStart = const_cast<ARpgGameModeBase*>(this)->FindPlayerStart(PC);
	if (PlayerStart)
	{
		return PlayerStart->GetActorTransform();
	}

	return FTransform::Identity;
}


// ---------------------------------------------------------------------------
// Respawn (server-authoritative)
// ---------------------------------------------------------------------------

void ARpgGameModeBase::RequestPlayerRespawn(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgGameMode: RequestPlayerRespawn — no pawn for [%s]."), *GetNameSafe(PC));
		return;
	}

	const FTransform SpawnPoint = GetPlayerCheckpointTransform(PC);
	ExecuteRespawn(PC, SpawnPoint);
}

void ARpgGameModeBase::ExecuteRespawn(APlayerController* PC, const FTransform& SpawnPoint)
{
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// Teleport to checkpoint.
	Pawn->SetActorTransform(SpawnPoint, false, nullptr, ETeleportType::ResetPhysics);

	// Restore health and clear death/downed state.
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
		{
			// Clear death tags.
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Dead_WaitingForRespawn, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
			ASC->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);

			// Restore health to max.
			if (const URpgHealthSet* HealthSet = ASC->GetSet<URpgHealthSet>())
			{
				ASC->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());
			}
		}
	}

	// Reset HealthComponent death state.
	if (URpgHealthComponent* HealthComp = URpgHealthComponent::FindHealthComponent(Pawn))
	{
		// Reset death state so the character is considered alive again.
		// HealthComponent tracks DeathState internally — we need to reset it.
		// Since DeathState is protected, we re-initialize the component.
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
		{
			if (URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
			{
				HealthComp->UninitializeFromAbilitySystem();
				HealthComp->InitializeWithAbilitySystem(ASC);
			}
		}
	}

	// Reset downed component.
	if (URpgDownedComponent* DownedComp = URpgDownedComponent::FindDownedComponent(Pawn))
	{
		if (DownedComp->IsDowned())
		{
			DownedComp->ExitDowned();
		}
	}

	// Re-enable collision and movement.
	Pawn->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* MoveComp = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	// Notify RespawnComponent (for client-side UI cleanup).
	if (URpgRespawnComponent* RespawnComp = URpgRespawnComponent::FindRespawnComponent(Pawn))
	{
		RespawnComp->OnServerRespawnExecuted(SpawnPoint);
	}

	// Send gameplay event.
	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Pawn))
	{
		if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
		{
			FGameplayEventData Payload;
			Payload.EventTag = RpgGameplayTags::GameplayEvent_Respawn;
			Payload.Instigator = Pawn;
			Payload.Target = Pawn;
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}

	OnPlayerRespawned.Broadcast(PC, SpawnPoint);

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: [%s] respawned at %s."), *GetNameSafe(PC), *SpawnPoint.GetLocation().ToString());
}
