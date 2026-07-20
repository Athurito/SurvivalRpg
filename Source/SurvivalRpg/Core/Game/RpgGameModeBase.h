// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgWorldSaveGame.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "ModularGameMode.h"
#include "RpgGameModeBase.generated.h"

class AGameModeBase;
class ARpgDroppedInventoryActor;
class URpgPawnData;
class URpgAbilitySystemComponent;
class URpgExperienceDefinition;
class URpgInventoryManagerComponent;
struct FRpgActionBarSlotsChangedMessage;
struct FRpgEquipmentLoadoutSlotsChangedMessage;
struct FRpgInventoryChangeMessage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgRespawn_OnPlayerRespawned, APlayerController*, PC, FTransform, RespawnTransform);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRpgGameModePlayerInitialized, AGameModeBase* /*GameMode*/, AController* /*NewPlayer*/);

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPlayerRespawnState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	bool bIsWaitingForRespawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	float RespawnAvailableServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	FTransform PendingRespawnTransform = FTransform::Identity;
};

/**
 * Server-authoritative GameMode.
 *
 * Selects and loads the gameplay experience, waits for it before starting players,
 * resolves PawnData for controllers, and owns host-only survival save/respawn state.
 */
UCLASS()
class SURVIVALRPG_API ARpgGameModeBase : public AModularGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;
	virtual void GenericPlayerInitialization(AController* NewPlayer) override;
	virtual void FailedToRestartPlayer(AController* NewPlayer) override;

	/** Returns the PawnData that should drive pawn class selection and startup grants for the controller. */
	const URpgPawnData* GetPawnDataForController(const AController* InController) const;
	/** Returns true once the current experience has fully loaded. */
	bool IsExperienceLoaded() const;

	/** Restarts the player or AI controller next frame, optionally resetting the controller first. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Player")
	void RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset = false);

	/** Pawn-agnostic restart gate that works for both real players and AI controllers. */
	virtual bool ControllerCanRestart(AController* Controller);

	// --- Save Data API (host-authoritative) ---

	/** Returns save data for a player. Creates a default entry if none exists. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Save")
	FRpgPlayerSaveData& GetOrCreatePlayerSaveData(APlayerController* PC);

	/** Returns save data for a player (read-only). Returns nullptr if not found. */
	const FRpgPlayerSaveData* FindPlayerSaveData(APlayerController* PC) const;

	/** Returns the host-owned profile map keyed by UniqueNetId string or the stable offline profile key. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Save")
	const TMap<FString, FRpgPlayerSaveData>& GetAllPlayerSaveData() const { return PlayerSaveDataMap; }

	/** Resolves the stable disk profile key used for this controller on the authoritative host. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Save")
	FString GetPlayerProfileKey(const APlayerController* PC) const;

	/** True once the authoritative host attempted disk restore for this controller connection, including when no save exists. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Save")
	bool IsPlayerProfileRestoreComplete(const APlayerController* PC) const;

	/** True only when this controller connection atomically restored a saved inventory graph. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Save")
	bool HasRestoredPlayerProfile(const APlayerController* PC) const;

	/** Marks player state dirty and restarts the two-second asynchronous disk-save debounce. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Save")
	void MarkPlayerSaveDirty(APlayerController* PC);

	/** Captures one persistent world container and restarts the asynchronous save debounce. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Save")
	void MarkWorldContainerSaveDirty(FName PersistentContainerId, URpgInventoryManagerComponent* Inventory);

	/** Atomically imports a saved world-container graph, trying valid backup snapshots if the newest graph fails. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Save")
	bool RestoreWorldContainer(FName PersistentContainerId, URpgInventoryManagerComponent* Inventory);

	/** Synchronously captures and writes all current host state, used by checkpoints, logout, and shutdown. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Save")
	bool FlushWorldSave();

	// --- Checkpoint API ---

	/** Registers a checkpoint for a player. Stored in the host's save data map. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void SetPlayerCheckpoint(APlayerController* PC, const FTransform& CheckpointTransform);

	/** Returns the checkpoint transform for a player. Falls back to the default spawn. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	FTransform GetPlayerCheckpointTransform(APlayerController* PC) const;

	// --- Respawn API ---

	/** Marks a player as dead on the host and starts the respawn delay. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void NotifyPlayerDeath(APlayerController* PC);

	/** Returns whether a player may currently respawn according to the host's runtime state. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	bool CanRespawnPlayer(APlayerController* PC) const;

	/**
	 * Called when a player requests respawn (e.g. from a death screen).
	 * Server-authoritative: validates the pending respawn state and restarts the player at the saved checkpoint.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void RequestPlayerRespawn(APlayerController* PC);

	/** Minimum time the death screen is shown before the player can respawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Respawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 5.0f;

	/** Pickup actor spawned when death-drop rules produce material or backpack loot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Inventory")
	TSubclassOf<ARpgDroppedInventoryActor> DeathDropActorClass;

	/** Primary SaveGame slot containing the newest successfully written host snapshot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk")
	FString WorldSaveSlotName = TEXT("SurvivalRpg_World");

	/** Previous valid snapshot written before the primary slot is replaced. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk")
	FString WorldSaveBackupSlotName = TEXT("SurvivalRpg_World_Backup");

	/** Synchronous-flush safety slot; its sequence wins if an older async write completes during shutdown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk")
	FString WorldSaveRecoverySlotName = TEXT("SurvivalRpg_World_Recovery");

	/** Platform save user index supplied to Unreal's SaveGame API. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk", meta = (ClampMin = "0", UIMin = "0"))
	int32 WorldSaveUserIndex = 0;

	/** Stable fallback profile key used when no valid online UniqueNetId exists. May be overridden by ?ProfileKey=. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk")
	FString OfflineProfileKey = TEXT("LocalProfile");

	/** Quiet period in seconds after the latest mutation before an asynchronous disk snapshot is started. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float AutoSaveDebounceSeconds = 2.0f;

	/** Enables host disk persistence. Disable only for transient test modes that intentionally discard progress. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Save|Disk")
	bool bEnableDiskPersistence = true;

public:
	/** Fired on the server when a player respawns. */
	UPROPERTY(BlueprintAssignable)
	FRpgRespawn_OnPlayerRespawned OnPlayerRespawned;

	FOnRpgGameModePlayerInitialized OnGameModePlayerInitialized;

