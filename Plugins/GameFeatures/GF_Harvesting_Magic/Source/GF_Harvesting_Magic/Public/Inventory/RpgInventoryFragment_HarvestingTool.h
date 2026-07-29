#pragma once

#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_HarvestingTool.generated.h"

/** Static item-definition data that makes a concrete inventory item usable as a harvesting tool. */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class GF_HARVESTING_MAGIC_API URpgInventoryFragment_HarvestingTool final
	: public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Tool category supplied by this item, such as Tool.Harvesting.Skinning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Tool", meta = (Categories = "Tool.Harvesting"))
	FGameplayTag ToolTag;

	/** Positive relative power applied to yield rolls; one is the baseline tool strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Tool", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float HarvestPower = 1.0f;

	/** Returns whether this fragment supplies RequiredToolTag and has usable finite power. */
	bool SupportsToolTag(FGameplayTag RequiredToolTag) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
