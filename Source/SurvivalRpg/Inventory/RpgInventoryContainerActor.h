#pragma once

#include "GameFramework/Actor.h"

#include "RpgInventoryContainerActor.generated.h"

class USceneComponent;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;

/**
 * Placeable replicated world container for shared storage, chests, and loot proxies.
 *
 * Designers can subclass this actor to add meshes, locks, VFX, or custom interaction presentation while the
 * inventory data remains owned by URpgInventoryManagerComponent and transfer rules stay server-authoritative.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgInventoryContainerActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgInventoryContainerActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Replicated inventory that stores this container's item entries. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	URpgInventoryManagerComponent* GetInventoryManager() const { return InventoryManagerComponent; }

	/** Interaction and access-rule component for this world container. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	URpgInventoryContainerComponent* GetContainerComponent() const { return ContainerComponent; }

protected:
	/** Simple root so Blueprint children can attach meshes and interaction visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Authoritative replicated inventory for shared storage contents. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	TObjectPtr<URpgInventoryManagerComponent> InventoryManagerComponent;

	/** Access, interaction, and crafting-source metadata for this shared container. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	TObjectPtr<URpgInventoryContainerComponent> ContainerComponent;
};
