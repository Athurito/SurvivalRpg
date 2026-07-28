#pragma once

#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgHarvestAutomationTestTypes.generated.h"

/** Player-state fixture that skips Experience wiring while retaining real inventory and trade-skill components. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationTestPlayerState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Stackable 1x1 material used by asset-free harvest reward tests. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestStackItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestStackItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};
/** Distinct stackable material used to prove multi-row overflow batches remain complete. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestSecondMaterialDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestSecondMaterialDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};
