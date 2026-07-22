// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameModeBase.h"

#include "RpgWorldSettings.h"
#include "AssetRegistry/AssetData.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
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
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
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
	DeathDropActorClass = ARpgDroppedInventoryActor::StaticClass();
}

void ARpgGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ResolvedOfflineProfileKey = UGameplayStatics::ParseOption(Options, TEXT("ProfileKey"));
	if (ResolvedOfflineProfileKey.IsEmpty())
	{
		ResolvedOfflineProfileKey = OfflineProfileKey.IsEmpty() ? TEXT("LocalProfile") : OfflineProfileKey;
	}

	LoadWorldSaveFromDisk();
	RegisterSaveStateListeners();

	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void ARpgGameModeBase::InitGameState()
{
	Super::InitGameState();

	URpgExperienceManagerComponent* ExperienceComponent = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
	check(ExperienceComponent);
	ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void ARpgGameModeBase::StartPlay()
{
	Super::StartPlay();
	RestorePlacedWorldContainers();
}

void ARpgGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bEnableDiskPersistence && HasAuthority())
	{
		FlushWorldSave();
	}

	GetWorldTimerManager().ClearTimer(AutoSaveTimerHandle);
	UnregisterSaveStateListeners();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

void ARpgGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	GetOrCreatePlayerSaveData(NewPlayer);
	GetOrCreatePlayerRespawnState(NewPlayer);
	RestorePlayerProfile(NewPlayer);
	SyncPlayerCheckpointDataToPlayerState(NewPlayer);
	SyncPlayerRespawnStateToPlayerState(NewPlayer);

	Super::PostLogin(NewPlayer);
}

void ARpgGameModeBase::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		CapturePlayerSaveData(PC);
		FlushWorldSave();
		PlayerProfileRestoreStates.Remove(TWeakObjectPtr<APlayerController>(PC));
	}

	// Profile data stays persistent, while a reconnect receives a fresh controller-scoped restore attempt.
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

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		ApplyRestoredEquipmentSelection(PC);
	}
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
// Save Data (host-authoritative, keyed by online id string / stable offline profile)
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
	const FString ProfileKey = GetPlayerProfileKey(PC);

	if (FRpgPlayerSaveData* Existing = PlayerSaveDataMap.Find(ProfileKey))
	{
		return *Existing;
	}

	UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Creating save data for player [%s] with profile key [%s]."),
		*GetNameSafe(PC), *ProfileKey);
	return PlayerSaveDataMap.Add(ProfileKey, FRpgPlayerSaveData());
}

const FRpgPlayerSaveData* ARpgGameModeBase::FindPlayerSaveData(APlayerController* PC) const
{
	return PlayerSaveDataMap.Find(GetPlayerProfileKey(PC));
}

FString ARpgGameModeBase::GetPlayerProfileKey(const APlayerController* PC) const
{
	const FUniqueNetIdRepl NetId = GetNetIdForPC(PC);
	if (NetId.IsValid())
	{
		const FString OnlineId = NetId.ToString();
		if (!OnlineId.IsEmpty() && !OnlineId.Equals(TEXT("INVALID"), ESearchCase::IgnoreCase))
		{
			return FString::Printf(TEXT("Online:%s"), *OnlineId);
		}
	}

	const FString& StableOfflineKey = ResolvedOfflineProfileKey.IsEmpty() ? OfflineProfileKey : ResolvedOfflineProfileKey;
	return FString::Printf(TEXT("Offline:%s"), StableOfflineKey.IsEmpty() ? TEXT("LocalProfile") : *StableOfflineKey);
}

bool ARpgGameModeBase::IsPlayerProfileRestoreComplete(const APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	const TWeakObjectPtr<APlayerController> ControllerKey(const_cast<APlayerController*>(PC));
	return PlayerProfileRestoreStates.Contains(ControllerKey);
}

bool ARpgGameModeBase::HasRestoredPlayerProfile(const APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}

	const TWeakObjectPtr<APlayerController> ControllerKey(const_cast<APlayerController*>(PC));
	const bool* bRestored = PlayerProfileRestoreStates.Find(ControllerKey);
	return bRestored && *bRestored;
}

