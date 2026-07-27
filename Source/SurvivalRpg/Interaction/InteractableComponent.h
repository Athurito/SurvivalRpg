// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IInteractableTarget.h"
#include "Components/ActorComponent.h"
#include "SurvivalRpg/Inventory/IPickupable.h"
#include "InteractableComponent.generated.h"


/** Lightweight configurable interaction provider for actors that own suitable interaction-query collision. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API UInteractableComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	explicit UInteractableComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	//~IInteractableTarget contract
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	//~End of IInteractableTarget contract
	
protected:
	/** Static designer-authored interaction emitted for every local or authoritative query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rpg|Interaction")
	FInteractionOption Option;

	/** Optional static pickup payload retained for legacy component-authored collectables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rpg|Interaction")
	FInventoryPickup StaticInventory;
};
