// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "RpgWorldCollectable.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API ARpgWorldCollectable : public AActor, public IInteractableTarget, public IPickupable
{
	GENERATED_BODY()

public:

	ARpgWorldCollectable();

	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual FInventoryPickup GetPickupInventory() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectable")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectable")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;

	UPROPERTY(EditAnywhere)
	FInteractionOption Option;

	UPROPERTY(EditAnywhere)
	FInventoryPickup StaticInventory;
};