void ARpgGameModeBase::MarkPlayerSaveDirty(APlayerController* PC)
{
	if (!HasAuthority() || !PC)
	{
		return;
	}

	MarkWorldSaveDirty();
}

void ARpgGameModeBase::MarkWorldContainerSaveDirty(
	FName PersistentContainerId,
	URpgInventoryManagerComponent* Inventory)
{
	if (!HasAuthority() || PersistentContainerId.IsNone() || !Inventory)
	{
		return;
	}

	FRpgWorldContainerSaveData& SaveData = WorldContainerSaveDataMap.FindOrAdd(PersistentContainerId);
	const FRpgInventoryGraphSaveData ExportedGraph = Inventory->ExportInventoryGraph();
	if (ExportedGraph.Items.Num() != Inventory->GetAllEntries().Num())
	{
		bDiskWritesBlockedByRestoreFailure = true;
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: World-container graph export failed for [%s]; disk writes are blocked."),
			*PersistentContainerId.ToString());
		return;
	}
	SaveData.PersistentContainerId = PersistentContainerId;
	SaveData.InventoryGraph = ExportedGraph;
	MarkWorldSaveDirty();
}

bool ARpgGameModeBase::RestorePlayerProfile(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return false;
	}

	const FString ProfileKey = GetPlayerProfileKey(PC);
	const TWeakObjectPtr<APlayerController> ControllerKey(PC);
	if (const bool* ExistingRestoreResult = PlayerProfileRestoreStates.Find(ControllerKey))
	{
		return *ExistingRestoreResult;
	}

	PlayerProfileRestoreStates.Add(ControllerKey, false);
	const FRpgPlayerSaveData* CurrentSaveData = PlayerSaveDataMap.Find(ProfileKey);
	if (!CurrentSaveData || !CurrentSaveData->bHasInventoryGraph)
	{
		return false;
	}

	TArray<const FRpgPlayerSaveData*> Candidates;
	Candidates.Add(CurrentSaveData);
	for (const URpgWorldSaveGame* CandidateSave : ValidLoadedSaveCandidates)
	{
		const FRpgPlayerSaveData* CandidatePlayer = CandidateSave ? CandidateSave->Players.Find(ProfileKey) : nullptr;
		if (CandidatePlayer && CandidatePlayer->bHasInventoryGraph && CandidatePlayer != CurrentSaveData)
		{
			Candidates.Add(CandidatePlayer);
		}
	}

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const FRpgPlayerSaveData* Candidate = Candidates[CandidateIndex];
		bIsRestoringSaveState = true;
		const bool bRestored = TryRestorePlayerSaveData(PC, *Candidate);
		bIsRestoringSaveState = false;
		if (!bRestored)
		{
			continue;
		}

		PlayerSaveDataMap.Add(ProfileKey, *Candidate);
		PlayerProfileRestoreStates.FindChecked(ControllerKey) = true;
		if (CandidateIndex > 0)
		{
			UE_LOG(LogRpg, Warning, TEXT("RpgGameMode: Restored profile [%s] from a fallback disk snapshot."), *ProfileKey);
			MarkWorldSaveDirty();
		}
		return true;
	}

	bDiskWritesBlockedByRestoreFailure = true;
	UE_LOG(LogRpg, Error,
		TEXT("RpgGameMode: Every disk candidate failed atomic graph import for profile [%s]. Disk writes are blocked to preserve the last valid save."),
		*ProfileKey);
	return false;
}

