// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

ARpgPlayerState::ARpgPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));

	PlayerProgressionComponent = CreateDefaultSubobject<URpgPlayerProgressionComponent>(TEXT("PlayerProgressionComponent"));
	TradeSkillProgressionComponent = CreateDefaultSubobject<URpgTradeSkillProgressionComponent>(TEXT("TradeSkillProgressionComponent"));
}

void ARpgPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARpgPlayerState, bIsWaitingForRespawn);
	DOREPLIFETIME(ARpgPlayerState, RespawnAvailableServerTime);
	DOREPLIFETIME(ARpgPlayerState, bHasCheckpoint);
	DOREPLIFETIME(ARpgPlayerState, CurrentCheckpointTransform);
}

ARpgPlayerController* ARpgPlayerState::GetRpgPlayerController() const
{
	return nullptr;
}

void ARpgPlayerState::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = FGameplayTag::RequestGameplayTag("Event.Abilities.Changed");
	EventData.Instigator = this;
	EventData.Target = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

TObjectPtr<URpgAbilitySystemComponent> ARpgPlayerState::GetRpgAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARpgPlayerState::SetPawnData(const UBasePawnData* InPawnData)
{
	check(InPawnData);
	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;
}

void ARpgPlayerState::SetRespawnState(bool bInIsWaitingForRespawn, float InRespawnAvailableServerTime)
{
	bIsWaitingForRespawn = bInIsWaitingForRespawn;
	RespawnAvailableServerTime = InRespawnAvailableServerTime;
}

void ARpgPlayerState::SetCheckpointData(bool bInHasCheckpoint, const FTransform& InCheckpointTransform)
{
	bHasCheckpoint = bInHasCheckpoint;
	CurrentCheckpointTransform = InCheckpointTransform;
}
