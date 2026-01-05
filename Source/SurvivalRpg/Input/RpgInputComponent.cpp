// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgInputComponent.h"

class URpgInputConfig;

URpgInputComponent::URpgInputComponent(const FObjectInitializer& ObjectInitializer)
{
}

void URpgInputComponent::AddInputMappings(const URpgInputConfig* InputConfig,
                                          UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputSubsystem);
	check(InputConfig);
}

void URpgInputComponent::RemoveInputMappings(const URpgInputConfig* InputConfig,
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputSubsystem);
	check(InputConfig);
}


void URpgInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}

