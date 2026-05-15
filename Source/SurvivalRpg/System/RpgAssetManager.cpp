// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgAssetManager.h"

#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopedSlowTask.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/SurvivalRpg.h"

const FName FRpgBundles::Equipped("Equipped");

static FAutoConsoleCommand CVarDumpLoadedAssets(
	TEXT("Rpg.DumpLoadedAssets"),
	TEXT("Shows all assets that were loaded via the RPG asset manager and are currently in memory."),
	FConsoleCommandDelegate::CreateStatic(URpgAssetManager::DumpLoadedAssets));

URpgAssetManager::URpgAssetManager()
{
	DefaultPawnData = nullptr;
}

URpgAssetManager& URpgAssetManager::Get()
{
	check(GEngine);

	if (URpgAssetManager* Singleton = Cast<URpgAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogRpg, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini. It must be set to RpgAssetManager."));
	return *NewObject<URpgAssetManager>();
}

UObject* URpgAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		if (ShouldLogAssetLoads())
		{
			UE_LOG(LogRpg, Log, TEXT("Synchronously loading asset [%s]."), *AssetPath.ToString());
		}

		if (UAssetManager::IsInitialized())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath, false);
		}

		return AssetPath.TryLoad();
	}

	return nullptr;
}

bool URpgAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

void URpgAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock LoadedAssetsLock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}

void URpgAssetManager::DumpLoadedAssets()
{
	UE_LOG(LogRpg, Log, TEXT("========== Start Dumping Loaded Assets =========="));

	for (const UObject* LoadedAsset : Get().LoadedAssets)
	{
		UE_LOG(LogRpg, Log, TEXT("  %s"), *GetNameSafe(LoadedAsset));
	}

	UE_LOG(LogRpg, Log, TEXT("... %d assets in loaded pool"), Get().LoadedAssets.Num());
	UE_LOG(LogRpg, Log, TEXT("========== Finish Dumping Loaded Assets =========="));
}

void URpgAssetManager::StartInitialLoading()
{
	SCOPED_BOOT_TIMING("URpgAssetManager::StartInitialLoading");

	Super::StartInitialLoading();
	InitializeGameplayCueManager();
}

void URpgAssetManager::InitializeGameplayCueManager()
{
	SCOPED_BOOT_TIMING("URpgAssetManager::InitializeGameplayCueManager");

	UAbilitySystemGlobals::Get().GetGameplayCueManager();
}

const URpgPawnData* URpgAssetManager::GetDefaultPawnData() const
{
	return GetAsset(DefaultPawnData);
}

#if WITH_EDITOR
void URpgAssetManager::PreBeginPIE(bool bStartSimulate)
{
	Super::PreBeginPIE(bStartSimulate);

	FScopedSlowTask SlowTask(0, NSLOCTEXT("RpgEditor", "BeginLoadingPIEData", "Loading PIE Data"));
	const bool bShowCancelButton = false;
	const bool bAllowInPIE = true;
	SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);

	GetDefaultPawnData();
}
#endif
