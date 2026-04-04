// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RpgEquipmentBase.h"
#include "RpgWeaponBase.generated.h"

UCLASS(Abstract)
class SURVIVALRPG_API ARpgWeaponBase : public ARpgEquipmentBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARpgWeaponBase();
};
