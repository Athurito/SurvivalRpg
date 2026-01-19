// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameplayAbility.h"

URpgGameplayAbility::URpgGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag("GameplayAbility.Active"));

}

bool URpgGameplayAbility::HasPlayerController() const
{
	const APawn* PawnObject = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (!PawnObject) return false;
	
	return PawnObject->IsPlayerControlled();
}
