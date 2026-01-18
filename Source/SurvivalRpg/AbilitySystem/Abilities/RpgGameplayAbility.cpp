// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameplayAbility.h"

URpgGameplayAbility::URpgGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
}
