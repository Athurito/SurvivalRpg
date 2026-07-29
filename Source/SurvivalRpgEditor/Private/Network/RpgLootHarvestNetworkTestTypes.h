#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgLootSourceComponent.h"

#include "RpgLootHarvestNetworkTestTypes.generated.h"

class URpgHarvestableInstancedMeshComponent;
class URpgHarvestProfile;
class URpgInventoryManagerComponent;
class URpgLootTable;
class USceneComponent;

/** Test-only loot source that accepts a transient deterministic table without widening the gameplay API. */
UCLASS(NotBlueprintable, Transient)
class URpgNetworkAutomationLootSourceComponent final : public URpgLootSourceComponent
{
	GENERATED_BODY()

public:
	void ConfigureLootTable(URpgLootTable* InLootTable);
};

/** Stackable 1x1 material used by the real PIE replication test. */
UCLASS(NotBlueprintable, Transient)
class URpgNetworkAutomationMaterialDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationMaterialDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Second material proving that a harvest overflow remains one complete multi-row batch. */
UCLASS(NotBlueprintable, Transient)
class URpgNetworkAutomationSecondMaterialDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationSecondMaterialDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Replicated corpse-like fixture using the production inventory and loot-source components. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationLootFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationLootFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URpgInventoryManagerComponent* GetInventory() const { return Inventory; }
	URpgNetworkAutomationLootSourceComponent* GetLootSource() const { return LootSource; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<URpgInventoryManagerComponent> Inventory;

	UPROPERTY()
	TObjectPtr<URpgNetworkAutomationLootSourceComponent> LootSource;
};

/** Player-state fixture retaining real inventory/skills while skipping Experience-only initialization. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationHarvesterState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Asset-free replicated resource node containing one stable real harvestable HISM instance. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationHarvestFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationHarvestFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URpgHarvestableInstancedMeshComponent* GetHarvestableInstances() const
	{
		return HarvestableInstances;
	}

	bool ConfigureHarvestProfile(URpgHarvestProfile* InProfile);

private:
	UPROPERTY()
	TObjectPtr<URpgHarvestableInstancedMeshComponent> HarvestableInstances;
};
