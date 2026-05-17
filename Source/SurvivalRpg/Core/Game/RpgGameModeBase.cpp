// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameModeBase.h"

#include "RpgWorldSettings.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/AI/RpgAIController.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/System/RpgAssetManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

ARpgGameModeBase::ARpgGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = ARpgGameStateBase::StaticClass();
	PlayerControllerClass = ARpgPlayerController::StaticClass();
	PlayerStateClass = ARpgPlayerState::StaticClass();
	DefaultPawnClass = ARpgCharacter::StaticClass();
}

void ARpgGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void ARpgGameModeBase::InitGameState()
{
	Super::InitGameState();

	URpgExperienceManagerComponent* ExperienceComponent = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

void ARpgGameModeBase::PostLogin(APlayerController* NewPlayer)
{
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

void ARpgGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
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
		if (const URpgAIPawnData* AIPawnData = AIController->GetDefaultPawnData())
		{
			return static_cast<const URpgPawnData*>(AIPawnData);
		}
	}

	if (GameState)
	{
		if (const URpgExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<URpgExperienceManagerComponent>())
		{
			if (ExperienceComponent->IsExperienceLoaded())
			{
				const URpgExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();
				if (Experience && Experience->DefaultPawnData)
				{
					return Experience->DefaultPawnData;
				}

				return URpgAssetManager::Get().GetDefaultPawnData();
			}
		}
	}

	return nullptr;
}

bool ARpgGameModeBase::IsExperienceLoaded() const
{
	check(GameState);

	const URpgExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<URpgExperienceManagerComponent>();
	check(ExperienceComponent);

	return ExperienceComponent->IsExperienceLoaded();
}

void ARpgGameModeBase::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType(URpgExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
		ExperienceIdSource = TEXT("OptionsString");
	}

	if (!ExperienceId.IsValid() && GetWorld()->IsPlayInEditor())
	{
		ExperienceId = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		ExperienceIdSource = TEXT("DeveloperSettings");
	}

	if (!ExperienceId.IsValid())
	{
		FString ExperienceFromCommandLine;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
		{
			ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
			if (!ExperienceId.PrimaryAssetType.IsValid())
			{
				ExperienceId = FPrimaryAssetId(FPrimaryAssetType(URpgExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
			}
			ExperienceIdSource = TEXT("CommandLine");
		}
	}

	if (!ExperienceId.IsValid())
	{
		if (const ARpgWorldSettings* WorldSettings = Cast<ARpgWorldSettings>(GetWorldSettings()))
		{
			ExperienceId = WorldSettings->GetDefaultGameplayExperience();
			ExperienceIdSource = TEXT("WorldSettings");
		}
	}

	URpgAssetManager& AssetManager = URpgAssetManager::Get();
	FAssetData Dummy;
	if (ExperienceId.IsValid() && !AssetManager.GetPrimaryAssetData(ExperienceId, Dummy))
	{
		UE_LOG(LogRpgExperience, Error, TEXT("Wanted to use experience [%s] from [%s], but AssetManager could not find it. Falling back."),
			*ExperienceId.ToString(),
			*ExperienceIdSource);
		ExperienceId = FPrimaryAssetId();
	}

	if (!ExperienceId.IsValid())
	{
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType(URpgExperienceDefinition::StaticClass()->GetFName()), FName(TEXT("RpgPrototypeExperience")));
		ExperienceIdSource = TEXT("Default");
	}

	OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}

void ARpgGameModeBase::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource)
{
	if (!ExperienceId.IsValid())
	{
		UE_LOG(LogRpgExperience, Error, TEXT("Failed to identify a valid experience."));
		return;
	}

	UE_LOG(LogRpgExperience, Log, TEXT("Identified experience [%s] from [%s]."),
		*ExperienceId.ToString(),
		*ExperienceIdSource);

	check(GameState);
	URpgExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<URpgExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->SetCurrentExperience(ExperienceId);
}

void ARpgGameModeBase::OnExperienceLoaded(const URpgExperienceDefinition* CurrentExperience)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (PC && PC->GetPawn() == nullptr && PlayerCanRestart(PC))
		{
			RestartPlayer(PC);
		}
	}
}

bool ARpgGameModeBase::ShouldSpawnAtStartSpot(AController* Player)
{
	return false;
}

void ARpgGameModeBase::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);
}

bool ARpgGameModeBase::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return ControllerCanRestart(Player);
}

bool ARpgGameModeBase::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
	return true;
}

void ARpgGameModeBase::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);
	OnGameModePlayerInitialized.Broadcast(this, NewPlayer);
}

void ARpgGameModeBase::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	if (GetDefaultPawnClassForController(NewPlayer))
	{
		if (APlayerController* NewPC = Cast<APlayerController>(NewPlayer))
		{
			if (PlayerCanRestart(NewPC))
			{
				RequestPlayerRestartNextFrame(NewPlayer, false);
			}
			else
			{
				UE_LOG(LogRpg, Verbose, TEXT("FailedToRestartPlayer(%s) and PlayerCanRestart returned false, so not retrying."), *GetPathNameSafe(NewPlayer));
			}
		}
		else
		{
			RequestPlayerRestartNextFrame(NewPlayer, false);
		}
	}
	else
	{
		UE_LOG(LogRpg, Verbose, TEXT("FailedToRestartPlayer(%s) but there is no pawn class, so giving up."), *GetPathNameSafe(NewPlayer));
	}
}

void ARpgGameModeBase::RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset)
{
	if (!Controller)
	{
		return;
	}

	if (bForceReset)
	{
		Controller->Reset();
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		GetWorldTimerManager().SetTimerForNextTick(PC, &APlayerController::ServerRestartPlayer_Implementation);
		return;
	}

	TWeakObjectPtr<AController> WeakController = Controller;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		if (WeakController.IsValid())
		{
			RestartPlayer(WeakController.Get());
		}
	}));
}

bool ARpgGameModeBase::ControllerCanRestart(AController* Controller)
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (!Super::PlayerCanRestart_Implementation(PC))
		{
			return false;
		}
	}
	else if ((Controller == nullptr) || Controller->IsPendingKillPending())
	{
		return false;
	}

	return true;
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
