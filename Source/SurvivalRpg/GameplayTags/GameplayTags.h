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
	
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);
}