bool ARpgGameModeBase::TryRestorePlayerSaveData(APlayerController* PC, const FRpgPlayerSaveData& SaveData)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(PC);
	ARpgPlayerState* PlayerState = RpgPC ? RpgPC->GetRpgPlayerState() : nullptr;
	URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (!RpgPC || !Inventory || !SaveData.IsSchemaSupported() || !SaveData.bHasInventoryGraph)
	{
		return false;
	}

	FRpgInventoryMutationResult ImportResult;
	if (!Inventory->RestoreInventoryGraph(SaveData.InventoryGraph, ImportResult))
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Player inventory restore rejected with result code %d."),
			static_cast<int32>(ImportResult.Code));
		return false;
	}

	// Ordering is intentional: bindings and equipment ids may only resolve after item instances have been rebuilt.
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = RpgPC->GetEquipmentLoadoutComponent())
	{
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
	}
	if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
	{
		ActionBar->RestoreQuickAccessBindings(SaveData.QuickAccessBindings);
	}
	if (URpgEquipmentLoadoutComponent* EquipmentLoadout = RpgPC->GetEquipmentLoadoutComponent())
	{
		EquipmentLoadout->RestoreEquipmentSelection(SaveData.EquipmentSelection);
	}

	return true;
}

void ARpgGameModeBase::CapturePlayerSaveData(APlayerController* PC)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(PC);
	ARpgPlayerState* PlayerState = RpgPC ? RpgPC->GetRpgPlayerState() : nullptr;
	URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (!RpgPC || !Inventory)
	{
		return;
	}

	FRpgPlayerSaveData& SaveData = GetOrCreatePlayerSaveData(PC);
	const FRpgInventoryGraphSaveData ExportedGraph = Inventory->ExportInventoryGraph();
	if (ExportedGraph.Items.Num() != Inventory->GetAllEntries().Num())
	{
		bDiskWritesBlockedByRestoreFailure = true;
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Player graph export failed for [%s]; disk writes are blocked."),
			*GetPlayerProfileKey(PC));
		return;
	}
	SaveData.SchemaVersion = FRpgPlayerSaveData::CurrentSchemaVersion;
	SaveData.InventoryGraph = ExportedGraph;
	SaveData.bHasInventoryGraph = true;

	if (const URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
	{
		SaveData.QuickAccessBindings = ActionBar->GetQuickAccessBindings();
	}
	if (const URpgEquipmentLoadoutComponent* EquipmentLoadout = RpgPC->GetEquipmentLoadoutComponent())
	{
		SaveData.EquipmentSelection = EquipmentLoadout->ExportEquipmentSelection();
	}
}

void ARpgGameModeBase::CaptureConnectedPlayers()
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		CapturePlayerSaveData(Iterator->Get());
	}
}

void ARpgGameModeBase::CaptureWorldContainers()
{
	for (TActorIterator<AActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		AActor* Actor = *Iterator;
		URpgInventoryContainerComponent* Container = Actor ? Actor->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
		const FName PersistentId = Container ? Container->GetPersistentContainerId() : NAME_None;
		URpgInventoryManagerComponent* Inventory = Container ? Container->GetInventoryManager() : nullptr;
		if (PersistentId.IsNone() || !Inventory)
		{
			continue;
		}

		FRpgWorldContainerSaveData& SaveData = WorldContainerSaveDataMap.FindOrAdd(PersistentId);
		const FRpgInventoryGraphSaveData ExportedGraph = Inventory->ExportInventoryGraph();
		if (ExportedGraph.Items.Num() != Inventory->GetAllEntries().Num())
		{
			bDiskWritesBlockedByRestoreFailure = true;
			UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Graph export failed for world container [%s]; disk writes are blocked."),
				*PersistentId.ToString());
			return;
		}
		SaveData.PersistentContainerId = PersistentId;
		SaveData.InventoryGraph = ExportedGraph;
	}
}

void ARpgGameModeBase::RestorePlacedWorldContainers()
{
	if (!HasAuthority())
	{
		return;
	}

	for (TActorIterator<AActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		AActor* Actor = *Iterator;
		URpgInventoryContainerComponent* Container = Actor ? Actor->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
		if (Container && !Container->GetPersistentContainerId().IsNone())
		{
			RestoreWorldContainer(Container->GetPersistentContainerId(), Container->GetInventoryManager());
		}
	}
}

