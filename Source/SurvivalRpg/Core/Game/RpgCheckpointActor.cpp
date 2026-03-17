// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCheckpointActor.h"

#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"

ARpgCheckpointActor::ARpgCheckpointActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ActivationSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ActivationSphere"));
	ActivationSphere->SetupAttachment(SceneRoot);
	ActivationSphere->SetSphereRadius(150.0f);
	ActivationSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActivationSphere->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleActivationSphereBeginOverlap);

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(SceneRoot);
}

void ARpgCheckpointActor::ActivateCheckpointForActor(AActor* ActivatingActor)
{
	ARpgPlayerController* PlayerController = nullptr;
	if (!TryResolvePlayerController(ActivatingActor, PlayerController))
	{
		return;
	}

	ActivateCheckpointForPlayerController(PlayerController);
}

void ARpgCheckpointActor::ActivateCheckpointForPlayerController(ARpgPlayerController* PlayerController)
{
	if (!HasAuthority() || !PlayerController)
	{
		return;
	}

	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->SetPlayerCheckpoint(PlayerController, GetRespawnTransform());
	}
}

FTransform ARpgCheckpointActor::GetRespawnTransform() const
{
	return RespawnPoint ? RespawnPoint->GetComponentTransform() : GetActorTransform();
}

void ARpgCheckpointActor::HandleActivationSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bActivateOnOverlap || !HasAuthority())
	{
		return;
	}

	ActivateCheckpointForActor(OtherActor);
}

bool ARpgCheckpointActor::TryResolvePlayerController(AActor* ActivatingActor, ARpgPlayerController*& OutPlayerController) const
{
	OutPlayerController = nullptr;

	if (!ActivatingActor)
	{
		return false;
	}

	if (ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(ActivatingActor))
	{
		OutPlayerController = PlayerController;
		return true;
	}

	if (const APawn* Pawn = Cast<APawn>(ActivatingActor))
	{
		OutPlayerController = Cast<ARpgPlayerController>(Pawn->GetController());
		return (OutPlayerController != nullptr);
	}

	return false;
}
