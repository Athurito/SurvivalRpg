// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

URpgPawnExtensionComponent::URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	PawnData = nullptr;
	AbilitySystemComponent = nullptr;
}

void URpgPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(URpgPawnExtensionComponent, PawnData);
}

void URpgPawnExtensionComponent::SetPawnData(const URpgPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		//UE_LOG(LogTemp, Error, TEXT("Trying to set PawnData [%s] on pawn [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(Pawn), *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;

	Pawn->ForceNetUpdate();

}


// Called when the game starts
void URpgPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();
}


void URpgPawnExtensionComponent::OnRep_PawnData()
{
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

	if (Asc->GetAvatarActor() != GetPawn<APawn>())
	{
		Asc->InitAbilityActorInfo(PS, GetPawn<APawn>());
	}
	
}

void URpgPawnExtensionComponent::UnInitialize()
{
	if (!bInitialized) return;

	bInitialized = false;
	AbilitySystemComponent = nullptr;
}
