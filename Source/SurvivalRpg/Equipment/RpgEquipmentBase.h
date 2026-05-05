#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgEquipmentBase.generated.h"

UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgEquipmentBase : public AActor
{
	GENERATED_BODY()

public:
	ARpgEquipmentBase();
};
