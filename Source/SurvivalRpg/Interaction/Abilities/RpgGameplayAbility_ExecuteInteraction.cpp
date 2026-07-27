// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGameplayAbility_ExecuteInteraction.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_ExecuteInteraction)

URpgGameplayAbility_ExecuteInteraction::URpgGameplayAbility_ExecuteInteraction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_ExecuteInteraction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	FInteractionOption ValidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!ActorInfo || !UInteractionStatics::ValidateInteractionEventData(
			*ActorInfo,
			TriggerEventData,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Instigator = ActorInfo->AvatarActor.Get();
	const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo) &&
		ValidatedOption.InteractableTarget &&
		ValidatedOption.InteractableTarget->CommitInteraction(AuthoritativeQuery, ValidatedOption);

	UInteractionStatics::BroadcastInteractionMessage(
		this,
		bCommitted
			? RpgGameplayTags::Rpg_Interaction_Message_Ended
			: RpgGameplayTags::Rpg_Interaction_Message_Rejected,
		ValidatedOption,
		Instigator,
		bCommitted);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bCommitted);
}
