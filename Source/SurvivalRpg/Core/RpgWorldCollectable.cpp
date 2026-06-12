// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgWorldCollectable.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"


ARpgWorldCollectable::ARpgWorldCollectable()
{
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	bReplicates = true;
	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->InitSphereRadius(120.0f);
	InteractionCollision->SetCollisionProfileName(TEXT("Interactable_OverlapDynamic"));
	InteractionCollision->SetGenerateOverlapEvents(true);

	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(SceneRoot);
	DisplayMesh->SetCollisionProfileName(TEXT("Interactable_BlockDynamic"));
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


