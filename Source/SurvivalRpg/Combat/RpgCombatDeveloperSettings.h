#pragma once

#include "Engine/DeveloperSettings.h"

#include "RpgCombatDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Combat Debug"))
class SURVIVALRPG_API URpgCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URpgCombatDeveloperSettings();

	/**
	 * Emits correlated client/server attack lifecycle records for activation, montage timing,
	 * authority windows, traces, cancellation, and damage. Debug-only logging; never gameplay-authoritative.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Weapon Trace", meta = (DisplayName = "Log Weapon Attack Lifecycle"))
	bool bLogWeaponAttackLifecycle = false;

	/** Draws authority weapon traces in the world for local combat debugging; cosmetic-only and not replicated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Weapon Trace", meta = (DisplayName = "Draw Weapon Attack Traces"))
	bool bDrawWeaponAttackTraces = false;

	/** Lifetime in seconds for cosmetic weapon-trace debug shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Weapon Trace", meta = (DisplayName = "Display Duration", EditCondition = "bDrawWeaponAttackTraces", ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float WeaponAttackTraceDebugDuration = 1.5f;
};
