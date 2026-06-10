// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameInstance.h"

#include "CommonLocalPlayer.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameUIManagerSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

void URpgGameInstance::Init()
{
	Super::Init();
	
	UGameFrameworkComponentManager* ComponentManager = GetSubsystem<UGameFrameworkComponentManager>(this);

	if (ensure(ComponentManager))
	{
		ComponentManager->RegisterInitState(RpgGameplayTags::InitState_Spawned, false, FGameplayTag());
		ComponentManager->RegisterInitState(RpgGameplayTags::InitState_DataAvailable, false, RpgGameplayTags::InitState_Spawned);
		ComponentManager->RegisterInitState(RpgGameplayTags::InitState_DataInitialized, false, RpgGameplayTags::InitState_DataAvailable);
		ComponentManager->RegisterInitState(RpgGameplayTags::InitState_GameplayReady, false, RpgGameplayTags::InitState_DataInitialized);
	}
}

int32 URpgGameInstance::AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId)
{
	const int32 ReturnValue = Super::AddLocalPlayer(NewPlayer, UserId);

	if (ReturnValue != INDEX_NONE)
	{
		if (UCommonLocalPlayer* CommonLocalPlayer = Cast<UCommonLocalPlayer>(NewPlayer))
		{
			if (UGameUIManagerSubsystem* UIManager = GetSubsystem<UGameUIManagerSubsystem>())
			{
				UIManager->NotifyPlayerAdded(CommonLocalPlayer);
			}
		}
	}

	return ReturnValue;
}

bool URpgGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	if (UCommonLocalPlayer* CommonLocalPlayer = Cast<UCommonLocalPlayer>(ExistingPlayer))
	{
		if (UGameUIManagerSubsystem* UIManager = GetSubsystem<UGameUIManagerSubsystem>())
		{
			UIManager->NotifyPlayerDestroyed(CommonLocalPlayer);
		}
	}

	return Super::RemoveLocalPlayer(ExistingPlayer);
}
