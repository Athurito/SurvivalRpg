// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RpgBasePlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "RpgPlayerState.generated.h"

class ARpgPlayerController;
class URpgTradeSkillProgressionComponent;
class URpgInventoryCapacitySet;
class URpgPlayerProgressionComponent;
class URpgInventoryManagerComponent;
class URpgExperienceDefinition;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgReplicatedRespawnState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	bool bIsWaitingForRespawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	float RespawnAvailableServerTime = 0.0f;

	bool operator==(const FRpgReplicatedRespawnState& Other) const
	{
		return bIsWaitingForRespawn == Other.bIsWaitingForRespawn
			&& FMath::IsNearlyEqual(RespawnAvailableServerTime, Other.RespawnAvailableServerTime);
	}

	bool operator!=(const FRpgReplicatedRespawnState& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgReplicatedCheckpointState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	bool bHasCheckpoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Respawn")
	FTransform CheckpointTransform = FTransform::Identity;

	bool operator==(const FRpgReplicatedCheckpointState& Other) const
	{
		return bHasCheckpoint == Other.bHasCheckpoint
			&& CheckpointTransform.Equals(Other.CheckpointTransform);
	}

	bool operator!=(const FRpgReplicatedCheckpointState& Other) const
	{
		return !(*this == Other);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgPlayerState_RespawnStateChanged, bool, bIsWaitingForRespawn, float, RespawnAvailableServerTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgPlayerState_CheckpointChanged, bool, bHasCheckpoint, FTransform, CheckpointTransform);

UCLASS()
class SURVIVALRPG_API ARpgPlayerState : public ARpgBasePlayerState
{
	GENERATED_BODY()
	
public:
	ARpgPlayerState();

	virtual void PostInitializeComponents() override;
	virtual void ClientInitialize(AController* C) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerState")
	ARpgPlayerController* GetRpgPlayerController() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	URpgInventoryManagerComponent* GetInventoryManagerComponent() const { return InventoryManagerComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	void SetDeathDropMode(ERpgPlayerDeathDropMode NewDropMode);

	UFUNCTION(BlueprintPure, Category = "Rpg|Inventory")
	ERpgPlayerDeathDropMode GetDeathDropMode() const { return DeathDropMode; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Progression")
	URpgPlayerProgressionComponent* GetPlayerProgressionComponent() const { return PlayerProgressionComponent; }

	void SetRespawnState(bool bInIsWaitingForRespawn, float InRespawnAvailableServerTime);
	void SetCheckpointData(bool bInHasCheckpoint, const FTransform& InCheckpointTransform);

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	bool IsWaitingForRespawn() const { return RespawnState.bIsWaitingForRespawn; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	float GetRespawnAvailableServerTime() const { return RespawnState.RespawnAvailableServerTime; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	bool HasCheckpoint() const { return CheckpointState.bHasCheckpoint; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	const FTransform& GetCheckpointTransform() const { return CheckpointState.CheckpointTransform; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	float GetRemainingRespawnTime() const;

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	bool CanRespawnNow() const;

	UPROPERTY(BlueprintAssignable, Category = "Rpg|Respawn")
	FRpgPlayerState_RespawnStateChanged OnRespawnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rpg|Respawn")
	FRpgPlayerState_CheckpointChanged OnCheckpointChanged;
	
protected:
	UFUNCTION()
	void OnRep_RespawnState();

	UFUNCTION()
	void OnRep_CheckpointState();

	void OnExperienceLoaded(const URpgExperienceDefinition* CurrentExperience);
	void BroadcastRespawnStateChanged() const;
	void BroadcastCheckpointChanged() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgPlayerProgressionComponent> PlayerProgressionComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgTradeSkillProgressionComponent> TradeSkillProgressionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rpg|Inventory")
	TObjectPtr<URpgInventoryManagerComponent> InventoryManagerComponent;

	/** Player-owned GAS attributes that can increase backpack capacity through gear, runes, or progression. */
	UPROPERTY()
	TObjectPtr<const URpgInventoryCapacitySet> InventoryCapacitySet;

	/** Player-facing death-drop setting replicated for UI and consumed server-side by GameMode on final death. */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory")
	ERpgPlayerDeathDropMode DeathDropMode = ERpgPlayerDeathDropMode::MaterialsOnly;

	UPROPERTY(ReplicatedUsing = OnRep_RespawnState, VisibleAnywhere, Category = "Rpg|Respawn")
	FRpgReplicatedRespawnState RespawnState;

	UPROPERTY(ReplicatedUsing = OnRep_CheckpointState, VisibleAnywhere, Category = "Rpg|Respawn")
	FRpgReplicatedCheckpointState CheckpointState;
};
