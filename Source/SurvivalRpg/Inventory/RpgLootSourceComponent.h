#pragma once

#include "Components/ActorComponent.h"
#include "IPickupable.h"

#include "RpgLootSourceComponent.generated.h"

class URpgHealthComponent;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;

/**
 * Populates an actor inventory with loot when the owning actor dies.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgLootSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgLootSourceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Loot")
	void PopulateLoot();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleDeathFinished(AActor* OwningActor);

	/** Static V1 loot templates added to the owner's InventoryManager when loot is populated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	TArray<FPickupTemplate> LootTemplates;

	/** Static V1 loot instances added to the owner's InventoryManager when loot is populated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	TArray<FPickupInstance> LootInstances;

	/** Whether the owning container starts inaccessible and becomes lootable after death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Loot")
	bool bUnlockContainerOnDeath = true;

private:
	URpgInventoryManagerComponent* FindInventoryManager() const;
	URpgInventoryContainerComponent* FindContainerComponent() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> BoundHealthComponent;

	UPROPERTY(Transient)
	bool bLootPopulated = false;
};
