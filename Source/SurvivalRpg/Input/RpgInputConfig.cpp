// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgInputConfig.h"

namespace
{
FGameplayTag ResolveGameplayTag(FName TagName)
{
	return TagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(TagName);
}
}

URpgInputConfig::URpgInputConfig(const FObjectInitializer& ObjectInitializer)
{
}

const UInputAction* URpgInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRpgInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* URpgInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRpgInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

void URpgInputConfig::ClearAbilityInputActions()
{
	AbilityInputActions.Reset();
}

void URpgInputConfig::AddAbilityInputActionByTagName(const UInputAction* InputAction, FName InputTagName)
{
	FRpgInputAction& NewAction = AbilityInputActions.AddDefaulted_GetRef();
	NewAction.InputAction = InputAction;
	NewAction.InputTag = ResolveGameplayTag(InputTagName);
}
