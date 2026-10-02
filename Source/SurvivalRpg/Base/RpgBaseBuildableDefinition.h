#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
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

/** One authored physical chest capacity tier. Tier zero defines the initial grid; later tiers consume their Costs. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseChestUpgradeTier
{
	GENERATED_BODY()

	/** Root grid in cells. Successive tiers may expand width/height without moving existing items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Chest")
	FRpgInventoryGridSize GridSize;

	/** Ordinary materials paid from the acting player first, then eligible shared sources, when entering this tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Chest")
	TArray<FRpgBaseBuildResourceCost> Costs;
};

/**
 * Data-driven definition for a buildable base actor.
 *
 * Static designer data for server-authoritative placement and costs. Physical chests commit payment
 * and their fully initialized actor together. Other buildables may use a construction site.
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

	/** Optional physical chest capacity progression. Tier zero is the starting grid; costs belong to the destination tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Chest")
	TArray<FRpgBaseChestUpgradeTier> ChestUpgradeTiers;

	/** Base upgrade/unlock tags required before this buildable can be placed. Empty means unlocked by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building", meta = (Categories = "Base"))
	FGameplayTagContainer RequiredUnlockTags;

	/** Optional stricter horizontal placement radius in centimeters; never enlarges the owning base area. Zero uses the base radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float MaxPlacementDistanceFromBase = 0.0f;

	/** Maximum distance from the requesting player to the requested placement transform. Zero disables this check. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float MaxPlacementDistanceFromBuilder = 800.0f;

	/** Authored half-size in centimeters used by the authority and cosmetic preview to test the occupied box. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "1", Units = "cm"))
	FVector PlacementHalfExtent = FVector(60.0f, 60.0f, 60.0f);

	/** Maximum supported ground slope in degrees; evaluated against the placement trace normal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
	float MaxGroundSlopeDegrees = 20.0f;

	/** Vertical distance in centimeters available to find support below the placement preview. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "1", Units = "cm"))
	float GroundTraceDistance = 200.0f;

	/** Small gap in centimeters above the traced support plane to avoid initial contact penetration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Building|Placement", meta = (ClampMin = "0", Units = "cm"))
	float GroundClearance = 2.0f;
};
