#pragma once

#include "GameFramework/Actor.h"

#include "RpgCraftingStationActor.generated.h"

class URpgCraftingStationComponent;
class URpgInventoryManagerComponent;
class USceneComponent;
class USphereComponent;

/**
 * Placeable V1 crafting station with native output inventory and interaction collision.
 *
 * Designers can subclass this actor to add meshes, recipes, and station visuals while the output
 * inventory stays in the existing replicated inventory manager path.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgCraftingStationActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgCraftingStationActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Crafting rules component that consumes resources and stores recipe outputs. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	URpgCraftingStationComponent* GetCraftingStationComponent() const { return CraftingStationComponent; }

	/** Fixed-slot output inventory where crafted items wait when not auto-deposited. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	URpgInventoryManagerComponent* GetOutputInventoryComponent() const { return OutputInventoryComponent; }

protected:
	/** Simple root so Blueprint children can attach station meshes and VFX. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Native overlap used by Lyra-style interaction scans to discover this station. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<USphereComponent> InteractionCollision;

	/** Reusable crafting logic and base-storage linkage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<URpgCraftingStationComponent> CraftingStationComponent;

	/** Replicated output inventory owned by this crafting station actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<URpgInventoryManagerComponent> OutputInventoryComponent;
};
