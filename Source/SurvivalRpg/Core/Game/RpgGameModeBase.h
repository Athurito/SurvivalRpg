// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgPlayerSaveData.h"
#include "GameFramework/GameModeBase.h"
#include "RpgGameModeBase.generated.h"

class UBasePawnData;
class URpgHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgRespawn_OnPlayerRespawned, APlayerController*, PC, FTransform, RespawnTransform);

/**
 * Server-authoritative GameMode.
 * - Manages respawn flow (checkpoint lookup, teleport, state restore).
 * - Stores all player save data keyed by Steam NetId (host-authoritative, like Dark Souls / not like Valheim).
 */
UCLASS()
class SURVIVALRPG_API ARpgGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	const UBasePawnData* GetPawnDataForController(const AController* InController) const;

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

	/** Returns the checkpoint transform for a player. Falls back to default spawn. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	FTransform GetPlayerCheckpointTransform(APlayerController* PC) const;

	// --- Respawn API ---

	/**
	 * Called when a player requests respawn (e.g. after death screen timer).
	 * Server-authoritative: validates, teleports pawn, restores health, clears death state.
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

	/** Host-authoritative save data. Keyed by Steam NetId. */
	UPROPERTY()
	TMap<FUniqueNetIdRepl, FRpgPlayerSaveData> PlayerSaveDataMap;
};
