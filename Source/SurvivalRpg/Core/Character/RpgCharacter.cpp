// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacter.h"

#include "RpgCharacterMovementComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"


ARpgCharacter::ARpgCharacter(const FObjectInitializer& ObjectInitializer) : 
	Super(ObjectInitializer.SetDefaultSubobjectClass<URpgCharacterMovementComponent>(CharacterMovementComponentName))
{
	MovementComponent = Cast<URpgCharacterMovementComponent>(ACharacter::GetMovementComponent());
	
	PrimaryActorTick.bCanEverTick = true;
	PawnExtensionComponent = CreateDefaultSubobject<URpgPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtensionComponent->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtensionComponent->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	
	HealthComponent = CreateDefaultSubobject<URpgHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
}

ARpgPlayerController* ARpgCharacter::GetRpgPlayerController() const
{
	return CastChecked<ARpgPlayerController>(GetController(), ECastCheckedType::NullAllowed);
}

ARpgPlayerState* ARpgCharacter::GetRpgPlayerState() const
{
	return CastChecked<ARpgPlayerState>(GetPlayerState(), ECastCheckedType::NullAllowed);
}

URpgAbilitySystemComponent* ARpgCharacter::GetRpgAbilitySystemComponent() const
{
	check(PawnExtensionComponent);
	return Cast<URpgAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* ARpgCharacter::GetAbilitySystemComponent() const
{
	if (PawnExtensionComponent == nullptr) return nullptr;
	
	return PawnExtensionComponent->GetRpgAbilitySystemComponent();
}

// Called when the game starts or when spawned
void ARpgCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ARpgCharacter::OnAbilitySystemInitialized()
{
	URpgAbilitySystemComponent* Asc = GetRpgAbilitySystemComponent();
	check(Asc);

	//HealthComponent->InitializeWithAbilitySystem(Asc);
}

void ARpgCharacter::OnAbilitySystemUninitialized()
{
	//HealthComponent->UninitializeFromAbilitySystem();
}

void ARpgCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgCharacter::UnPossessed()
{
	Super::UnPossessed();
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnExtensionComponent->HandlePlayerStateReplicated();
}

void ARpgCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	HealthComponent->DamageSelfDestruct(/*bFellOutOfWorld=*/ true);
}

// Called to bind functionality to input
void ARpgCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtensionComponent->SetupPlayerInputComponent();
}



