// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RpgPlayerController.generated.h"

class URpgAbilitySystemComponent;
class UInputMappingContext;
class ARpgPlayerState;

UCLASS(Abstract)
class SURVIVALRPG_API ARpgPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerController")
	ARpgPlayerState* GetRpgPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerController")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void RequestRespawn();

	virtual void OnRep_PlayerState() override;
	//~APlayerController interface
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	//~End of APlayerController interface
protected:
	
	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn();

	UFUNCTION()
	void HandleRespawnStateChanged(bool bIsWaitingForRespawn, float RespawnAvailableServerTime);

	UFUNCTION()
	void HandleCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Respawn", meta = (DisplayName = "On Respawn State Changed"))
	void K2_OnRespawnStateChanged(bool bIsWaitingForRespawn, float RespawnAvailableServerTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Respawn", meta = (DisplayName = "On Checkpoint Changed"))
	void K2_OnCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform);
	
protected:
	virtual void BeginPlayingState() override;
	virtual void SetupInputComponent() override;
	

private:
	void RefreshPlayerStateBindings();
	void BindToPlayerState(ARpgPlayerState* NewPlayerState);
	void UnbindFromPlayerState();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(Transient)
	TObjectPtr<ARpgPlayerState> BoundPlayerState;
};
