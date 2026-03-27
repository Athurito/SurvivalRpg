// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameInstance.h"

#include "Components/GameFrameworkComponentManager.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"

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
