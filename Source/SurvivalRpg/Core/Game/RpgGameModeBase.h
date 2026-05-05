// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerSaveData.h"
#include "GameFramework/GameModeBase.h"
#include "RpgGameModeBase.generated.h"

class URpgPawnData;
class URpgAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgRespawn_OnPlayerRespawned, APlayerController*, PC, FTransform, RespawnTransform);

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
 * - Manages host-owned player save data and checkpoint registration.
 * - Runs a Lyra-style respawn flow by spawning a fresh pawn and reusing the persistent ASC on the PlayerState.
 */
UCLASS()
class SURVIVALRPG_API ARpgGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	const URpgPawnData* GetPawnDataForController(const AController* InController) const;

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

protected:
	/** Executes the actual respawn logic. Override for custom behavior. */
	virtual void ExecuteRespawn(APlayerController* PC, const FTransform& SpawnPoint);

private:
	/** Helper to get a stable NetId key from a PlayerController. */
	static FUniqueNetIdRepl GetNetIdForPC(const APlayerController* PC);

	FRpgPlayerRespawnState& GetOrCreatePlayerRespawnState(APlayerController* PC);
	const FRpgPlayerRespawnState* FindPlayerRespawnState(APlayerController* PC) const;

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
