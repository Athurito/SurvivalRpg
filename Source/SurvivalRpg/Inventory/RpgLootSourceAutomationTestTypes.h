#pragma once

#include "RpgLootSourceComponent.h"

#include "RpgLootSourceAutomationTestTypes.generated.h"

class URpgLootTable;

/** Test-only component exposing deterministic transient table setup without weakening the production API. */
UCLASS(NotBlueprintable, Transient)
class URpgLootSourceAutomationTestComponent final : public URpgLootSourceComponent
{
	GENERATED_BODY()

public:
	void ConfigureLootTable(
		URpgLootTable* InLootTable,
		bool bInUnlockContainerOnDeath = false,
		int32 InSourceLevel = 1)
	{
		LootTable = InLootTable;
		LootTemplates.Reset();
		LootInstances.Reset();
		SourceLevel = FMath::Max(1, InSourceLevel);
		bUnlockContainerOnDeath = bInUnlockContainerOnDeath;
	}
};
