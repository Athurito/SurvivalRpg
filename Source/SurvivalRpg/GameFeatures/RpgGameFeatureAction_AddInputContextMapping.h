// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RpgGameFeatureAction_WorldActionBase.h"
#include "UObject/SoftObjectPtr.h"
#include "RpgGameFeatureAction_AddInputContextMapping.generated.h"

class APlayerController;
class UGameInstance;
class UInputMappingContext;
class ULocalPlayer;
class UPlayer;
struct FComponentRequestHandle;

USTRUCT(BlueprintType)
struct FRpgInputMappingContextAndPriority
{
	GENERATED_BODY()

	/** Input mapping context to add for local players while this feature is active. */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	/** Higher priority mappings win over lower priority mappings when actions overlap. */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;

	/** If true, the mapping is also registered with Enhanced Input user settings. */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bRegisterWithSettings = true;
};

/**
 * GameFeatureAction that adds Enhanced Input mapping contexts to local players.
 *
 * Runtime binding waits for the player pawn's gameplay component to signal that inputs are ready.
 */
UCLASS(meta = (DisplayName = "Add Rpg Input Mapping"))
class SURVIVALRPG_API URpgGameFeatureAction_AddInputContextMapping final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	virtual void OnGameFeatureUnregistering() override;
	//~ End UGameFeatureAction interface

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	/** Mapping contexts managed by this feature. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FRpgInputMappingContextAndPriority> InputMappings;

private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APlayerController>> ControllersAddedTo;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;
	/** Delegate used to register mapping contexts when game instances become available. */
	FDelegateHandle RegisterInputContextMappingsForGameInstanceHandle;

	//~ URpgGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End URpgGameFeatureAction_WorldActionBase interface

	/** Registers mappings with Enhanced Input user settings and hooks GameInstance/local-player changes. */
	void RegisterInputMappingContexts();
	/** Registers mappings for all local players in the given game instance. */
	void RegisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);
	/** Registers mappings for one local player. */
	void RegisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);
	/** Unregisters mappings and removes GameInstance/local-player hooks. */
	void UnregisterInputMappingContexts();
	/** Unregisters mappings for all local players in the given game instance. */
	void UnregisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);
	/** Unregisters mappings for one local player. */
	void UnregisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);
	void Reset(FPerContextData& ActiveData);
	void HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddInputMappingForPlayer(UPlayer* Player, APlayerController* PlayerController, FPerContextData& ActiveData);
	void RemoveInputMapping(APlayerController* PlayerController, FPerContextData& ActiveData);
};
