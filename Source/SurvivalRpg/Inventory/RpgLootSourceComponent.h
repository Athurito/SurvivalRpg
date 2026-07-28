#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "IPickupable.h"
#include "Loot/RpgLootResolver.h"

#include "RpgLootSourceComponent.generated.h"

class URpgHealthComponent;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;
class URpgLootTable;
class FDataValidationContext;

/**
 * Rolls an actor's loot once on the authoritative death path and atomically populates its inventory.
 * A transient delivery failure retains the original roll and concrete item state for an unchanged retry.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgLootSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgLootSourceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Resolves at most one authoritative roll and retries its unchanged atomic delivery until it succeeds. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Loot")
	void PopulateLoot();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleDeathFinished(AActor* OwningActor);

	/** Server-resolved loot table used for this corpse. Leave unset only for migrated fixed-loot actors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	TObjectPtr<URpgLootTable> LootTable;

	/** Designer-authored source level forwarded to generated equipment as its authoritative item level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot", meta = (ClampMin = "1", UIMin = "1"))
	int32 SourceLevel = 1;

	/** Semantic tags describing this enemy or reward source; forwarded unchanged to the server loot context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	FGameplayTagContainer SourceTags;

	/** Deprecated fixed templates retained only as a serialized fallback for actors without a LootTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use LootTable. Fixed templates are read only when LootTable is unset."))
	TArray<FPickupTemplate> LootTemplates;

	/** Deprecated fixed instances retained only as a serialized fallback for actors without a LootTable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use LootTable. Fixed instances are read only when LootTable is unset."))
	TArray<FPickupInstance> LootInstances;

	/** Whether the owning container starts inaccessible and becomes lootable after death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	bool bUnlockContainerOnDeath = true;

private:
	bool TryPopulateLoot();
	URpgInventoryManagerComponent* FindInventoryManager() const;
	URpgInventoryContainerComponent* FindContainerComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> BoundHealthComponent;

	UPROPERTY(Transient)
	bool bLootPopulated = false;

	/** First successful authoritative table result retained across transient inventory/materialization failures. */
	UPROPERTY(Transient)
	FRpgLootRollResult CachedLootRoll;

	/** Materialized batch retained unchanged until one atomic inventory insertion succeeds. */
	UPROPERTY(Transient)
	FInventoryPickup CachedLootPickup;

	UPROPERTY(Transient)
	bool bLootRollResolved = false;

	UPROPERTY(Transient)
	bool bLootMaterialized = false;
};
