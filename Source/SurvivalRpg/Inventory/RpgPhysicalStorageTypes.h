#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgInventoryGraphTypes.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgPhysicalStorageTypes.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/** One designer/player-authored automatic destination rule; multiple slots are alternatives. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStorageAssignment
{
	GENERATED_BODY()

	/** Exact item definition; mutually exclusive with Category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Storage")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Hierarchical ItemTraits tag; selects this category and all descendants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Storage", meta = (Categories = "Item"))
	FGameplayTag Category;

	/** Server-assigned global creation order, saved across sessions; zero means an unauthored slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	int64 AssignmentOrder = 0;

	bool IsValid() const { return (ItemDefinition != nullptr) != Category.IsValid(); }
};

/** Saved and replicated physical chest settings. The chest's inventory owns the actual item graph. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPhysicalStorageMetadata
{
	GENERATED_BODY()

	/** Stable world-save key, independent of actor name and current position. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	FName PersistentContainerId;

	/** Server-resolved spatial base membership; None identifies an outside chest. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	FName BaseId;

	/** Current zero-based authored capacity tier. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	int32 UpgradeTier = 0;

	/** Root dimensions restored before the item graph. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	FRpgInventoryGridSize GridSize;

	/** Persistent automatic destination slots; they reserve no inventory cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	TArray<FRpgStorageAssignment> Assignments;

	/** Last issued order known by this chest, including deleted rules. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	int64 AssignmentOrderHighWaterMark = 0;

	/** Server revision used to reject stale setting changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	int32 SettingsRevision = 0;

	/** True for player-built actors which must be reconstructed from the world save. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Storage")
	bool bRuntimeBuilt = false;
};

/** Server-local ordinary resource operation. Authority/access validation belongs to the caller. */
struct SURVIVALRPG_API FRpgInventoryBatchOperation
{
	/** Null for a newly authored definition grant. */
	URpgInventoryManagerComponent* SourceInventory = nullptr;
	/** Null for consumption; otherwise receives a compatible stack or preserved runtime instance. */
	URpgInventoryManagerComponent* TargetInventory = nullptr;
	/** Exact source identity, required when SourceInventory is set. Providers are deliberately excluded. */
	FRpgInventoryItemId ItemId;
	/** Required only for a definition grant; source operations resolve their definition from ItemId. */
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
	/** Positive exact quantity. Partial acceptance is planned by the caller as a smaller operation. */
	int32 Quantity = 0;
	/** Optional expected revisions; INDEX_NONE asks the kernel to capture current authority state. */
	int32 ExpectedSourceRevision = INDEX_NONE;
	int32 ExpectedTargetRevision = INDEX_NONE;
};

/** Prevalidated capacity expansion participating in an ordinary resource batch. */
struct SURVIVALRPG_API FRpgInventoryBatchCapacityChange
{
	URpgInventoryManagerComponent* Inventory = nullptr;
	FRpgInventoryGridSize NewGridSize;
	int32 ExpectedRevision = INDEX_NONE;
};
