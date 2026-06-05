#pragma once

#include "Engine/DeveloperSettings.h"

#include "RpgCombatDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Combat Debug"))
class SURVIVALRPG_API URpgCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URpgCombatDeveloperSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Weapon Trace", meta = (DisplayName = "Draw Weapon Attack Traces"))
	bool bDrawWeaponAttackTraces = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Weapon Trace", meta = (DisplayName = "Display Duration", EditCondition = "bDrawWeaponAttackTraces", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float WeaponAttackTraceDebugDuration = 1.5f;
};
