// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacter.h"

#include "RpgCharacterMovementComponent.h"
#include "RpgDeathComponent.h"
#include "RpgDownedComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "RpgPawnGameplayComponent.h"
#include "Components/CapsuleComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Camera/RpgCameraComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Equipment/RpgWeaponPresentationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"


ARpgCharacter::ARpgCharacter(const FObjectInitializer& ObjectInitializer) : 
	Super(ObjectInitializer.SetDefaultSubobjectClass<URpgCharacterMovementComponent>(CharacterMovementComponentName))
{
	// Avoid ticking characters if possible.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	SetNetCullDistanceSquared(900000000.0f);
	
	URpgCharacterMovementComponent* RpgMoveComp = CastChecked<URpgCharacterMovementComponent>(ACharacter::GetMovementComponent());
	RpgMoveComp->GravityScale = 1.0f;
	RpgMoveComp->MaxAcceleration = 2400.0f;
	RpgMoveComp->BrakingFrictionFactor = 1.0f;
	RpgMoveComp->BrakingFriction = 6.0f;
	RpgMoveComp->GroundFriction = 8.0f;
	RpgMoveComp->BrakingDecelerationWalking = 1400.0f;
	RpgMoveComp->bUseControllerDesiredRotation = false;
	RpgMoveComp->bOrientRotationToMovement = false;
	RpgMoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	RpgMoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
	RpgMoveComp->GetNavAgentPropertiesRef().bCanCrouch = true;
	RpgMoveComp->bCanWalkOffLedgesWhenCrouching = true;
	RpgMoveComp->SetCrouchedHalfHeight(65.0f);
	

	
	PawnExtensionComponent = CreateDefaultSubobject<URpgPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtensionComponent->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtensionComponent->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	
	WeaponPresentationComponent = CreateDefaultSubobject<URpgWeaponPresentationComponent>(TEXT("WeaponPresentationComponent"));
	
	HealthComponent = CreateDefaultSubobject<URpgHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
	
	DeathComponent = CreateDefaultSubobject<URpgDeathComponent>(TEXT("DeathComponent"));
	DownedComponent = CreateDefaultSubobject<URpgDownedComponent>(TEXT("DownedComponent"));
	DownedComponent->OnDownedStateChanged.AddDynamic(this, &ThisClass::OnDownedStateChanged);
	
	CameraComponent = CreateDefaultSubobject<URpgCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetRelativeLocation(FVector(-300.0f, 0.0f, 75.0f));
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 80.0f;
	CrouchedEyeHeight = 50.0f;
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
	DownedComponent->InitializeWithAbilitySystem(Asc);

	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandleAbilitySystemInitialized();
	}
}

void ARpgCharacter::OnAbilitySystemUninitialized()
{
	HealthComponent->UninitializeFromAbilitySystem();
	DeathComponent->UninitializeFromAbilitySystem();
	DownedComponent->UninitializeFromAbilitySystem();

	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandleAbilitySystemUninitialized();
	}
}

void ARpgCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnExtensionComponent->HandleControllerChanged();
	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandlePawnContextChanged();
	}
}

void ARpgCharacter::UnPossessed()
{
	Super::UnPossessed();
	PawnExtensionComponent->HandleControllerChanged();
	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandlePawnContextChanged();
	}
}

void ARpgCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnExtensionComponent->HandleControllerChanged();
	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandlePawnContextChanged();
	}
}

void ARpgCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnExtensionComponent->HandlePlayerStateReplicated();
	if (WeaponPresentationComponent != nullptr)
	{
		WeaponPresentationComponent->HandlePawnContextChanged();
	}

	if (URpgPawnGameplayComponent* PawnGameplayComponent = FindComponentByClass<URpgPawnGameplayComponent>())
	{
		PawnGameplayComponent->CheckDefaultInitialization();
	}
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

void ARpgCharacter::OnDownedStateChanged(ERpgDownedState NewState)
{
	if (NewState == ERpgDownedState::Downed)
	{
		DisableMovementForDowned();
		return;
	}

	if (!HealthComponent->IsDeadOrDying())
	{
		RestoreMovementAndCollision();
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

void ARpgCharacter::DisableMovementForDowned() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(true);
	}

	if (URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void ARpgCharacter::RestoreMovementAndCollision() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(false);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}
}

void ARpgCharacter::EnterDeadState()
{
	DetachFromControllerPendingDestroy();
	SetActorEnableCollision(false);
}

// Called to bind functionality to input
void ARpgCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtensionComponent->SetupPlayerInputComponent();
	if (URpgPawnGameplayComponent* PawnGameplayComponent = FindComponentByClass<URpgPawnGameplayComponent>())
	{
		PawnGameplayComponent->InitializePlayerInput(PlayerInputComponent);
	}
}



