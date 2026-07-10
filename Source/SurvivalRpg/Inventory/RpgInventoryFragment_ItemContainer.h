#pragma once

#include "GameplayTagContainer.h"
#include "RpgInventoryGraphTypes.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryFragment_ItemContainer.generated.h"

class UTexture2D;

/**
 * Static definition of one grid physically owned by a concrete item instance.
 *
 * The owning item's persistent id becomes part of the runtime container handle, so filled backpacks, belts, and
 * pouches retain their contents when moved, equipped, dropped, saved, or transferred to another inventory.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryItemContainerDefinition
{
	GENERATED_BODY()

	/** Definition-local grid id. It only needs to be unique among the containers provided by this item definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container")
	FName ContainerId = NAME_None;

	/** Player-facing label used by inventory detail panels and breadcrumbs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container")
	FText DisplayName;

	/** Optional client presentation icon. Gameplay validation never depends on this asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Spatial dimensions in grid cells. Normal capacity is determined only by this grid and item stack limits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container", meta = (ClampMin = "1", UIMin = "1"))
	FRpgInventoryGridSize GridSize;

	/** Broad item categories accepted by this grid. Empty accepts every category unless tag rules reject it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Rules")
	TArray<ERpgInventoryItemCategory> AllowedCategories;

	/** ItemTraits tags that an inserted item must contain. Static definition data evaluated by the server planner. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Rules")
	FGameplayTagContainer RequiredItemTags;

	/** ItemTraits tags that reject an inserted item when any are present. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Rules")
	FGameplayTagContainer BlockedItemTags;

	/** Whether items that themselves provide containers may be inserted into this grid. Cycle checks still always apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Nesting")
	bool bAllowNestedContainers = false;

	/** Deepest item-owned level permitted below a root through this grid; hard-clamped to the global maximum of four. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Nesting", meta = (EditCondition = "bAllowNestedContainers", ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4"))
	uint8 MaxNestingDepth = RpgInventoryMaxItemOwnedDepth;

	/** Whether consumable quick-access bindings may resolve stacks directly inside this grid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container|Quick Access")
	bool bQuickAccessEligible = false;

	/** Returns whether the static definition is usable by the runtime graph. */
	bool IsValid() const
	{
		return !ContainerId.IsNone() && GridSize.IsValid();
	}

	/** Returns the effective designer-configured nesting limit, always within the global range. */
	uint8 GetEffectiveMaxNestingDepth() const
	{
		return FMath::Clamp<uint8>(MaxNestingDepth, 1, RpgInventoryMaxItemOwnedDepth);
	}

	/** Evaluates category/tag/nested-container rules for an item definition. Server placement still validates occupancy. */
	bool AllowsItemDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, uint8 TargetDepth) const;
};

/**
 * Item fragment that contributes one or more item-owned spatial containers to every concrete item instance.
 *
 * The fragment contains only immutable designer data. URpgInventoryManagerComponent remains the sole owner and
 * mutation authority for entries placed in these containers.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_ItemContainer : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Item-owned grids provided by this definition, such as a backpack main compartment or pouch pocket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Container", meta = (TitleProperty = "ContainerId"))
	TArray<FRpgInventoryItemContainerDefinition> ProvidedContainers;

	/** Appends effective item-owned definitions. Compatibility fragments override this to migrate legacy layout data. */
	virtual void GetProvidedContainers(TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const;

	/** Finds one native container definition by its definition-local id. */
	const FRpgInventoryItemContainerDefinition* FindProvidedContainer(FName ContainerId) const;
};
