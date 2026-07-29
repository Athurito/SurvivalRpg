#pragma once

#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgHarvestAutomationTestTypes.generated.h"

class URpgCorpseLifecycleComponent;
class URpgHarvestableCorpseComponent;

/** Drop fixture that deliberately materializes only part of a payload before reporting failure. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationPartialFailureDropActor final : public ARpgDroppedInventoryActor
{
	GENERATED_BODY()

public:
	virtual bool TrySetPickupInventory(
		const FInventoryPickup& NewPickupInventory) override;
};

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

/** Low-power skinning tool used to verify deterministic best-tool selection. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestLowToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestLowToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** High-power skinning tool used to verify power wins before item identity. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestHighToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestHighToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** Equal-power skinning tool used to verify stable item-id tie-breaking. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestTieToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestTieToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** Asset-free authoritative corpse fixture with the real lifecycle and harvest components. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationCorpseActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationCorpseActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgCorpseLifecycleComponent> CorpseLifecycle;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestableCorpseComponent> HarvestableCorpse;
};
