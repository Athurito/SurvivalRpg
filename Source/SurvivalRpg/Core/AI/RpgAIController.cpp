// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "StateTree.h"
#include "SurvivalRpg/Core/AI/RpgAIPlayerState.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

ARpgAIController::ARpgAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;

	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	StateTreeComponent->SetStartLogicAutomatically(false);
	BrainComponent = StateTreeComponent;
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

	if (URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(InPawn))
	{
		PawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePawnAbilitySystemInitialized));
	}
}

void ARpgAIController::OnUnPossess()
{
	StopStateTreeLogic(TEXT("UnPossess"));
	Super::OnUnPossess();
}

void ARpgAIController::HandlePawnAbilitySystemInitialized()
{
	if (!StateTreeComponent)
	{
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	const URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(ControlledPawn);
	const URpgAIPawnData* PawnData = PawnExtension ? PawnExtension->GetPawnData<URpgAIPawnData>() : nullptr;
	if (!PawnData)
	{
		PawnData = DefaultPawnData.Get();
	}

	if (!PawnData || !PawnData->StateTree)
	{
		return;
	}

	StateTreeComponent->SetStateTree(PawnData->StateTree);
	StateTreeComponent->StartLogic();
}

void ARpgAIController::StopStateTreeLogic(const FString& Reason)
{
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(Reason);
	}
}
