// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerState.h"

#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

ARpgPlayerState::ARpgPlayerState()
{
	PlayerProgressionComponent = CreateDefaultSubobject<URpgPlayerProgressionComponent>(TEXT("PlayerProgressionComponent"));
	TradeSkillProgressionComponent = CreateDefaultSubobject<URpgTradeSkillProgressionComponent>(TEXT("TradeSkillProgressionComponent"));
	InventoryManagerComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("InventoryManagerComponent"));
}

void ARpgPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
	{
		AGameStateBase* WorldGameState = World->GetGameState();
		check(WorldGameState);

		URpgExperienceManagerComponent* ExperienceComponent = WorldGameState->FindComponentByClass<URpgExperienceManagerComponent>();
		check(ExperienceComponent);

		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	}
}

void ARpgPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARpgPlayerState, RespawnState);
	DOREPLIFETIME(ARpgPlayerState, CheckpointState);
}

void ARpgPlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	if (URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExtension->CheckDefaultInitialization();
	}
}

ARpgPlayerController* ARpgPlayerState::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
}

void ARpgPlayerState::OnExperienceLoaded(const URpgExperienceDefinition* CurrentExperience)
{
	if (ARpgGameModeBase* RpgGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		if (const URpgPawnData* NewPawnData = RpgGameMode->GetPawnDataForController(GetOwningController()))
		{
			SetPawnData(NewPawnData);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ARpgPlayerState::OnExperienceLoaded(): Unable to find PawnData for [%s]."), *GetNameSafe(this));
		}
	}
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
