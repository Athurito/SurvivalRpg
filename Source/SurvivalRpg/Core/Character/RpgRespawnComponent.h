// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgRespawnComponent.generated.h"


/**
 * Respawn states for the death screen / respawn flow.
 * Flow: Player dies → WaitingForRespawn (death screen, timer) → Respawning → Alive.
 */
UENUM(BlueprintType)
enum class ERpgRespawnState : uint8
{
	/** Character is alive, no respawn needed. */
	Alive = 0,

	/** Character is dead, death screen is shown, waiting for respawn timer or input. */
	WaitingForRespawn,

	/** Respawn is in progress (teleporting, fading, etc.). */
	Respawning,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgRespawn_StateChanged, ERpgRespawnState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgRespawn_TimerTick, float, TimeRemaining, float, TimerDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgRespawn_Completed, FTransform, RespawnTransform);

/**
 * Client-side respawn UI component.
 * Tracks death screen timer and state. Actual respawn logic lives in the GameMode (server-authoritative).
 * When the timer elapses, the player can request respawn which is forwarded to the GameMode.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgRespawnComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Returns the respawn component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	static URpgRespawnComponent* FindRespawnComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgRespawnComponent>() : nullptr); }

	// --- Respawn Flow ---

	/**
	 * Called when the character truly dies (after downed/bleedout or instant death).
	 * Starts the death screen timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void StartRespawnTimer();

	/**
	 * Called to request respawn (e.g. player presses a button on the death screen).
	 * Forwards the request to the GameMode on the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void RequestRespawn();

	/**
	 * Called by the GameMode after it executes the respawn.
	 * Resets local UI state back to Alive.
	 */
	void OnServerRespawnExecuted(const FTransform& RespawnTransform);

	// --- Queries ---

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Respawn")
	ERpgRespawnState GetRespawnState() const { return RespawnState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Respawn")
	bool IsWaitingForRespawn() const { return RespawnState == ERpgRespawnState::WaitingForRespawn; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Respawn")
	bool CanRespawnNow() const { return RespawnState == ERpgRespawnState::WaitingForRespawn && bRespawnTimerElapsed; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Respawn")
	float GetRespawnTimeRemaining() const;

public:
	// --- Delegates ---

	/** Fired whenever the respawn state changes. */
	UPROPERTY(BlueprintAssignable)
	FRpgRespawn_StateChanged OnRespawnStateChanged;

	/** Fired every second while the respawn timer is active. */
	UPROPERTY(BlueprintAssignable)
	FRpgRespawn_TimerTick OnRespawnTimerTick;

	/** Fired when respawn completes. Provides the transform where the player respawned. */
	UPROPERTY(BlueprintAssignable)
	FRpgRespawn_Completed OnRespawnCompleted;

protected:
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetRespawnState(ERpgRespawnState NewState);
	void OnRespawnTimerExpired();

	/** Sends a Server RPC to the GameMode to request respawn. */
	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn();

private:
	ERpgRespawnState RespawnState = ERpgRespawnState::Alive;

	// --- Timer ---

	/** Minimum time the death screen is shown before the player can respawn. Dark Souls style. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Respawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 5.0f;

	/** Tracks remaining time on the respawn delay. */
	float RespawnTimerRemaining = 0.0f;

	/** True once the respawn delay has elapsed and the player can press to respawn. */
	bool bRespawnTimerElapsed = false;
};
