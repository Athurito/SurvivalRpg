// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgAIController.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/AI/RpgAIPlayerState.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"

ARpgAIController::ARpgAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;
}

void ARpgAIController::InitPlayerState()
{
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	UWorld* const World = GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (GameMode == nullptr)
	{
		const AGameStateBase* const GameState = World ? World->GetGameState() : nullptr;
		GameMode = GameState ? GameState->GetDefaultGameMode() : nullptr;
	}

	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;

	SetPlayerState(World->SpawnActor<APlayerState>(ARpgAIPlayerState::StaticClass(), SpawnInfo));

	if (PlayerState && GameMode && PlayerState->GetPlayerName().IsEmpty())
	{
		PlayerState->SetPlayerNameInternal(GameMode->DefaultPlayerName.ToString());
	}

	if (ARpgAIPlayerState* RpgAIPlayerState = GetPlayerState<ARpgAIPlayerState>())
	{
		if (DefaultPawnData)
		{
			RpgAIPlayerState->SetPawnData(DefaultPawnData);
		}
	}
}

void ARpgAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ARpgBasePlayerState* RpgPlayerState = GetPlayerState<ARpgBasePlayerState>();
	URpgAbilitySystemComponent* AbilitySystemComponent = RpgPlayerState ? RpgPlayerState->GetRpgAbilitySystemComponent() : nullptr;
	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(InPawn);

	if (!RpgPlayerState || !AbilitySystemComponent || !PawnExtension)
	{
		return;
	}

	if (!PawnExtension->GetPawnData<URpgAIPawnData>())
	{
		if (const URpgAIPawnData* PawnData = RpgPlayerState->GetPawnData<URpgAIPawnData>())
		{
			PawnExtension->SetPawnData(PawnData);
		}
	}

	PawnExtension->InitializeAbilitySystemComponent(AbilitySystemComponent, RpgPlayerState);
}

void ARpgAIController::OnUnPossess()
{
	if (APawn* PawnBeingUnpossessed = GetPawn())
	{
		if (URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(PawnBeingUnpossessed))
		{
			PawnExtension->UninitializeAbilitySystem();
		}
		else if (ARpgBasePlayerState* RpgPlayerState = GetPlayerState<ARpgBasePlayerState>())
		{
			if (URpgAbilitySystemComponent* AbilitySystemComponent = RpgPlayerState->GetRpgAbilitySystemComponent())
			{
				if (AbilitySystemComponent->GetAvatarActor() == PawnBeingUnpossessed)
				{
					AbilitySystemComponent->SetAvatarActor(nullptr);
				}
			}
		}
	}

	Super::OnUnPossess();
}

void ARpgAIController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	Super::SetGenericTeamId(NewTeamID);
}

FGenericTeamId ARpgAIController::GetGenericTeamId() const
{
	const ARpgBasePlayerState* RpgPlayerState = GetPlayerState<ARpgBasePlayerState>();
	return RpgPlayerState ? RpgPlayerState->GetGenericTeamId() : Super::GetGenericTeamId();
}

ETeamAttitude::Type ARpgAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	return ARpgBasePlayerState::GetTeamAttitudeTowardsActor(GetGenericTeamId(), Other);
}
