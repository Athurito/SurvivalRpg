#pragma once

#include "Components/ActorComponent.h"

#include "RpgCraftingStationComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/** One resource requirement consumed by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingResourceCost
{
	GENERATED_BODY()

	/** Item definition required by the recipe. Static recipe data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items to consume across player inventory and linked storage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/**
 * V1 crafting station helper that gathers resource inventories from the player and nearby shared storage.
 */
UCLASS(Blueprintable, ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCraftingStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns player inventory plus crafting-accessible containers in range or in the same storage group. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	TArray<URpgInventoryManagerComponent*> GetResourceInventories(AActor* RequestingActor) const;

	/** Returns total available count across all resource inventories for one item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	int32 GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Consumes resources across player inventory and nearby/same-group storage after verifying the full cost is available. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems);

protected:
	/** Shared storage group this station belongs to. Empty means radius-only shared-container lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName StorageGroupId;

	/** Radius in centimeters for including nearby shared containers as crafting resource sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float StorageSearchRadius = 1200.0f;
};