bool ARpgGameModeBase::RestoreWorldContainer(
	FName PersistentContainerId,
	URpgInventoryManagerComponent* Inventory)
{
	if (!HasAuthority() || PersistentContainerId.IsNone() || !Inventory)
	{
		return false;
	}

	TArray<const FRpgWorldContainerSaveData*> Candidates;
	if (const FRpgWorldContainerSaveData* Current = WorldContainerSaveDataMap.Find(PersistentContainerId))
	{
		Candidates.Add(Current);
	}
	for (const URpgWorldSaveGame* CandidateSave : ValidLoadedSaveCandidates)
	{
		const FRpgWorldContainerSaveData* Candidate = CandidateSave ? CandidateSave->WorldContainers.Find(PersistentContainerId) : nullptr;
		if (Candidate && !Candidates.Contains(Candidate))
		{
			Candidates.Add(Candidate);
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		FRpgInventoryMutationResult ImportResult;
		bIsRestoringSaveState = true;
		const bool bRestored = Inventory->RestoreInventoryGraph(Candidates[CandidateIndex]->InventoryGraph, ImportResult);
		bIsRestoringSaveState = false;
		if (bRestored)
		{
			WorldContainerSaveDataMap.Add(PersistentContainerId, *Candidates[CandidateIndex]);
			if (CandidateIndex > 0)
			{
				MarkWorldSaveDirty();
			}
			return true;
		}
	}

	bDiskWritesBlockedByRestoreFailure = true;
	UE_LOG(LogRpg, Error,
		TEXT("RpgGameMode: Every disk candidate failed atomic import for world container [%s]. Disk writes are blocked."),
		*PersistentContainerId.ToString());
	return false;
}

void ARpgGameModeBase::ApplyRestoredEquipmentSelection(APlayerController* PC)
{
	if (!PC || !HasRestoredPlayerProfile(PC))
	{
		return;
	}

	const FRpgPlayerSaveData* SaveData = FindPlayerSaveData(PC);
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(PC);
	if (SaveData && RpgPC)
	{
		if (URpgEquipmentLoadoutComponent* EquipmentLoadout = RpgPC->GetEquipmentLoadoutComponent())
		{
			EquipmentLoadout->RestoreEquipmentSelection(SaveData->EquipmentSelection);
		}
	}
}

void ARpgGameModeBase::LoadWorldSaveFromDisk()
{
	PlayerSaveDataMap.Reset();
	WorldContainerSaveDataMap.Reset();
	ValidLoadedSaveCandidates.Reset();
	LastSuccessfulSaveGame = nullptr;
	NextSaveSequence = 1;

	if (!bEnableDiskPersistence)
	{
		return;
	}

	TSet<FString> VisitedSlots;
	const TArray<FString> CandidateSlots = { WorldSaveSlotName, WorldSaveBackupSlotName, WorldSaveRecoverySlotName };
	for (const FString& SlotName : CandidateSlots)
	{
		if (SlotName.IsEmpty() || VisitedSlots.Contains(SlotName) ||
			!UGameplayStatics::DoesSaveGameExist(SlotName, WorldSaveUserIndex))
		{
			continue;
		}
		VisitedSlots.Add(SlotName);

		URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, WorldSaveUserIndex));
		FString ValidationError;
		if (!Loaded || !Loaded->ValidateForLoad(ValidationError))
		{
			UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Ignoring invalid save slot [%s]: %s"),
				*SlotName, Loaded ? *ValidationError : TEXT("wrong SaveGame class or load failure"));
			continue;
		}

		ValidLoadedSaveCandidates.Add(Loaded);
	}

	ValidLoadedSaveCandidates.Sort([](const URpgWorldSaveGame& A, const URpgWorldSaveGame& B)
	{
		return A.SaveSequence > B.SaveSequence;
	});

	if (!ValidLoadedSaveCandidates.IsEmpty())
	{
		LastSuccessfulSaveGame = ValidLoadedSaveCandidates[0];
		PlayerSaveDataMap = LastSuccessfulSaveGame->Players;
		WorldContainerSaveDataMap = LastSuccessfulSaveGame->WorldContainers;
		NextSaveSequence = LastSuccessfulSaveGame->SaveSequence + 1;
		UE_LOG(LogRpg, Log, TEXT("RpgGameMode: Loaded world save sequence %lld with %d player profiles and %d world containers."),
			LastSuccessfulSaveGame->SaveSequence, PlayerSaveDataMap.Num(), WorldContainerSaveDataMap.Num());
	}
}

