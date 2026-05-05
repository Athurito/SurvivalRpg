#pragma once

#include "CoreMinimal.h"
#include "RpgEquipmentBase.h"
#include "RpgWeaponBase.generated.h"

UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgWeaponBase : public ARpgEquipmentBase
{
	GENERATED_BODY()

public:
	ARpgWeaponBase();
};
