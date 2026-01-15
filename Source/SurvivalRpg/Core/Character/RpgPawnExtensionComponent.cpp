// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

URpgPawnExtensionComponent::URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	AbilitySystemComponent = nullptr;
}

// Called when the game starts
void URpgPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URpgPawnExtensionComponent::TryInitialize()
{
	if (bInitialized) return;

	
	APlayerState* PS = GetPlayerState<APlayerState>();
	const AController* C = GetController<AController>();
	if (!C) return;
	if (C->IsPlayerController() && !PS) return;

	URpgAbilitySystemComponent* Asc = PS->FindComponentByClass<URpgAbilitySystemComponent>();
	if (!Asc) return;

	bInitialized = true;
	AbilitySystemComponent = Asc;

	APawn* Pawn = GetPawn<APawn>();

	const bool bNeedsInit = (Asc->GetAvatarActor() != Pawn);
	if (bNeedsInit)
	{
		Asc->InitAbilityActorInfo(PS, Pawn);
	}
	
	if (Pawn->HasAuthority())
	{
		Asc->ApplyDefaultAbilitySetupIfNeeded(Pawn);
	}
	
}

void URpgPawnExtensionComponent::UnInitialize()
{
	if (!bInitialized) return;
	
	if (APawn* Pawn = GetPawn<APawn>())
	{
		if (Pawn->HasAuthority() && AbilitySystemComponent)
		{
			AbilitySystemComponent->RemoveDefaultAbilitySetup();
		}
	}

	bInitialized = false;
	AbilitySystemComponent = nullptr;
}