URpgWorldSaveGame* ARpgGameModeBase::BuildWorldSaveSnapshot()
{
	URpgWorldSaveGame* Snapshot = NewObject<URpgWorldSaveGame>(this);
	Snapshot->SchemaVersion = URpgWorldSaveGame::CurrentSchemaVersion;
	Snapshot->SaveSequence = NextSaveSequence++;
	Snapshot->Players = PlayerSaveDataMap;
	Snapshot->WorldContainers = WorldContainerSaveDataMap;
	return Snapshot;
}

void ARpgGameModeBase::MarkWorldSaveDirty()
{
	if (!bEnableDiskPersistence || bIsRestoringSaveState || bDiskWritesBlockedByRestoreFailure)
	{
		return;
	}

	bWorldSaveDirty = true;
	ScheduleAsyncWorldSave();
}

void ARpgGameModeBase::ScheduleAsyncWorldSave()
{
	if (bAsyncSaveInFlight)
	{
		bSaveQueuedDuringAsync = true;
		return;
	}

	GetWorldTimerManager().ClearTimer(AutoSaveTimerHandle);
	if (AutoSaveDebounceSeconds <= 0.0f)
	{
		AutoSaveTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::SaveWorldStateAsync);
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			AutoSaveTimerHandle,
			this,
			&ThisClass::SaveWorldStateAsync,
			AutoSaveDebounceSeconds,
			false);
	}
}

bool ARpgGameModeBase::WritePreviousSnapshotToBackup() const
{
	if (!LastSuccessfulSaveGame)
	{
		return true;
	}
	if (WorldSaveBackupSlotName.IsEmpty() || WorldSaveBackupSlotName == WorldSaveSlotName)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Backup slot must be non-empty and different from the primary slot."));
		return false;
	}

	return UGameplayStatics::SaveGameToSlot(LastSuccessfulSaveGame, WorldSaveBackupSlotName, WorldSaveUserIndex);
}

void ARpgGameModeBase::SaveWorldStateAsync()
{
	if (!bWorldSaveDirty || !bEnableDiskPersistence || bDiskWritesBlockedByRestoreFailure)
	{
		return;
	}
	if (bAsyncSaveInFlight)
	{
		bSaveQueuedDuringAsync = true;
		return;
	}

	CaptureConnectedPlayers();
	CaptureWorldContainers();
	if (bDiskWritesBlockedByRestoreFailure)
	{
		return;
	}
	if (!WritePreviousSnapshotToBackup())
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Async save aborted because the previous valid snapshot could not be backed up."));
		ScheduleAsyncWorldSave();
		return;
	}

	ActiveAsyncSaveGame = BuildWorldSaveSnapshot();
	bAsyncSaveInFlight = true;
	bSaveQueuedDuringAsync = false;
	bWorldSaveDirty = false;
	UGameplayStatics::AsyncSaveGameToSlot(
		ActiveAsyncSaveGame,
		WorldSaveSlotName,
		WorldSaveUserIndex,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &ThisClass::HandleAsyncSaveCompleted));
}

void ARpgGameModeBase::HandleAsyncSaveCompleted(const FString& SlotName, int32 UserIndex, bool bSuccess)
{
	URpgWorldSaveGame* CompletedSnapshot = ActiveAsyncSaveGame;
	bAsyncSaveInFlight = false;
	ActiveAsyncSaveGame = nullptr;

	if (bSuccess && CompletedSnapshot)
	{
		if (!LastSuccessfulSaveGame || CompletedSnapshot->SaveSequence >= LastSuccessfulSaveGame->SaveSequence)
		{
			LastSuccessfulSaveGame = CompletedSnapshot;
		}
		else
		{
			// A checkpoint/logout flush completed while this older task was running. Reassert the newer primary snapshot.
			UGameplayStatics::SaveGameToSlot(LastSuccessfulSaveGame, WorldSaveSlotName, WorldSaveUserIndex);
		}
	}
	else
	{
		bWorldSaveDirty = true;
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Async SaveGame write failed for slot [%s], user %d."), *SlotName, UserIndex);
	}

	if (bSaveQueuedDuringAsync || bWorldSaveDirty)
	{
		bSaveQueuedDuringAsync = false;
		ScheduleAsyncWorldSave();
	}
}

