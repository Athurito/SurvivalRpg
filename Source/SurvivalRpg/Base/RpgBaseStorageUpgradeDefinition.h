#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgBaseStorageComponent.h"

#include "RpgBaseStorageUpgradeDefinition.generated.h"

class UTexture2D;
class URpgInventoryItemDefinition;
class FDataValidationContext;

/** Explicit material amount used by installation costs or authored decommission refunds. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeCost
{
	GENERATED_BODY()

	/** Material item definition consumed during install or granted by an approved decommission transaction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Positive number of units consumed or refunded according to the owning array's operation. */
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

/** Domain-wide capacity points contributed while an upgrade is installed. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeCapacityEffect
{
	GENERATED_BODY()

	/** Additional shared Materials capacity points; valid only when the upgrade targets Storage.Domain.Materials. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0", UIMin = "0"))
	int32 AdditionalCapacity = 0;

	bool IsNeutral() const { return AdditionalCapacity == 0; }
};

/** Spatial expansion contributed to an instance-preserving domain grid. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeGridEffect
{
	GENERATED_BODY()

	/** Armory grid columns added while installed. Valid only for Storage.Domain.Armory; zero leaves width unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0", UIMin = "0"))
	int32 AdditionalColumns = 0;

	/** Armory grid rows added while installed. Valid only for Storage.Domain.Armory; zero leaves height unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0", UIMin = "0"))
	int32 AdditionalRows = 0;

	bool IsNeutral() const
	{
		return AdditionalColumns == 0 && AdditionalRows == 0;
	}
};

/** Sealed-slot and protection contribution for a containment domain or anchor. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeContainmentEffect
{
	GENERATED_BODY()

	/** Additional concrete contained-item slots; valid only for Storage.Domain.RiftContainment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0", UIMin = "0"))
	int32 AdditionalSealedSlots = 0;

	/** Rift containment strength added for special-item placement; valid only for RiftContainment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ContainmentStrengthDelta = 0.0f;

	/** Rift corruption shielding added for quarantine placement; valid only for RiftContainment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CorruptionProtectionDelta = 0.0f;

	bool IsNeutral() const
	{
		return AdditionalSealedSlots == 0 &&
			ContainmentStrengthDelta == 0.0f &&
			CorruptionProtectionDelta == 0.0f;
	}
};

/** Risk/tolerance contribution used by magical compression, protection, and Rift-storage upgrades. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeStrainEffect
{
	GENERATED_BODY()

	/** Passive Rift strain added while installed; valid only for Storage.Domain.RiftContainment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AddedStrain = 0.0f;

	/** Additional safe Rift strain tolerance supplied by a RiftContainment upgrade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StrainToleranceDelta = 0.0f;

	/** Non-negative mitigation subtracted from generated Rift strain by the authoritative coordinator. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StrainMitigation = 0.0f;

	bool IsNeutral() const
	{
		return AddedStrain == 0.0f &&
			StrainToleranceDelta == 0.0f &&
			StrainMitigation == 0.0f;
	}
};

/**
 * Data-driven upgrade that can be installed on a base storage station.
 *
 * Upgrades are static designer-authored assets. Legacy station installation still consumes Costs, applies
 * CapacityBonuses, and exposes GrantedUpgradeTags. The local storage-network coordinator owns evaluation and
 * application of domain targeting, knowledge/capability prerequisites, new effects, and decommission refunds.
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

	/** Required logical domain affected by this installed package. Must be a strict child of Storage.Domain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Target", meta = (Categories = "Storage.Domain"))
	FGameplayTag TargetDomainTag;

	/** Required stable fixed anchor within TargetDomainTag. Multiple distinct packages may target the same anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Target")
	FName TargetAnchorId = NAME_None;

	/** Progression knowledge the base must possess before the authoritative installer may apply this upgrade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Requirements", meta = (Categories = "Storage.Knowledge"))
	FGameplayTagContainer RequiredKnowledgeTags;

	/** Functional storage capabilities that must already be installed in the local network. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Requirements", meta = (Categories = "Storage.Capability"))
	FGameplayTagContainer RequiredInstalledCapabilityTags;

	/** Capacity added to the linked base storage while this upgrade is installed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade")
	TArray<FRpgBaseResourceCapacity> CapacityBonuses;

	/** Domain-wide abstract capacity contribution consumed by the storage-network coordinator. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects")
	FRpgBaseStorageUpgradeCapacityEffect CapacityEffect;

	/** Spatial grid expansion for the targeted instance-preserving storage domain or anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects")
	FRpgBaseStorageUpgradeGridEffect GridEffect;

	/** Sealed-slot, containment-strength, and corruption-protection contribution for the target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects")
	FRpgBaseStorageUpgradeContainmentEffect ContainmentEffect;

	/** Passive risk, safe tolerance, and mitigation contribution for the target storage domain or anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects")
	FRpgBaseStorageUpgradeStrainEffect StrainEffect;

	/** Functional capabilities granted while this upgrade remains installed in the local network. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Effects", meta = (Categories = "Storage.Capability"))
	FGameplayTagContainer GrantedCapabilityTags;

	/** Upgrade tags granted to the station, such as auto-sort, remote access, or crafting output auto-deposit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade", meta = (Categories = "Base.Storage.Upgrade"))
	FGameplayTagContainer GrantedUpgradeTags;

	/**
	 * Exact resources refunded by an approved server-side decommission transaction.
	 * Empty intentionally means no refund; values are never inferred from installation Costs.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Upgrade|Decommission")
	TArray<FRpgBaseStorageUpgradeCost> DecommissionRefunds;

	/** Returns whether any new domain/anchor effect requires network-coordinator application. */
	bool HasNetworkEffects() const
	{
		return !CapacityEffect.IsNeutral() ||
			!GridEffect.IsNeutral() ||
			!ContainmentEffect.IsNeutral() ||
			!StrainEffect.IsNeutral();
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
