#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgBaseStorageSaveTypes.generated.h"

class URpgBaseStorageUpgradeDefinition;
class URpgInventoryItemDefinition;

/** Pointer-free persisted count for one definition stored in the shared material domain. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageBulkSaveEntry
{
	GENERATED_BODY()

	/** Soft item-definition class used to reconstruct the bulk row without retaining a runtime UObject. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;

	/** Authoritative stored unit count. Capacity is derived from the current domain-anchor configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 0;

	/** Legacy shared ordering retained for migration; current terminal presentation sorts locally per client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	int32 SortIndex = 0;
};

/** Persisted identity of one upgrade package installed on a fixed base-domain anchor. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageInstalledUpgradeSaveData
{
	GENERATED_BODY()

	/** Primary-asset identity resolved through AssetManager during restore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FPrimaryAssetId UpgradeId;

	/** Stable designer-authored anchor id targeted by this package; multiple distinct packages may share it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FName AnchorId = NAME_None;
};

/** Legacy-compatible explicit mirror of containment state associated with one concrete inventory item identity. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseContainmentItemStateSaveData
{
	GENERATED_BODY()

	/** Persistent identity of the concrete item stored in the containment inventory graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FRpgInventoryItemId ItemId;

	/** Server-authoritative stabilization mirror; the graph runtime payload on the same item is canonical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	bool bStabilized = false;
};

/** Complete host-owned reconstruction payload for one persistent base storage network. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageSaveData
{
	GENERATED_BODY()

	/** Stable map-authored id of the base actor represented by this payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FName BaseId = NAME_None;

	/** Stable host profile key allowed to perform owner-only base operations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FString OwnerProfileKey;

	/** Definition/count rows in the shared Materials domain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	TArray<FRpgBaseStorageBulkSaveEntry> BulkEntries;

	/** Installed upgrade packages; distinct tiers may target the same fixed domain anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	TArray<FRpgBaseStorageInstalledUpgradeSaveData> InstalledUpgrades;

	/** Derived capability snapshot used for validation and forward-compatible reconstruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FGameplayTagContainer InstalledCapabilities;

	/** True when ArmoryGraph was captured successfully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	bool bHasArmoryGraph = false;

	/** Concrete item graph for the shared Armory domain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FRpgInventoryGraphSaveData ArmoryGraph;

	/** True when ContainmentGraph was captured successfully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	bool bHasContainmentGraph = false;

	/** Concrete item graph for the shared Rift Containment domain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	FRpgInventoryGraphSaveData ContainmentGraph;

	/** Compatibility mirror keyed by item identity; canonical state also travels in each graph item's runtime payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	TArray<FRpgBaseContainmentItemStateSaveData> ContainmentStates;

	/** Shared deterministic extraction strain in the inclusive range 0..100. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
	int32 RiftStrain = 0;

	/** Owner-private locker graphs keyed by stable host player-profile id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save|Base Storage")
	TMap<FString, FRpgInventoryGraphSaveData> PersonalLockerGraphs;
};
