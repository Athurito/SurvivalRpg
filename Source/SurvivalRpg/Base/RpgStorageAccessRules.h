#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class ARpgBaseCampActor;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;
class URpgInventoryItemDefinition;

/** Shared spatial rules for physical storage. These queries never grant direct player interaction access. */
namespace RpgStorageAccessRules
{
	/** Horizontal base membership, including the boundary; radius must be finite and positive. */
	SURVIVALRPG_API bool IsInsideBaseArea(const FVector& Center, float Radius, const FVector& Location);

	/** Touching or overlapping horizontal base discs conflict, independently of their height. */
	SURVIVALRPG_API bool BaseAreasConflict(const FVector& CenterA, float RadiusA, const FVector& CenterB, float RadiusB);

	/** Returns the unique valid spatial base; malformed overlapping areas fail closed. */
	SURVIVALRPG_API ARpgBaseCampActor* ResolveBaseAtLocation(const UWorld* World, const FVector& Location);

	/** Rejects non-positive/non-finite radii and any overlap with another live base. */
	SURVIVALRPG_API bool CanPlaceBaseArea(const UWorld* World, const FVector& Center, float Radius, const ARpgBaseCampActor* IgnoredBase = nullptr);

	/** Physical, accessible shared chests and output trays; never player, corpse, dropped-loot or private inventories. */
	SURVIVALRPG_API void ResolveStorageSources(const UWorld* World, const FVector& ContextLocation, float OutsideSearchRadius,
		TArray<URpgInventoryManagerComponent*>& OutSources);

	/** Ranked destinations: exact item, category, existing stock; quantity descending, then persistent assignment order. */
	SURVIVALRPG_API TArray<URpgInventoryContainerComponent*> GetPhysicalStorageTargets(const UWorld* World,
		const FVector& ContextLocation, float OutsideSearchRadius, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);

	/** Inventory projection of GetPhysicalStorageTargets for batch transfer/output planning. */
	SURVIVALRPG_API void ResolveDepositTargets(const UWorld* World, const FVector& ContextLocation, float OutsideSearchRadius,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, TArray<URpgInventoryManagerComponent*>& OutTargets);

	/** Stable source key used by persisted refund claims. Player keys require the authoritative GameMode. */
	SURVIVALRPG_API FName GetPersistentInventoryId(const URpgInventoryManagerComponent* Inventory);

	/** Resolves a saved source key only to its original owner; disconnected players return null. */
	SURVIVALRPG_API URpgInventoryManagerComponent* FindPersistentInventory(const UWorld* World, FName InventoryId);

	/** Issues a globally monotonic order from saved base/chest high-water marks; zero means authority/overflow failure. */
	SURVIVALRPG_API int64 AllocateAssignmentOrder(UWorld* World);
}
