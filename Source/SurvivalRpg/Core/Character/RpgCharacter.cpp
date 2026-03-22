// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacter.h"

#include "RpgCharacterMovementComponent.h"
#include "RpgDeathComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "Components/CapsuleComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
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
	
	DeathComponent = CreateDefaultSubobject<URpgDeathComponent>(TEXT("DeathComponent"));
	// RespawnComponent = CreateDefaultSubobject<URpgRespawnComponent>(TEXT("RespawnComponent"));
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

	HealthComponent->InitializeWithAbilitySystem(Asc);
	DeathComponent->InitializeWithAbilitySystem(Asc);
}

void ARpgCharacter::OnAbilitySystemUninitialized()
{
	HealthComponent->UninitializeFromAbilitySystem();
	DeathComponent->UninitializeFromAbilitySystem();
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

void ARpgCharacter::OnDeathStarted(AActor* OwningActor)
{
	DisableMovementAndCollision();
}

void ARpgCharacter::OnDeathFinished(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ARpgPlayerController* PC = GetRpgPlayerController())
	{
		if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
		{
			GameMode->NotifyPlayerDeath(PC);
		}
	}
}

void ARpgCharacter::DisableMovementAndCollision() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(true);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
}

void ARpgCharacter::EnterDeadState()
{
	// Controller lösen
	DetachFromControllerPendingDestroy();

	// Actor bleibt aber bestehen
	SetActorEnableCollision(false);

	// Optional: Pawn als Dead markieren
	// bIsDead = true;
}

// Called to bind functionality to input
void ARpgCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtensionComponent->SetupPlayerInputComponent();
}



