#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "Templates/SubclassOf.h"

#include "RpgBaseBuildableDefinition.generated.h"

class AActor;
class ARpgBaseConstructionSiteActor;
class UTexture2D;

/** Broad category used by terminal/build UI to group buildable base objects. */
UENUM(BlueprintType)
enum class ERpgBaseBuildableType : uint8
{
	/** Main base interface and upgrade point. */
	Terminal,

	/** Physical module that contributes resource capacity and optional filtered access. */
	StorageUnit,

	/** Workstation that consumes recipes and owns an output inventory. */
	CraftingStation,

	/** Utility object that does not fit the other V1 groups. */
	Utility
};

/** One material requirement for constructing a base buildable. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseBuildResourceCost
{
	GENERATED_BODY()

	/** Material item definition consumed by the construction site. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Total units required before the construction site can complete. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/**
 * Data-driven definition for a buildable base actor.
 *
 * The definition is static designer data. Runtime placement spawns a construction site, pays these
 * costs server-side, then spawns BuildActorClass and links supported components to the chosen base.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgBaseBuildableDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing buildable name shown in terminal/build UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Display")
	FText DisplayName;

	/** Short description of what this buildable adds to the base. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional icon used by build lists and construction site UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Category used by terminal/build UI grouping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building")
	ERpgBaseBuildableType BuildableType = ERpgBaseBuildableType::Utility;

	/** Final replicated actor class spawned when construction is complete. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building")
	TSubclassOf<AActor> BuildActorClass;

	/** Optional construction site class for custom visuals; falls back to ARpgBaseConstructionSiteActor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building")
	TSubclassOf<ARpgBaseConstructionSiteActor> ConstructionSiteActorClass;

	/** Material costs required to finish this buildable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building")
	TArray<FRpgBaseBuildResourceCost> BuildCosts;

	/** Base upgrade/unlock tags required before this buildable can be placed. Empty means unlocked by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building", meta = (Categories = "Base"))
	FGameplayTagContainer RequiredUnlockTags;

	/** Maximum distance from the base camp origin where this buildable may be placed. Zero uses the base camp default radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float MaxPlacementDistanceFromBase = 0.0f;

	/** Maximum distance from the requesting player to the requested placement transform. Zero disables this check. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float MaxPlacementDistanceFromBuilder = 800.0f;
};
