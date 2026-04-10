// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgWorldCollectable.h"


ARpgWorldCollectable::ARpgWorldCollectable()
{
}

void ARpgWorldCollectable::GatherInteractionOptions(const FInteractionQuery& InteractQuery,
                                                    FInteractionOptionBuilder& InteractionBuilder)
{
	InteractionBuilder.AddInteractionOption(Option);
}

FInventoryPickup ARpgWorldCollectable::GetPickupInventory() const
{
	return StaticInventory;
}


