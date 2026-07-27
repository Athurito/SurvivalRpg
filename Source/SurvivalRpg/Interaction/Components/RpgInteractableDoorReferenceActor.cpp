// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractableDoorReferenceActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractableDoorComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractableDoorReferenceActor)

ARpgInteractableDoorReferenceActor::ARpgInteractableDoorReferenceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetMobility(EComponentMobility::Movable);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(DoorMesh);
	InteractionVolume->SetBoxExtent(FVector(60.0f, 20.0f, 120.0f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(Rpg_TraceChannel_Interaction, ECR_Block);

	DoorInteraction = CreateDefaultSubobject<URpgInteractableDoorComponent>(TEXT("DoorInteraction"));
}

void ARpgInteractableDoorReferenceActor::BeginPlay()
{
	Super::BeginPlay();
	if (DoorInteraction)
	{
		DoorInteraction->OnDoorStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDoorStateChanged);
		HandleDoorStateChanged(DoorInteraction->IsDoorOpen(), DoorInteraction->IsDoorLocked());
	}
}

void ARpgInteractableDoorReferenceActor::HandleDoorStateChanged(const bool bIsOpen, const bool bIsLocked)
{
	if (DoorMesh)
	{
		DoorMesh->SetRelativeRotation(FRotator(0.0f, bIsOpen ? OpenYawDegrees : 0.0f, 0.0f));
	}
	K2_AnimateDoorState(bIsOpen, bIsLocked);
}
