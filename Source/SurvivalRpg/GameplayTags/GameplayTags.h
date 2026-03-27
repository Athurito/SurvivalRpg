// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RpgGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);      
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Mouse);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look_Stick);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Crouch);    
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_AutoRun);   
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Jump);   
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_StopJump);   
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Reset);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_RequestReset);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Downed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Revive);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Downed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Downed_BleedingOut);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Downed_Reviving);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_CannotBeRevived);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Crouching);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_AutoRunning);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dying);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Death_Dead);
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_GodMode);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cheat_UnlimitedHealth);
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);
	
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Behavior_SurvivesDeath);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Respawn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_BleedoutExpired);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Dead_WaitingForRespawn);
}
