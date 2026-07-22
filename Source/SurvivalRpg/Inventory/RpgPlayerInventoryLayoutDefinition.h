#pragma once

#include "Engine/DataAsset.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgPlayerInventoryLayoutDefinition.generated.h"

/**
 * Immutable designer-authored root layout for a player inventory.
 *
 * PawnData owns this definition. Runtime item-provided containers are projected separately by the
 * controller-owned layout component and never mutate this asset.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPlayerInventoryLayoutDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Static gear, carry, and content roots in stable presentation order.
	 * This is cooked definition data: designers author it in the asset and runtime systems treat it as read-only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (TitleProperty = "ContainerId"))
	TArray<FRpgInventorySlotGroupDefinition> StaticSlotGroups;
};
