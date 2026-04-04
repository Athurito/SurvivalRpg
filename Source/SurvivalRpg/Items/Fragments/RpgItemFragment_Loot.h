#pragma once

#include "CoreMinimal.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Loot.generated.h"

class ARpgItemPickup;

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemFragment_Loot : public URpgItemFragment
{
	GENERATED_BODY()

public:
	TSubclassOf<ARpgItemPickup> GetPickupActorClass() const { return PickupActorClass; }
	const FText& GetInteractionText() const { return InteractionText; }
	float GetDefaultDropWeight() const { return DefaultDropWeight; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ToolTip = "Pickup actor class spawned when this item drops into the world. Leave empty to use the default RPG item pickup actor."))
	TSubclassOf<ARpgItemPickup> PickupActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ToolTip = "Interaction text shown on the pickup in the world, for example Equip, Take, or Loot."))
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "Fallback weight used by loot systems when no explicit table weight overrides this item. Higher values mean the item is rolled more often."))
	float DefaultDropWeight = 1.0f;
};
