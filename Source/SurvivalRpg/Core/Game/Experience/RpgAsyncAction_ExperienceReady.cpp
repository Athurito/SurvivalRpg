// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgAsyncAction_ExperienceReady.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "RpgExperienceManagerComponent.h"

URpgAsyncAction_ExperienceReady* URpgAsyncAction_ExperienceReady::WaitForExperienceReady(UObject* WorldContextObject)
{
	URpgAsyncAction_ExperienceReady* Action = NewObject<URpgAsyncAction_ExperienceReady>();
	Action->WorldContextObject = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void URpgAsyncAction_ExperienceReady::Activate()
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			Step2_ListenToExperienceLoading(GameState);
		}
		else
		{
			World->GameStateSetEvent.AddUObject(this, &ThisClass::Step1_HandleGameStateSet);
		}
	}
	else
	{
		SetReadyToDestroy();
	}
}

void URpgAsyncAction_ExperienceReady::Step1_HandleGameStateSet(AGameStateBase* GameState)
{
	Step2_ListenToExperienceLoading(GameState);
}

void URpgAsyncAction_ExperienceReady::Step2_ListenToExperienceLoading(AGameStateBase* GameState)
{
	if (URpgExperienceManagerComponent* ExperienceComponent = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr)
	{
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnRpgExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::Step3_HandleExperienceLoaded));
	}
	else
	{
		SetReadyToDestroy();
	}
}

void URpgAsyncAction_ExperienceReady::Step3_HandleExperienceLoaded(const URpgExperienceDefinition* CurrentExperience)
{
	OnReady.Broadcast();
	SetReadyToDestroy();
}
