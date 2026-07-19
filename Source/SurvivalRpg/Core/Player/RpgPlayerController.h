// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayTagContainer.h"
#include "ModularPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "RpgPlayerController.generated.h"

class URpgAbilitySystemComponent;
class URpgActionBarComponent;
class URpgEquipmentLoadoutComponent;
class URpgInventoryManagerComponent;
class URpgInventoryUiActionComponent;
class URpgPlayerInventoryLayoutComponent;
class URpgPlayerGameplayInputRouterComponent;
class URpgWeaponAbilityLoadoutComponent;
class URpgPawnExtensionComponent;
class UInputMappingContext;
class ARpgPlayerState;
class ARpgGameModeBase;

UCLASS(Abstract)
class SURVIVALRPG_API ARpgPlayerController : public AModularPlayerController, public IGenericTeamAgentInterface
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

	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	void SetDeathDropMode(ERpgPlayerDeathDropMode NewDropMode);

	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Rpg|Respawn")
	void ClientRestoreGameplayInputFocus();

	/** Opens the local loot screen for a server-authoritative loot inventory that could not be fully auto-collected. */
	UFUNCTION(Client, Reliable, Category = "Rpg|Inventory")
	void ClientOpenLootInventory(URpgInventoryManagerComponent* PrimaryInventory, URpgInventoryManagerComponent* LootInventory, AActor* LootActor);

	/** Opens and monitors an accessible world storage inventory on the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	void OpenStorageInventory(URpgInventoryManagerComponent* PrimaryInventory, URpgInventoryManagerComponent* StorageInventory, AActor* StorageActor);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Action Bar")
	URpgActionBarComponent* GetActionBarComponent() const { return ActionBarComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Weapon Abilities")
	URpgWeaponAbilityLoadoutComponent* GetWeaponAbilityLoadoutComponent() const { return WeaponAbilityLoadoutComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Equipment")
	URpgEquipmentLoadoutComponent* GetEquipmentLoadoutComponent() const { return EquipmentLoadoutComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	URpgInventoryUiActionComponent* GetInventoryUiActionComponent() const { return InventoryUiActionComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rpg|Inventory")
	URpgPlayerInventoryLayoutComponent* GetPlayerInventoryLayoutComponent() const { return PlayerInventoryLayoutComponent; }

	/** Local systemic-input router observed by the gameplay quick-access radial. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Input")
	URpgPlayerGameplayInputRouterComponent* GetGameplayInputRouterComponent() const { return GameplayInputRouterComponent; }

	UFUNCTION(Exec)
	void RpgPrintProgression() const;

	virtual void OnRep_PlayerState() override;
	//~APlayerController interface
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
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

	UFUNCTION(Server, Reliable)
	void ServerSetDeathDropMode(ERpgPlayerDeathDropMode NewDropMode);

	UFUNCTION()
	void HandleRespawnStateChanged(bool bIsWaitingForRespawn, float RespawnAvailableServerTime);

	UFUNCTION()
	void HandleCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Respawn", meta = (DisplayName = "On Checkpoint Changed"))
	void K2_OnCheckpointChanged(bool bHasCheckpoint, FTransform CheckpointTransform);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Character", meta = (DisplayName = "On Start Auto Run"))
	void K2_OnStartAutoRun();

	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Character", meta = (DisplayName = "On End Auto Run"))
	void K2_OnEndAutoRun();
	
protected:
	virtual void BeginPlayingState() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	

private:
	void OnStartAutoRun();
	void OnEndAutoRun();
	void RestoreGameplayInputFocus();
	void BindToPawnExtensionForLoadout(APawn* InPawn);
	void UnbindFromPawnExtensionForLoadout();
	void HandlePossessedPawnAbilitySystemInitialized();
	void HandlePossessedPawnAbilitySystemUninitialized();
	void BindToGameModeRespawnEvent();
	void UnbindFromGameModeRespawnEvent();
	UFUNCTION()
	void HandleGameModePlayerRespawned(APlayerController* RespawnedPlayerController, FTransform RespawnTransform);
	void RefreshPlayerStateBindings();
	void BindToPlayerState(ARpgPlayerState* NewPlayerState);
	void UnbindFromPlayerState();
	void OpenInventoryContainerScreen(FGameplayTag ScreenTag, URpgInventoryManagerComponent* PrimaryInventory, URpgInventoryManagerComponent* SecondaryInventory, AActor* ContextActor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(Transient)
	TObjectPtr<ARpgPlayerState> BoundPlayerState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Action Bar", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgActionBarComponent> ActionBarComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Weapon Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgWeaponAbilityLoadoutComponent> WeaponAbilityLoadoutComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPlayerGameplayInputRouterComponent> GameplayInputRouterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgEquipmentLoadoutComponent> EquipmentLoadoutComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryUiActionComponent> InventoryUiActionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgPlayerInventoryLayoutComponent> PlayerInventoryLayoutComponent;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundLoadoutPawnExtension;

	UPROPERTY(Transient)
	TObjectPtr<ARpgGameModeBase> BoundRespawnGameMode;

	/** Local loot/storage context monitored for access loss while its CommonUI screen is open. */
	TWeakObjectPtr<AActor> ActiveLootContextActor;
	FGameplayTag ActiveInventoryContextScreenTag;
	bool bHasActiveLootContext = false;
};
