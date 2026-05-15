// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerSaveData.h"
#include "ModularGameMode.h"
#include "RpgGameModeBase.generated.h"

class AGameModeBase;
class URpgPawnData;
class URpgAbilitySystemComponent;
class URpgExperienceDefinition;

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

	/** Returns the full save data map (e.g. for serialization). */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Save")
	const TMap<FUniqueNetIdRepl, FRpgPlayerSaveData>& GetAllPlayerSaveData() const { return PlayerSaveDataMap; }

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

public:
	/** Fired on the server when a player respawns. */
	UPROPERTY(BlueprintAssignable)
	FRpgRespawn_OnPlayerRespawned OnPlayerRespawned;

	FOnRpgGameModePlayerInitialized OnGameModePlayerInitialized;

protected:
	/** Executes the actual respawn logic. Override for custom behavior. */
	virtual void ExecuteRespawn(APlayerController* PC, const FTransform& SpawnPoint);

private:
	/** Helper to get a stable NetId key from a PlayerController. */
	static FUniqueNetIdRepl GetNetIdForPC(const APlayerController* PC);

	FRpgPlayerRespawnState& GetOrCreatePlayerRespawnState(APlayerController* PC);
	const FRpgPlayerRespawnState* FindPlayerRespawnState(APlayerController* PC) const;

	void HandleMatchAssignmentIfNotExpectingOne();
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource);
	void OnExperienceLoaded(const URpgExperienceDefinition* CurrentExperience);

	void SyncPlayerCheckpointDataToPlayerState(APlayerController* PC);
	void SyncPlayerRespawnStateToPlayerState(APlayerController* PC);
	void ResetPlayerRespawnState(APlayerController* PC);
	static void ClearRespawnGameplayState(URpgAbilitySystemComponent* ASC);

	/** Host-authoritative persistent save data. Keyed by Steam NetId. */
	UPROPERTY()
	TMap<FUniqueNetIdRepl, FRpgPlayerSaveData> PlayerSaveDataMap;

	/** Host-authoritative runtime respawn state. Keyed by Steam NetId. */
	UPROPERTY()
	TMap<FUniqueNetIdRepl, FRpgPlayerRespawnState> PlayerRespawnStateMap;
};
