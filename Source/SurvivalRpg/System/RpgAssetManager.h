// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/AssetManager.h"
#include "Templates/SubclassOf.h"
#include "RpgAssetManager.generated.h"

class URpgPawnData;

/** Asset bundle names used by gameplay systems and GameFeature loading. */
struct FRpgBundles
{
	static const FName Equipped;
};

/**
 * Game-specific AssetManager.
 *
 * This is the central seam for project asset loading rules, default PawnData fallback,
 * gameplay cue setup, and keeping synchronously loaded soft references resident.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	URpgAssetManager();

	/** Returns the game asset manager singleton. */
	static URpgAssetManager& Get();

	/** Returns an asset referenced by a soft object pointer, synchronously loading it if needed. */
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** Returns a class referenced by a soft class pointer, synchronously loading it if needed. */
	template<typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	/** Logs all assets currently kept alive by this asset manager. */
	static void DumpLoadedAssets();

	/** PawnData fallback used if an experience or PlayerState does not provide one. */
	const URpgPawnData* GetDefaultPawnData() const;

protected:
	static UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);
	static bool ShouldLogAssetLoads();

	/** Thread-safe way to keep a loaded asset resident for the lifetime of the manager. */
	void AddLoadedAsset(const UObject* Asset);

	//~ UAssetManager interface
	virtual void StartInitialLoading() override;
#if WITH_EDITOR
	virtual void PreBeginPIE(bool bStartSimulate) override;
#endif
	//~ End UAssetManager interface

private:
	/** Sets up ability-system related asset loading hooks. */
	void InitializeGameplayCueManager();

	/** Configured default PawnData fallback. */
	UPROPERTY(Config)
	TSoftObjectPtr<URpgPawnData> DefaultPawnData;

	/** Assets loaded and tracked by this manager so they stay in memory. */
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	/** Protects LoadedAssets during async or multi-threaded asset operations. */
	FCriticalSection LoadedAssetsCritical;
};

template<typename AssetType>
AssetType* URpgAssetManager::GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedAsset = AssetPointer.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset, TEXT("Failed to load asset [%s]"), *AssetPointer.ToString());
		}

		if (LoadedAsset && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}

	return LoadedAsset;
}

template<typename AssetType>
TSubclassOf<AssetType> URpgAssetManager::GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		LoadedSubclass = AssetPointer.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass, TEXT("Failed to load asset class [%s]"), *AssetPointer.ToString());
		}

		if (LoadedSubclass && bKeepInMemory)
		{
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}
