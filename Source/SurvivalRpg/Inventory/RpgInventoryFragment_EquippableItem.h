#pragma once

#include "CoreMinimal.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryFragment_EquippableItem.generated.h"

class URpgEquipmentDefinition;

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryFragment_EquippableItem : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	TSubclassOf<URpgEquipmentDefinition> GetEquipmentDefinition() const { return EquipmentDefinition; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition;
};
