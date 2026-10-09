#pragma once

#include "GameFramework/Actor.h"

#include "RpgCraftingStationActor.generated.h"

class URpgCraftingStationComponent;
class USceneComponent;
class USphereComponent;

/**
 * Placeable crafting station with interaction collision. Orders take materials from and deliver outputs into the
 * connected chests, so the station owns no inventory. Designers subclass it to add meshes, recipes and visuals.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgCraftingStationActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgCraftingStationActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Crafting rules component that runs the station's order. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	URpgCraftingStationComponent* GetCraftingStationComponent() const { return CraftingStationComponent; }

protected:
	/** Simple root so Blueprint children can attach station meshes and VFX. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Native overlap used by Lyra-style interaction scans to discover this station. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<USphereComponent> InteractionCollision;

	/** Reusable crafting logic and connected-chest access. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<URpgCraftingStationComponent> CraftingStationComponent;
};
