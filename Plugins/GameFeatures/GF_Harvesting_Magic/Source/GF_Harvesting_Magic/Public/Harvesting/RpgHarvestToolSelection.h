#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

/** Immutable result of deterministic server-side harvesting-tool selection. */
struct GF_HARVESTING_MAGIC_API FRpgSelectedHarvestTool
{
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;
	FRpgInventoryItemId ItemId;
	float HarvestPower = 0.0f;

	bool IsValid() const
	{
		return ItemInstance != nullptr && ItemId.IsValid() && FMath::IsFinite(HarvestPower) && HarvestPower > 0.0f;
	}
};

/** Deterministic read-only helpers for selecting and revalidating inventory-owned harvesting tools. */
class GF_HARVESTING_MAGIC_API FRpgHarvestToolSelection
{
public:
	/** Picks highest power, then the lexicographically smallest persistent item id. */
	static FRpgSelectedHarvestTool FindBestOwnedTool(
		const URpgInventoryManagerComponent* Inventory,
		FGameplayTag RequiredToolTag);

	/** Resolves one exact still-owned tool and verifies its category and power. */
	static FRpgSelectedHarvestTool FindOwnedToolById(
		const URpgInventoryManagerComponent* Inventory,
		FRpgInventoryItemId ItemId,
		FGameplayTag RequiredToolTag);
};
