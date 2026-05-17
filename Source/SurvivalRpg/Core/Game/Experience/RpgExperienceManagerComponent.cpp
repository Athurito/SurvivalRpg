// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgExperienceManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Net/UnrealNetwork.h"
#include "RpgExperienceActionSet.h"
#include "RpgExperienceDefinition.h"
#include "RpgExperienceManager.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/System/RpgAssetManager.h"
#include "TimerManager.h"

namespace RpgConsoleVariables
{
	static float ExperienceLoadRandomDelayMin = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayMin(
		TEXT("rpg.chaos.ExperienceDelayLoad.MinSecs"),
		ExperienceLoadRandomDelayMin,
		TEXT("This value in seconds is added to experience load completion for testing."),
		ECVF_Default);

	static float ExperienceLoadRandomDelayRange = 0.0f;
	static FAutoConsoleVariableRef CVarExperienceLoadRandomDelayRange(
		TEXT("rpg.chaos.ExperienceDelayLoad.RandomSecs"),
		ExperienceLoadRandomDelayRange,
		TEXT("A random amount between 0 and this value is added to experience load completion for testing."),
		ECVF_Default);

	float GetExperienceLoadDelayDuration()
	{
		return FMath::Max(0.0f, ExperienceLoadRandomDelayMin + FMath::FRand() * ExperienceLoadRandomDelayRange);
	}
}

URpgExperienceManagerComponent::URpgExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgExperienceManagerComponent::SetCurrentExperience(FPrimaryAssetId ExperienceId)
{
	URpgAssetManager& AssetManager = URpgAssetManager::Get();
	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	UObject* LoadedObject = AssetPath.TryLoad();

	const URpgExperienceDefinition* Experience = Cast<URpgExperienceDefinition>(LoadedObject);
	if (!Experience)
	{
		if (const UClass* ExperienceClass = Cast<UClass>(LoadedObject))
		{
			Experience = GetDefault<URpgExperienceDefinition>(ExperienceClass);
		}
	}

	check(Experience);
	check(CurrentExperience == nullptr);

	CurrentExperience = Experience;
	StartExperienceLoad();
}

void URpgExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_HighPriority(FOnRpgExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_HighPriority.Add(MoveTemp(Delegate));
	}
}

void URpgExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded.Add(MoveTemp(Delegate));
	}
}

void URpgExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_LowPriority(FOnRpgExperienceLoaded::FDelegate&& Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.Execute(CurrentExperience);
	}
	else
	{
		OnExperienceLoaded_LowPriority.Add(MoveTemp(Delegate));
	}
}

const URpgExperienceDefinition* URpgExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(LoadState == ERpgExperienceLoadState::Loaded);
	check(CurrentExperience != nullptr);
	return CurrentExperience;
}

bool URpgExperienceManagerComponent::IsExperienceLoaded() const
{
	return (LoadState == ERpgExperienceLoadState::Loaded) && (CurrentExperience != nullptr);
}

bool URpgExperienceManagerComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (LoadState != ERpgExperienceLoadState::Loaded)
	{
		OutReason = TEXT("Experience still loading");
		return true;
	}

	return false;
}

void URpgExperienceManagerComponent::OnRep_CurrentExperience()
{
	if (CurrentExperience && LoadState == ERpgExperienceLoadState::Unloaded)
	{
		StartExperienceLoad();
	}
}

