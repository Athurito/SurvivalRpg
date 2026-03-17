// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RpgPlayerState.generated.h"

class UBasePawnData;
class ARpgPlayerController;
class URpgTradeSkillProgressionComponent;
class URpgPlayerProgressionComponent;
class URpgAbilitySystemComponent;

UCLASS()
class SURVIVALRPG_API ARpgPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	ARpgPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerState")
	ARpgPlayerController* GetRpgPlayerController() const;
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SendAbilitiesChangedEvent();
	
	TObjectPtr<URpgAbilitySystemComponent> GetRpgAbilitySystemComponent() const;
	
	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	void SetPawnData(const UBasePawnData* InPawnData);
	void SetRespawnState(bool bInIsWaitingForRespawn, float InRespawnAvailableServerTime);
	void SetCheckpointData(bool bInHasCheckpoint, const FTransform& InCheckpointTransform);

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	bool IsWaitingForRespawn() const { return bIsWaitingForRespawn; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	float GetRespawnAvailableServerTime() const { return RespawnAvailableServerTime; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	bool HasCheckpoint() const { return bHasCheckpoint; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Respawn")
	const FTransform& GetCheckpointTransform() const { return CurrentCheckpointTransform; }
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Rpg|AbilitySystem")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgPlayerProgressionComponent> PlayerProgressionComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Rpg|Progression")
	TObjectPtr<URpgTradeSkillProgressionComponent> TradeSkillProgressionComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Pawn")
	TObjectPtr<const UBasePawnData> PawnData;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Respawn")
	bool bIsWaitingForRespawn = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Respawn")
	float RespawnAvailableServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Respawn")
	bool bHasCheckpoint = false;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Respawn")
	FTransform CurrentCheckpointTransform = FTransform::Identity;
	
private:
	UPROPERTY()
	TObjectPtr<const class URpgHealthSet> HealthSet;
};
