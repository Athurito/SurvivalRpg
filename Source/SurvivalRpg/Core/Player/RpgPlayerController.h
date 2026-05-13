// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/PlayerController.h"
#include "RpgPlayerController.generated.h"

class URpgAbilitySystemComponent;
class URpgQuickBarComponent;
class UInputMappingContext;
class ARpgPlayerState;

UCLASS(Abstract)
class SURVIVALRPG_API ARpgPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ARpgPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerController")
	ARpgPlayerState* GetRpgPlayerState() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerController")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Respawn")
	void RequestRespawn();

	UFUNCTION(BlueprintCallable, Category = "Rpg|QuickBar")
	URpgQuickBarComponent* GetQuickBarComponent() const { return QuickBarComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|QuickBar")
	void SetActiveQuickBarSlot(int32 SlotIndex);

	virtual void OnRep_PlayerState() override;
	//~APlayerController interface
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	//~End of APlayerController interface

	//~IGenericTeamAgentInterface interface
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~End of IGenericTeamAgentInterface interface

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	void SetIsAutoRunning(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Character")
	bool GetIsAutoRunning() const;
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Character", meta = (DisplayName = "On Start Auto Run"))
	void K2_OnStartAutoRun();

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Character", meta = (DisplayName = "On End Auto Run"))
	void K2_OnEndAutoRun();
	
protected:
	virtual void BeginPlayingState() override;
	virtual void SetupInputComponent() override;
	

private:
	void OnStartAutoRun();
	void OnEndAutoRun();
	void RefreshPlayerStateBindings();
	void BindToPlayerState(ARpgPlayerState* NewPlayerState);
	void UnbindFromPlayerState();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(Transient)
	TObjectPtr<ARpgPlayerState> BoundPlayerState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|QuickBar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgQuickBarComponent> QuickBarComponent;
};