void URpgExperienceManagerComponent::StartExperienceLoad()
{
	check(CurrentExperience);
	check(LoadState == ERpgExperienceLoadState::Unloaded);

	UE_LOG(LogRpgExperience, Log, TEXT("EXPERIENCE: StartExperienceLoad(%s)"), *CurrentExperience->GetPrimaryAssetId().ToString());

	LoadState = ERpgExperienceLoadState::Loading;

	URpgAssetManager& AssetManager = URpgAssetManager::Get();

	TSet<FPrimaryAssetId> BundleAssetList;
	TSet<FSoftObjectPath> RawAssetList;
	BundleAssetList.Add(CurrentExperience->GetPrimaryAssetId());

	for (const TObjectPtr<URpgExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet)
		{
			BundleAssetList.Add(ActionSet->GetPrimaryAssetId());
		}
	}

	TArray<FName> BundlesToLoad;
	BundlesToLoad.Add(FRpgBundles::Equipped);

	const ENetMode OwnerNetMode = GetOwner()->GetNetMode();
	const bool bLoadClient = GIsEditor || (OwnerNetMode != NM_DedicatedServer);
	const bool bLoadServer = GIsEditor || (OwnerNetMode != NM_Client);
	if (bLoadClient)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateClient);
	}
	if (bLoadServer)
	{
		BundlesToLoad.Add(UGameFeaturesSubsystemSettings::LoadStateServer);
	}

	TSharedPtr<FStreamableHandle> BundleLoadHandle = nullptr;
	if (!BundleAssetList.IsEmpty())
	{
		BundleLoadHandle = AssetManager.ChangeBundleStateForPrimaryAssets(
			BundleAssetList.Array(),
			BundlesToLoad,
			{},
			false,
			FStreamableDelegate(),
			FStreamableManager::AsyncLoadHighPriority);
	}

	TSharedPtr<FStreamableHandle> RawLoadHandle = nullptr;
	if (!RawAssetList.IsEmpty())
	{
		RawLoadHandle = AssetManager.LoadAssetList(
			RawAssetList.Array(),
			FStreamableDelegate(),
			FStreamableManager::AsyncLoadHighPriority,
			TEXT("StartExperienceLoad()"));
	}

	TSharedPtr<FStreamableHandle> Handle = nullptr;
	if (BundleLoadHandle.IsValid() && RawLoadHandle.IsValid())
	{
		Handle = AssetManager.GetStreamableManager().CreateCombinedHandle({ BundleLoadHandle, RawLoadHandle });
	}
	else
	{
		Handle = BundleLoadHandle.IsValid() ? BundleLoadHandle : RawLoadHandle;
	}

	FStreamableDelegate OnAssetsLoadedDelegate = FStreamableDelegate::CreateUObject(this, &ThisClass::OnExperienceLoadComplete);
	if (!Handle.IsValid() || Handle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnAssetsLoadedDelegate);
	}
	else
	{
		Handle->BindCompleteDelegate(OnAssetsLoadedDelegate);
		Handle->BindCancelDelegate(FStreamableDelegate::CreateLambda([OnAssetsLoadedDelegate]()
		{
			OnAssetsLoadedDelegate.ExecuteIfBound();
		}));
	}

	TSet<FPrimaryAssetId> PreloadAssetList;
	if (!PreloadAssetList.IsEmpty())
	{
		AssetManager.ChangeBundleStateForPrimaryAssets(PreloadAssetList.Array(), BundlesToLoad, {});
	}
}

void URpgExperienceManagerComponent::OnExperienceLoadComplete()
{
	check(LoadState == ERpgExperienceLoadState::Loading);
	check(CurrentExperience);

	UE_LOG(LogRpgExperience, Log, TEXT("EXPERIENCE: OnExperienceLoadComplete(%s)"), *CurrentExperience->GetPrimaryAssetId().ToString());

	GameFeaturePluginURLs.Reset();

	auto CollectGameFeaturePluginURLs = [this](const UPrimaryDataAsset* Context, const TArray<FString>& FeaturePluginList)
	{
		for (const FString& PluginName : FeaturePluginList)
		{
			FString PluginURL;
			if (UGameFeaturesSubsystem::Get().GetPluginURLByName(PluginName, PluginURL))
			{
				GameFeaturePluginURLs.AddUnique(PluginURL);
			}
			else
			{
				UE_LOG(LogRpgExperience, Error, TEXT("Failed to find GameFeature plugin URL for [%s] in [%s]."),
					*PluginName,
					*GetNameSafe(Context));
			}
		}
	};

	CollectGameFeaturePluginURLs(CurrentExperience, CurrentExperience->GameFeaturesToEnable);
	for (const TObjectPtr<URpgExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet)
		{
			CollectGameFeaturePluginURLs(ActionSet, ActionSet->GameFeaturesToEnable);
		}
	}

	NumGameFeaturePluginsLoading = GameFeaturePluginURLs.Num();
	if (NumGameFeaturePluginsLoading > 0)
	{
		LoadState = ERpgExperienceLoadState::LoadingGameFeatures;
		for (const FString& PluginURL : GameFeaturePluginURLs)
		{
			URpgExperienceManager::NotifyOfPluginActivation(PluginURL);
			UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
				PluginURL,
				FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::OnGameFeaturePluginLoadComplete));
		}
	}
	else
	{
		OnExperienceFullLoadCompleted();
	}
}

