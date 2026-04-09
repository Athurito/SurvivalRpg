// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameplayAbility.h"

#include "SurvivalRpg/Core/Player/RpgPlayerController.h"


URpgGameplayAbility::URpgGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag("GameplayAbility.Active"));
}

ARpgPlayerController* URpgGameplayAbility::GetRpgPlayerControllerFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<ARpgPlayerController>(CurrentActorInfo->PlayerController.Get()) : nullptr);
}

bool URpgGameplayAbility::HasPlayerController() const
{
	const APawn* PawnObject = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!PawnObject) return false;
	
	return PawnObject->IsPlayerControlled();
}
