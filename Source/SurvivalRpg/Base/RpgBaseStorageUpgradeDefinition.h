#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgBaseStorageComponent.h"

#include "RpgBaseStorageUpgradeDefinition.generated.h"

class UTexture2D;
class URpgInventoryItemDefinition;

/** Material cost paid when installing a base storage upgrade. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeCost
{
	GENERATED_BODY()

	/** Material item definition consumed by the upgrade install action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of units required from the player inventory and/or linked base storage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** Resource source order used when a player installs an upgrade at a base storage station. */
UENUM(BlueprintType)
enum class ERpgBaseStorageUpgradeCostConsumeOrder : uint8
{
	/** Consume from the linked base storage first, then the player inventory. */
	BaseThenPlayer,

	/** Consume from the player inventory first, then the linked base storage. */
	PlayerThenBase,

	/** Only consume resources already stored in the linked base storage. */
	BaseOnly,

	/** Only consume resources carried in the player inventory. */
	PlayerOnly
};

/**
 * Data-driven upgrade that can be installed on a base storage station.
 *
 * Upgrades are static designer-authored assets. Installing one stores a reference on the station,
 * applies its capacity bonuses on the server, and exposes granted gameplay tags to UI/crafting.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgBaseStorageUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing upgrade name shown in terminal and storage-unit UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Display")
	FText DisplayName;

	/** Optional compact description explaining what this upgrade unlocks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Display", meta = (MultiLine = true))
	FText Description;

	/** Optional icon used by UI only. Keep heavy art as a soft reference. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Material costs consumed when the upgrade is installed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade")
	TArray<FRpgBaseStorageUpgradeCost> Costs;

	/** Station tags this upgrade may be installed on. Empty means any storage station may install it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (Categories = "Base.Storage.Station"))
	FGameplayTagContainer AllowedStationTags;

	/** Capacity added to the linked base storage while this upgrade is installed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade")
	TArray<FRpgBaseResourceCapacity> CapacityBonuses;

	/** Upgrade tags granted to the station, such as auto-sort, remote access, or crafting output auto-deposit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (Categories = "Base.Storage.Upgrade"))
	FGameplayTagContainer GrantedUpgradeTags;
};
