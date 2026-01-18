// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "AbilitySystemInterface.h"
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
	APawn* Pawn = GetPawn<APawn>();
	const AController* C = GetController<AController>();
	APlayerState* PS = GetPlayerState<APlayerState>();

	if (!Pawn || !C) return;
	if (C->IsPlayerController() && !PS) return;
	if (!PS) return;

	// Wenn wir schon einen ASC haben und Avatar passt -> "sticky broadcast"
	if (bInitialized && AbilitySystemComponent)
	{
		if (AbilitySystemComponent->GetAvatarActor() == Pawn)
		{
			OnAscReady.Broadcast(AbilitySystemComponent);
			return;
		}

		// Initialized, aber Avatar stimmt nicht -> reinit erlauben
		bInitialized = false;
		AbilitySystemComponent = nullptr;
	}

	// ASC holen (bevorzugt via Interface)
	URpgAbilitySystemComponent* Asc = nullptr;
	if (PS->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
	{
		Asc = Cast<URpgAbilitySystemComponent>(Cast<IAbilitySystemInterface>(PS)->GetAbilitySystemComponent());
	}
	else
	{
		Asc = PS->FindComponentByClass<URpgAbilitySystemComponent>();
	}

	if (!Asc) return;

	bInitialized = true;
	AbilitySystemComponent = Asc;

	const bool bNeedsInit = (Asc->GetAvatarActor() != Pawn);
	if (bNeedsInit)
	{
		Asc->InitAbilityActorInfo(PS, Pawn);
	}

	// IMMER broadcasten sobald wir einen gültigen ASC haben (late listeners profitieren)
	OnAscReady.Broadcast(Asc);

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
