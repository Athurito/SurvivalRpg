// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacter.h"

#include "RpgCharacterMovementComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "RpgPawnGameplayComponent.h"


ARpgCharacter::ARpgCharacter(const FObjectInitializer& ObjectInitializer) : 
	Super(ObjectInitializer.SetDefaultSubobjectClass<URpgCharacterMovementComponent>(CharacterMovementComponentName))
{
	MovementComponent = Cast<URpgCharacterMovementComponent>(ACharacter::GetMovementComponent());
	
	PrimaryActorTick.bCanEverTick = true;
	PawnExtensionComponent = CreateDefaultSubobject<URpgPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnGameplayComponent = CreateDefaultSubobject<URpgPawnGameplayComponent>(TEXT("PawnGameplayComponent"));
}

// Called when the game starts or when spawned
void ARpgCharacter::BeginPlay()
{
	PawnExtensionComponent->TryInitialize();
	Super::BeginPlay();
	
}

void ARpgCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PawnExtensionComponent)
		PawnExtensionComponent->UnInitialize();
	
	Super::EndPlay(EndPlayReason);
}

void ARpgCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnExtensionComponent->TryInitialize();
}

void ARpgCharacter::UnPossessed()
{
	if (PawnExtensionComponent)
		PawnExtensionComponent->UnInitialize();
	Super::UnPossessed();
}

void ARpgCharacter::OnRep_Controller()
{
	PawnExtensionComponent->TryInitialize();
	Super::OnRep_Controller();
}

void ARpgCharacter::OnRep_PlayerState()
{
	PawnExtensionComponent->TryInitialize();
	Super::OnRep_PlayerState();
}


void ARpgCharacter::ToggleCrouch()
{
	// const ULyraCharacterMovementComponent* LyraMoveComp = CastChecked<ULyraCharacterMovementComponent>(GetCharacterMovement());
	//
	// if (IsCrouched() || LyraMoveComp->bWantsToCrouch)
	// {
	// 	UnCrouch();
	// }
	// else if (LyraMoveComp->IsMovingOnGround())
	// {
	// 	Crouch();
	// }
}

void ARpgCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	// {
	// 	LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Crouching, 1);
	// }
	//
	//
	// Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ARpgCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	// {
	// 	LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_Crouching, 0);
	// }
	//
	// Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

// Called every frame
void ARpgCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ARpgCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}