bool ARpgGameModeBase::SaveWorldStateSync()
{
	if (!bEnableDiskPersistence || bDiskWritesBlockedByRestoreFailure)
	{
		return !bEnableDiskPersistence;
	}

	CaptureConnectedPlayers();
	CaptureWorldContainers();
	if (bDiskWritesBlockedByRestoreFailure)
	{
		return false;
	}
	if (!WritePreviousSnapshotToBackup())
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Synchronous save aborted because the previous valid snapshot could not be backed up."));
		return false;
	}

	URpgWorldSaveGame* Snapshot = BuildWorldSaveSnapshot();
	const bool bRecoverySaved = !WorldSaveRecoverySlotName.IsEmpty() &&
		UGameplayStatics::SaveGameToSlot(Snapshot, WorldSaveRecoverySlotName, WorldSaveUserIndex);
	const bool bPrimarySaved = UGameplayStatics::SaveGameToSlot(Snapshot, WorldSaveSlotName, WorldSaveUserIndex);
	if (bPrimarySaved || bRecoverySaved)
	{
		LastSuccessfulSaveGame = Snapshot;
		bWorldSaveDirty = false;
	}

	if (!bPrimarySaved)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgGameMode: Primary synchronous SaveGame write failed; recovery result=%s."),
			bRecoverySaved ? TEXT("saved") : TEXT("failed"));
	}
	return bPrimarySaved || bRecoverySaved;
}

bool ARpgGameModeBase::FlushWorldSave()
{
	if (!HasAuthority())
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(AutoSaveTimerHandle);
	return SaveWorldStateSync();
}

void ARpgGameModeBase::RegisterSaveStateListeners()
{
	UnregisterSaveStateListeners();
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	InventoryChangedSaveHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);
	ActionBarChangedSaveHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarChanged);
	EquipmentChangedSaveHandle = MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
		RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleEquipmentLoadoutChanged);
}

void ARpgGameModeBase::UnregisterSaveStateListeners()
{
	if (InventoryChangedSaveHandle.IsValid())
	{
		InventoryChangedSaveHandle.Unregister();
	}
	if (ActionBarChangedSaveHandle.IsValid())
	{
		ActionBarChangedSaveHandle.Unregister();
	}
	if (EquipmentChangedSaveHandle.IsValid())
	{
		EquipmentChangedSaveHandle.Unregister();
	}
}

void ARpgGameModeBase::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	if (bIsRestoringSaveState)
	{
		return;
	}

	URpgInventoryManagerComponent* Inventory = Cast<URpgInventoryManagerComponent>(Message.InventoryOwner);
	AActor* InventoryOwner = Inventory ? Inventory->GetOwner() : nullptr;
	if (ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(InventoryOwner))
	{
		MarkPlayerSaveDirty(PlayerState->GetRpgPlayerController());
		return;
	}

	URpgInventoryContainerComponent* Container = InventoryOwner
		? InventoryOwner->FindComponentByClass<URpgInventoryContainerComponent>()
		: nullptr;
	if (Container && !Container->GetPersistentContainerId().IsNone())
	{
		MarkWorldContainerSaveDirty(Container->GetPersistentContainerId(), Inventory);
	}
}

void ARpgGameModeBase::HandleActionBarChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	if (!bIsRestoringSaveState)
	{
		MarkPlayerSaveDirty(Cast<APlayerController>(Message.Owner));
	}
}

