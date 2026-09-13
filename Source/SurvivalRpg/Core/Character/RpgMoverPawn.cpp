// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgMoverPawn.h"

#include "RpgPawnExtensionComponent.h"
#include "RpgPawnGameplayComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMoverPawn)

ARpgMoverPawn::ARpgMoverPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The source Blueprint updates presentation on actor tick. Mover owns the separate simulation ticks.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(false);

	// Body orientation is part of Mover's predicted state, independent of the controller's camera heading.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	PawnExtensionComponent = CreateDefaultSubobject<URpgPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnGameplayComponent = CreateDefaultSubobject<URpgPawnGameplayComponent>(TEXT("PawnGameplayComponent"));
}

URpgAbilitySystemComponent* ARpgMoverPawn::GetRpgAbilitySystemComponent() const
{
	return PawnExtensionComponent ? PawnExtensionComponent->GetRpgAbilitySystemComponent() : nullptr;
}

UAbilitySystemComponent* ARpgMoverPawn::GetAbilitySystemComponent() const
{
	return GetRpgAbilitySystemComponent();
}

void ARpgMoverPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtensionComponent->SetupPlayerInputComponent();
}

void ARpgMoverPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgMoverPawn::UnPossessed()
{
	Super::UnPossessed();
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgMoverPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgMoverPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnExtensionComponent->HandlePlayerStateReplicated();
	PawnGameplayComponent->CheckDefaultInitialization();
}
