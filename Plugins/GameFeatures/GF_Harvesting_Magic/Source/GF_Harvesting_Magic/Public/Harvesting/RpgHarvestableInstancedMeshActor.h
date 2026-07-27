#pragma once

#include "GameFramework/Actor.h"

#include "RpgHarvestableInstancedMeshActor.generated.h"

class URpgHarvestableInstancedMeshComponent;

/** Replicated wrapper for a project-controlled group of stable-index harvestable HISM instances. */
UCLASS(Blueprintable, meta = (DisplayName = "RPG Harvestable Instanced Mesh Actor"))
class GF_HARVESTING_MAGIC_API ARpgHarvestableInstancedMeshActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestableInstancedMeshActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Authored resource instances and the replicated active/depleted state for this actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgHarvestableInstancedMeshComponent> HarvestableInstances;
};