void ARpgGameModeBase::HandleEquipmentLoadoutChanged(
	FGameplayTag Channel,
	const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	if (!bIsRestoringSaveState)
	{
		MarkPlayerSaveDirty(Cast<APlayerController>(Message.Owner));
	}
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
	MarkPlayerSaveDirty(PC);
	FlushWorldSave();

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
		DropInventoryForPlayerDeath(PC, ExistingPawn->GetActorTransform());
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
	PC->SetIgnoreLookInput(false);
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

void ARpgGameModeBase::DropInventoryForPlayerDeath(APlayerController* PC, const FTransform& DropTransform)
{
	if (!PC || !HasAuthority())
	{
		return;
	}

	ARpgPlayerState* PlayerState = PC->GetPlayerState<ARpgPlayerState>();
	URpgInventoryManagerComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	if (!InventoryComponent || PlayerState->GetDeathDropMode() == ERpgPlayerDeathDropMode::None)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> InventoryEntries =
		InventoryComponent->GetAllEntries();
	TMap<FRpgInventoryItemId, int32> EntryIndexByItemId;
	TArray<bool> bEntryEligible;
	bEntryEligible.Init(false, InventoryEntries.Num());
	for (int32 EntryIndex = 0;
		EntryIndex < InventoryEntries.Num();
		++EntryIndex)
	{
		const FRpgInventoryEntryView& Entry =
			InventoryEntries[EntryIndex];
		URpgInventoryItemInstance* ItemInstance = Entry.Instance;
		if (!ItemInstance || Entry.StackCount <= 0 ||
			!Entry.ItemId.IsValid())
		{
			continue;
		}
		EntryIndexByItemId.Add(Entry.ItemId, EntryIndex);

		const FRpgInventoryContainerHandle EntryContainer =
			Entry.Placement.GetContainerHandle();
		const bool bOccupiesPhysicalGearSlot =
			URpgPlayerInventoryLayoutComponent::
				IsBuiltInGearContainer(EntryContainer);
		if (bOccupiesPhysicalGearSlot ||
			ItemInstance->FindFragmentByClass<
				URpgInventoryFragment_EquippableItem>() != nullptr)
		{
			continue;
		}

		const URpgInventoryFragment_ItemTraits* Traits = ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
		if (!Traits || !Traits->CanDropForMode(PlayerState->GetDeathDropMode()))
		{
			continue;
		}

		bEntryEligible[EntryIndex] = true;
	}

	auto IsDescendantOf =
		[&InventoryEntries, &EntryIndexByItemId](
			const FRpgInventoryEntryView& Candidate,
			const FRpgInventoryItemId& AncestorId)
		{
			FRpgInventoryContainerHandle Handle =
				Candidate.Placement.GetContainerHandle();
			TSet<FRpgInventoryItemId> VisitedOwnerIds;
			for (int32 Guard = 0;
				Guard <= InventoryEntries.Num() && Handle.IsItemOwned();
				++Guard)
			{
				if (Handle.ItemOwnerId == AncestorId)
				{
					return true;
				}
				if (VisitedOwnerIds.Contains(Handle.ItemOwnerId))
				{
					break;
				}

				VisitedOwnerIds.Add(Handle.ItemOwnerId);
				const int32* ParentIndex =
					EntryIndexByItemId.Find(Handle.ItemOwnerId);
				Handle = ParentIndex
					? InventoryEntries[*ParentIndex]
						  .Placement.GetContainerHandle()
					: FRpgInventoryContainerHandle();
			}
			return false;
		};

	TArray<int32> CandidateIndices;
	for (int32 EntryIndex = 0;
		EntryIndex < InventoryEntries.Num();
		++EntryIndex)
	{
		if (bEntryEligible[EntryIndex])
		{
			CandidateIndices.Add(EntryIndex);
		}
	}
	CandidateIndices.Sort(
		[&InventoryEntries](int32 A, int32 B)
		{
			const uint8 DepthA = InventoryEntries[A]
				.Placement.GetContainerHandle().Depth;
			const uint8 DepthB = InventoryEntries[B]
				.Placement.GetContainerHandle().Depth;
			return DepthA == DepthB ? A < B : DepthA < DepthB;
		});

	TArray<FRpgInventoryItemId> SelectedRootIds;
	for (const int32 CandidateIndex : CandidateIndices)
	{
		const FRpgInventoryEntryView& Candidate =
			InventoryEntries[CandidateIndex];
		bool bContainsProtectedDescendant = false;
		for (int32 PossibleDescendantIndex = 0;
			PossibleDescendantIndex < InventoryEntries.Num();
			++PossibleDescendantIndex)
		{
			if (!bEntryEligible[PossibleDescendantIndex] &&
				IsDescendantOf(
					InventoryEntries[PossibleDescendantIndex],
					Candidate.ItemId))
			{
				bContainsProtectedDescendant = true;
				break;
			}
		}
		if (bContainsProtectedDescendant)
		{
			continue;
		}

		const bool bCoveredBySelectedAncestor =
			SelectedRootIds.ContainsByPredicate(
				[&Candidate, &IsDescendantOf](
					const FRpgInventoryItemId& SelectedRootId)
				{
					return IsDescendantOf(
						Candidate,
						SelectedRootId);
				});
		if (!bCoveredBySelectedAncestor)
		{
			SelectedRootIds.Add(Candidate.ItemId);
		}
	}

	if (SelectedRootIds.IsEmpty() || !DeathDropActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.Instigator = PC->GetPawn();
	ARpgDroppedInventoryActor* DropActor = GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(DeathDropActorClass, DropTransform, SpawnParams);
	if (!DropActor)
	{
		return;
	}

	// A death drop is built exclusively from the player's concrete item graph. Clear any
	// Blueprint-authored static pickup defaults so they are not injected into every corpse.
	DropActor->SetPickupInventory(FInventoryPickup());

	bool bTransferredAnyItem = false;
	for (const FRpgInventoryItemId& SelectedRootId :
		SelectedRootIds)
	{
		URpgInventoryItemInstance* CurrentItem =
			InventoryComponent->FindItemById(SelectedRootId);
		const TArray<FRpgInventoryEntryView> CurrentEntries =
			InventoryComponent->GetAllEntries();
		const FRpgInventoryEntryView* SourceEntry =
			CurrentEntries.FindByPredicate(
				[&SelectedRootId](const FRpgInventoryEntryView& Entry)
				{
					return Entry.ItemId == SelectedRootId;
				});
		if (!CurrentItem || !SourceEntry ||
			SourceEntry->Instance != CurrentItem ||
			!SourceEntry->EntryId.IsValid() ||
			!SourceEntry->Placement.IsValid() ||
			SourceEntry->StackCount <= 0)
		{
			continue;
		}
		const int32 CurrentStackCount = SourceEntry->StackCount;
		FRpgInventoryTransferIntent DropIntent;
		DropIntent.RequestId = FGuid::NewGuid();
		DropIntent.ItemId = SelectedRootId;
		DropIntent.ExpectedEntryId = SourceEntry->EntryId;
		DropIntent.ExpectedSourcePlacement = SourceEntry->Placement;
		DropIntent.ExpectedSourceQuantity = CurrentStackCount;
		DropIntent.Quantity = CurrentStackCount;

		const FRpgInventoryMutationResult DropResult =
			DropActor->TransferItemFromInventoryByIntent(
				InventoryComponent,
				MoveTemp(DropIntent),
				true);
		if (!DropResult.IsSuccess() ||
			DropResult.AppliedQuantity != CurrentStackCount)
		{
			UE_LOG(
				LogRpg,
				Warning,
				TEXT("Death drop kept item %s in the player inventory because transfer failed with code %d."),
				*SelectedRootId.ToString(),
				static_cast<int32>(DropResult.Code));
			continue;
		}
		bTransferredAnyItem = true;
	}

	if (!bTransferredAnyItem)
	{
		DropActor->Destroy();
	}
	else if (URpgEquipmentLoadoutComponent* EquipmentLoadout =
				 PC->FindComponentByClass<
					 URpgEquipmentLoadoutComponent>())
	{
		// Container-only gear providers are valid physical equipment even without an
		// Equippable fragment. Reconcile once after all successful transfers so no slot or
		// remembered-offhand mirror can retain an item that now belongs to the corpse.
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory();
	}
}
