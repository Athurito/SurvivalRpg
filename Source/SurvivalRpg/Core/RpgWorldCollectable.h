// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "RpgWorldCollectable.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/** Replicated world pickup that exposes its inventory through the shared Lyra-style interaction flow. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgWorldCollectable : public AActor, public IInteractableTarget, public IPickupable
{
	GENERATED_BODY()

public:

	ARpgWorldCollectable();

	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual FInventoryPickup GetPickupInventory() const override;

protected:
	/** Builds the semantic collect option used by focus, nearby, and authority-validation queries. */
	virtual FInteractionOption BuildCollectInteractionOption(const FInteractionQuery& InteractQuery) const;

	/** Overlap shape used by nearby interaction discovery; gameplay execution still validates on the server. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectable")
	TObjectPtr<USphereComponent> InteractionCollision;

	/** World presentation and blocking trace target for this pickup. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectable")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;

	/** Designer-tunable collect prompt and optional ability override. Static definition data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectable|Interaction")
	FInteractionOption Option;

	/** Static pickup payload used until a specialized subclass supplies a runtime inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectable|Inventory")
	FInventoryPickup StaticInventory;
};
