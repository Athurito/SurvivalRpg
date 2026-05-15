// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "LoadingProcessInterface.h"
#include "RpgExperienceManagerComponent.generated.h"

namespace UE::GameFeatures
{
	struct FResult;
}

class URpgExperienceDefinition;

/** Delegate fired when the current experience has finished loading and executing its startup actions. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRpgExperienceLoaded, const URpgExperienceDefinition*);

/** Internal load state for the active gameplay experience. */
enum class ERpgExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	LoadingChaosTestingDelay,
	ExecutingActions,
	Loaded,
	Deactivating
};

/**
 * GameState component that owns the current gameplay experience lifecycle.
 *
 * It loads the selected experience asset, activates requested GameFeature plugins,
 * executes GameFeature actions, and notifies gameplay systems once the experience is ready.
 */
UCLASS()
class SURVIVALRPG_API URpgExperienceManagerComponent final : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	explicit URpgExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent interface

	//~ ILoadingProcessInterface interface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~ End ILoadingProcessInterface interface

	/** Sets the current experience and starts loading it. Should only be called once per match/world setup. */
	void SetCurrentExperience(FPrimaryAssetId ExperienceId);

	/** Registers a callback that runs before normal experience-loaded listeners. Calls immediately if already loaded. */
	void CallOrRegister_OnExperienceLoaded_HighPriority(FOnRpgExperienceLoaded::FDelegate&& Delegate);
	/** Registers a normal experience-loaded callback. Calls immediately if already loaded. */
	void CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate&& Delegate);
	/** Registers a callback that runs after normal experience-loaded listeners. Calls immediately if already loaded. */
	void CallOrRegister_OnExperienceLoaded_LowPriority(FOnRpgExperienceLoaded::FDelegate&& Delegate);

	/** Returns the loaded experience, asserting if called before loading has completed. */
	const URpgExperienceDefinition* GetCurrentExperienceChecked() const;
	/** Returns true once the experience asset, features, and startup actions are ready. */
	bool IsExperienceLoaded() const;

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();
	void OnExperienceLoadComplete();
	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);
	void OnExperienceFullLoadCompleted();
	void OnActionDeactivationCompleted();
	void OnAllActionsDeactivated();

private:
	/** Replicated experience selection. Clients start their own load when this value arrives. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const URpgExperienceDefinition> CurrentExperience;

	ERpgExperienceLoadState LoadState = ERpgExperienceLoadState::Unloaded;

	/** Feature plugin URLs collected from the experience and composed action sets. */
	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	/** Counts asynchronous GameFeature action deactivation pausers during cleanup. */
	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	/** Called just before normal experience-loaded listeners, for systems that must initialize first. */
	FOnRpgExperienceLoaded OnExperienceLoaded_HighPriority;
	/** Called when the experience has finished loading. */
	FOnRpgExperienceLoaded OnExperienceLoaded;
	/** Called after normal experience-loaded listeners, for late setup. */
	FOnRpgExperienceLoaded OnExperienceLoaded_LowPriority;
};
