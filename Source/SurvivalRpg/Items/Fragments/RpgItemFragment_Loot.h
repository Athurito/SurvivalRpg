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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ARpgItemPickup> PickupActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DefaultDropWeight = 1.0f;
};
