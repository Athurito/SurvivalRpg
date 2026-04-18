// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

ARpgPlayerState::ARpgPlayerState()
{
	// The local HUD reads attributes from the PlayerState-owned ASC, so keep updates responsive.
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));

	PlayerProgressionComponent = CreateDefaultSubobject<URpgPlayerProgressionComponent>(TEXT("PlayerProgressionComponent"));
	TradeSkillProgressionComponent = CreateDefaultSubobject<URpgTradeSkillProgressionComponent>(TEXT("TradeSkillProgressionComponent"));
	InventoryManagerComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("InventoryManagerComponent"));
}

void ARpgPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARpgPlayerState, RespawnState);
	DOREPLIFETIME(ARpgPlayerState, CheckpointState);
}

ARpgPlayerController* ARpgPlayerState::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
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

void ARpgPlayerState::SetPawnData(const URpgPawnData* InPawnData)
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
	FRpgReplicatedRespawnState NewState;
	NewState.bIsWaitingForRespawn = bInIsWaitingForRespawn;
	NewState.RespawnAvailableServerTime = InRespawnAvailableServerTime;

	if (RespawnState == NewState)
	{
		return;
	}

	RespawnState = NewState;
	BroadcastRespawnStateChanged();
}

void ARpgPlayerState::SetCheckpointData(bool bInHasCheckpoint, const FTransform& InCheckpointTransform)
{
	FRpgReplicatedCheckpointState NewState;
	NewState.bHasCheckpoint = bInHasCheckpoint;
	NewState.CheckpointTransform = InCheckpointTransform;

	if (CheckpointState == NewState)
	{
		return;
	}

	CheckpointState = NewState;
	BroadcastCheckpointChanged();
}

float ARpgPlayerState::GetRemainingRespawnTime() const
{
	if (!RespawnState.bIsWaitingForRespawn)
	{
		return 0.0f;
	}

	const AGameStateBase* WorldGameState = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const float ServerWorldTime = WorldGameState ? WorldGameState->GetServerWorldTimeSeconds() : 0.0f;
	return FMath::Max(0.0f, RespawnState.RespawnAvailableServerTime - ServerWorldTime);
}

bool ARpgPlayerState::CanRespawnNow() const
{
	return RespawnState.bIsWaitingForRespawn && (GetRemainingRespawnTime() <= 0.0f);
}

void ARpgPlayerState::OnRep_RespawnState()
{
	BroadcastRespawnStateChanged();
}

void ARpgPlayerState::OnRep_CheckpointState()
{
	BroadcastCheckpointChanged();
}

void ARpgPlayerState::BroadcastRespawnStateChanged() const
{
	const_cast<ARpgPlayerState*>(this)->OnRespawnStateChanged.Broadcast(
		RespawnState.bIsWaitingForRespawn,
		RespawnState.RespawnAvailableServerTime);
}

void ARpgPlayerState::BroadcastCheckpointChanged() const
{
	const_cast<ARpgPlayerState*>(this)->OnCheckpointChanged.Broadcast(
		CheckpointState.bHasCheckpoint,
		CheckpointState.CheckpointTransform);
}
