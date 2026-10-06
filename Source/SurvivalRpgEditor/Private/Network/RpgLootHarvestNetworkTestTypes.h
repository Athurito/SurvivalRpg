#pragma once

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameFramework/Actor.h"
#include "Harvesting/RpgHarvestSwarm.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgLootSourceComponent.h"

#include "RpgLootHarvestNetworkTestTypes.generated.h"

class URpgHarvestableComponent;
class URpgHarvestableInstancedMeshComponent;
class URpgHarvestableInstancesComponent;
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

/** Non-replicated owner of harvestable instances, loaded on every machine like a PCG partition actor. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationHarvestInstancesFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationHarvestInstancesFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URpgHarvestableInstancesComponent* GetHarvestableInstances() const
	{
		return HarvestableInstances;
	}

	/** Assigns the static designer profile before BeginPlay; tests call it on every machine, as content loads everywhere. */
	bool ConfigureHarvestProfile(URpgHarvestProfile* InProfile);

private:
	UPROPERTY()
	TObjectPtr<URpgHarvestableInstancesComponent> HarvestableInstances;
};

/** Dormant replicated actor-backed resource node with the real harvestable node component. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationHarvestNodeFixture final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationHarvestNodeFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	URpgHarvestableComponent* GetHarvestableNode() const
	{
		return HarvestableNode;
	}

	/** Assigns the static designer profile; tests call it on every machine, as content would load it everywhere. */
	bool ConfigureHarvestProfile(URpgHarvestProfile* InProfile);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<URpgHarvestableComponent> HarvestableNode;
};

/** Plain non-replicated actor each client spawns per creature of ARpgNetworkAutomationSwarm. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationSwarmCreature final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationSwarmCreature(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Swarm fixture whose creatures present as ARpgNetworkAutomationSwarmCreature, like a swarm Blueprint configures. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationSwarm final : public ARpgHarvestSwarm
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationSwarm(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Stride harvest ability the network test grants directly; the test configures the granted instance on the server. */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationStrideAbility final : public URpgGameplayAbility_Harvest
{
	GENERATED_BODY()

public:
	/**
	 * Harvests every target within Radius cm around the harvester every PulseSeconds for DurationSeconds, Sections
	 * sections each, and shows CueTag on the harvester while the stride runs.
	 */
	void ConfigureStride(float Radius, float DurationSeconds, float PulseSeconds, int32 Sections, FGameplayTag CueTag);
};