protected:
	/** Executes the actual respawn logic. Override for custom behavior. */
	virtual void ExecuteRespawn(APlayerController* PC, const FTransform& SpawnPoint);

private:
	/** Runtime-only respawn key retained for the existing replicated session state. */
	static FUniqueNetIdRepl GetNetIdForPC(const APlayerController* PC);

	bool RestorePlayerProfile(APlayerController* PC);
	bool TryRestorePlayerSaveData(APlayerController* PC, const FRpgPlayerSaveData& SaveData);
	void CapturePlayerSaveData(APlayerController* PC);
	void CaptureConnectedPlayers();
	void CaptureWorldContainers();
	void RestorePlacedWorldContainers();
	void ApplyRestoredEquipmentSelection(APlayerController* PC);

	void LoadWorldSaveFromDisk();
	URpgWorldSaveGame* BuildWorldSaveSnapshot();
	void ScheduleAsyncWorldSave();
	void SaveWorldStateAsync();
	bool SaveWorldStateSync();
	void HandleAsyncSaveCompleted(const FString& SlotName, int32 UserIndex, bool bSuccess);
	bool WritePreviousSnapshotToBackup() const;
	void MarkWorldSaveDirty();

	void RegisterSaveStateListeners();
	void UnregisterSaveStateListeners();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleActionBarChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);
	void HandleEquipmentLoadoutChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message);

	FRpgPlayerRespawnState& GetOrCreatePlayerRespawnState(APlayerController* PC);
	const FRpgPlayerRespawnState* FindPlayerRespawnState(APlayerController* PC) const;

	void HandleMatchAssignmentIfNotExpectingOne();
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource);
	void OnExperienceLoaded(const URpgExperienceDefinition* CurrentExperience);

	void SyncPlayerCheckpointDataToPlayerState(APlayerController* PC);
	void SyncPlayerRespawnStateToPlayerState(APlayerController* PC);
	void ResetPlayerRespawnState(APlayerController* PC);
	static void ClearRespawnGameplayState(URpgAbilitySystemComponent* ASC);
	void DropInventoryForPlayerDeath(APlayerController* PC, const FTransform& DropTransform);

	/** Host-authoritative persistent player data keyed by online id string or stable offline profile key. */
	UPROPERTY()
	TMap<FString, FRpgPlayerSaveData> PlayerSaveDataMap;

	/** Host-authoritative persistent physical world-container graphs keyed by stable container id. */
	UPROPERTY()
	TMap<FName, FRpgWorldContainerSaveData> WorldContainerSaveDataMap;

	/** Host-authoritative runtime respawn state. Keyed by Steam NetId. */
	UPROPERTY()
	TMap<FUniqueNetIdRepl, FRpgPlayerRespawnState> PlayerRespawnStateMap;

	/** Structurally valid disk candidates sorted newest-first for atomic per-graph fallback restore. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<URpgWorldSaveGame>> ValidLoadedSaveCandidates;

	/** Most recent disk snapshot known to have completed successfully. */
	UPROPERTY(Transient)
	TObjectPtr<URpgWorldSaveGame> LastSuccessfulSaveGame;

	/** Immutable snapshot currently owned by Unreal's async SaveGame task. */
	UPROPERTY(Transient)
	TObjectPtr<URpgWorldSaveGame> ActiveAsyncSaveGame;

	/** Connection-scoped restore result: presence means the attempt completed; true means a saved graph was restored. */
	TMap<TWeakObjectPtr<APlayerController>, bool> PlayerProfileRestoreStates;
	FString ResolvedOfflineProfileKey;
	int64 NextSaveSequence = 1;
	bool bWorldSaveDirty = false;
	bool bAsyncSaveInFlight = false;
	bool bSaveQueuedDuringAsync = false;
	bool bIsRestoringSaveState = false;
	bool bDiskWritesBlockedByRestoreFailure = false;
	FTimerHandle AutoSaveTimerHandle;

	FGameplayMessageListenerHandle InventoryChangedSaveHandle;
	FGameplayMessageListenerHandle ActionBarChangedSaveHandle;
	FGameplayMessageListenerHandle EquipmentChangedSaveHandle;
};
