// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgMoverPawn.h"

#include "Components/PrimitiveComponent.h"
#include "RpgCharacterMoverComponent.h"
#include "RpgDeathComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "RpgPawnGameplayComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"

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
	PawnExtensionComponent->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtensionComponent->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	PawnGameplayComponent = CreateDefaultSubobject<URpgPawnGameplayComponent>(TEXT("PawnGameplayComponent"));
	EquipmentManagerComponent = CreateDefaultSubobject<URpgEquipmentManagerComponent>(TEXT("EquipmentManagerComponent"));
	HealthComponent = CreateDefaultSubobject<URpgHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
	DeathComponent = CreateDefaultSubobject<URpgDeathComponent>(TEXT("DeathComponent"));
}

URpgAbilitySystemComponent* ARpgMoverPawn::GetRpgAbilitySystemComponent() const
{
	return PawnExtensionComponent ? PawnExtensionComponent->GetRpgAbilitySystemComponent() : nullptr;
}

UAbilitySystemComponent* ARpgMoverPawn::GetAbilitySystemComponent() const
{
	return GetRpgAbilitySystemComponent();
}

void ARpgMoverPawn::OnAbilitySystemInitialized()
{
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent();
	check(ASC);
	HealthComponent->InitializeWithAbilitySystem(ASC);
	DeathComponent->InitializeWithAbilitySystem(ASC);

	if (HealthComponent->IsDeadOrDying())
	{
		// Death replication can arrive before PawnData/PlayerState finishes the ASC binding on a late joiner.
		OnDeathStarted(this);
	}
	else if (HasAuthority())
	{
		if (const URpgHealthSet* HealthSet = ASC->GetSet<URpgHealthSet>())
		{
			ASC->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());
		}
	}
}

void ARpgMoverPawn::OnAbilitySystemUninitialized()
{
	DeathComponent->UninitializeFromAbilitySystem();
	HealthComponent->UninitializeFromAbilitySystem();
}

void ARpgMoverPawn::OnDeathStarted(AActor* OwningActor)
{
	if (URpgCharacterMoverComponent* Mover = FindComponentByClass<URpgCharacterMoverComponent>())
	{
		Mover->DisableMovementForDeath();
	}

	// Blueprint owns the root primitive; leave the gameplay mesh and its cosmetic follower independent.
	if (UPrimitiveComponent* RootCollision = Cast<UPrimitiveComponent>(GetRootComponent()))
	{
		RootCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RootCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void ARpgMoverPawn::OnDeathFinished(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(GetController()))
	{
		if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
		{
			GameMode->NotifyPlayerDeath(PlayerController);
		}
	}
}

void ARpgMoverPawn::FellOutOfWorld(const UDamageType& DamageType)
{
	if (HasAuthority())
	{
		HealthComponent->DamageSelfDestruct(true);
	}
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
