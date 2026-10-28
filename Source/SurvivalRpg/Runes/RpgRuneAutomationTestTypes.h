#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgRuneAutomationTestTypes.generated.h"

class URpgAbilitySet;

/** Socket grid id of the automation tool. */
inline const FName RpgRuneAutomationTestSocketGridId(TEXT("Runes"));

/** Editor-only 1x1 tool whose item-owned "Runes" grid holds three 1x1 rune sockets. */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgRuneAutomationTestToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgRuneAutomationTestToolDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 1x1 MainHand weapon with the same "Runes" socket grid, for tests that carry it in a carry slot. */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgRuneAutomationTestSocketedWeaponDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgRuneAutomationTestSocketedWeaponDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 1x1 bag with a 2x2 storage grid that refuses nested containers. */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgRuneAutomationTestBagDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgRuneAutomationTestBagDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/**
 * Editor-only rune A with three ranks. Each rank grants Rune.AutomationTest.Rank.<I|II|III>, and the rune grants
 * Rune.AutomationTest.A, once ConfigureRuneAutomationTestDefinitions ran.
 */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgRuneAutomationTestRuneDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgRuneAutomationTestRuneDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }

	/** Points rank Rank's AbilitySet of the class default fragment at AbilitySet; tests restore null when they finish. */
	static void SetRankAbilitySet(int32 Rank, const URpgAbilitySet* AbilitySet);
};

/** Editor-only rune B with one rank, which grants Rune.AutomationTest.B once configured. */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgRuneAutomationTestOtherRuneDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgRuneAutomationTestOtherRuneDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/**
 * Sets the gameplay tags of the rune automation definitions' class defaults. Tags are not available while class
 * defaults are constructed, so every test calls this first; it is idempotent.
 */
SURVIVALRPG_API void ConfigureRuneAutomationTestDefinitions();