void URpgExperienceManagerComponent::OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result)
{
	--NumGameFeaturePluginsLoading;

	if (NumGameFeaturePluginsLoading == 0)
	{
		OnExperienceFullLoadCompleted();
	}
}

void URpgExperienceManagerComponent::OnExperienceFullLoadCompleted()
{
	check(LoadState != ERpgExperienceLoadState::Loaded);
	check(CurrentExperience);

	if (LoadState != ERpgExperienceLoadState::LoadingChaosTestingDelay)
	{
		const float DelaySecs = RpgConsoleVariables::GetExperienceLoadDelayDuration();
		if (DelaySecs > 0.0f)
		{
			FTimerHandle DummyHandle;
			LoadState = ERpgExperienceLoadState::LoadingChaosTestingDelay;
			GetWorld()->GetTimerManager().SetTimer(DummyHandle, this, &ThisClass::OnExperienceFullLoadCompleted, DelaySecs, false);
			return;
		}
	}

	LoadState = ERpgExperienceLoadState::ExecutingActions;

	FGameFeatureActivatingContext Context;
	if (const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld()))
	{
		Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
	}

	auto ActivateListOfActions = [&Context](const TArray<TObjectPtr<UGameFeatureAction>>& ActionList)
	{
		for (UGameFeatureAction* Action : ActionList)
		{
			if (Action)
			{
				Action->OnGameFeatureRegistering();
				Action->OnGameFeatureLoading();
				Action->OnGameFeatureActivating(Context);
			}
		}
	};

	ActivateListOfActions(CurrentExperience->Actions);
	for (const TObjectPtr<URpgExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
	{
		if (ActionSet)
		{
			ActivateListOfActions(ActionSet->Actions);
		}
	}

	LoadState = ERpgExperienceLoadState::Loaded;

	OnExperienceLoaded_HighPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_HighPriority.Clear();

	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();

	OnExperienceLoaded_LowPriority.Broadcast(CurrentExperience);
	OnExperienceLoaded_LowPriority.Clear();
}

void URpgExperienceManagerComponent::OnActionDeactivationCompleted()
{
	check(IsInGameThread());
	++NumObservedPausers;

	if (NumObservedPausers == NumExpectedPausers)
	{
		OnAllActionsDeactivated();
	}
}

void URpgExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CurrentExperience);
}

void URpgExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		if (URpgExperienceManager::RequestToDeactivatePlugin(PluginURL))
		{
			UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL);
		}
	}

	if (LoadState == ERpgExperienceLoadState::Loaded)
	{
		LoadState = ERpgExperienceLoadState::Deactivating;

		NumExpectedPausers = INDEX_NONE;
		NumObservedPausers = 0;

		FGameFeatureDeactivatingContext Context(TEXT(""), [this](FStringView)
		{
			OnActionDeactivationCompleted();
		});

		if (const FWorldContext* ExistingWorldContext = GEngine->GetWorldContextFromWorld(GetWorld()))
		{
			Context.SetRequiredWorldContextHandle(ExistingWorldContext->ContextHandle);
		}

		auto DeactivateListOfActions = [&Context](const TArray<TObjectPtr<UGameFeatureAction>>& ActionList)
		{
			for (UGameFeatureAction* Action : ActionList)
			{
				if (Action)
				{
					Action->OnGameFeatureDeactivating(Context);
					Action->OnGameFeatureUnregistering();
				}
			}
		};

		DeactivateListOfActions(CurrentExperience->Actions);
		for (const TObjectPtr<URpgExperienceActionSet>& ActionSet : CurrentExperience->ActionSets)
		{
			if (ActionSet)
			{
				DeactivateListOfActions(ActionSet->Actions);
			}
		}

		NumExpectedPausers = Context.GetNumPausers();
		if (NumExpectedPausers > 0)
		{
			UE_LOG(LogRpgExperience, Error, TEXT("Actions that have asynchronous deactivation are not fully supported yet in RPG experiences."));
		}

		if (NumExpectedPausers == NumObservedPausers)
		{
			OnAllActionsDeactivated();
		}
	}
}

void URpgExperienceManagerComponent::OnAllActionsDeactivated()
{
	LoadState = ERpgExperienceLoadState::Unloaded;
	CurrentExperience = nullptr;
}
