#include "AbilitySystem/Abilities/RpgGameplayAbility_ExitPortal.h"

#include "Portals/RpgPortalExitActor.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_ExitPortal)

URpgGameplayAbility_ExitPortal::URpgGameplayAbility_ExitPortal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_ExitPortal::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	FInteractionOption ValidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() ||
		!UInteractionStatics::ValidateInteractionEventData(
			*ActorInfo,
			TriggerEventData,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* ExitingActor = ActorInfo->AvatarActor.Get();
	ARpgPortalExitActor* ExitPortal = Cast<ARpgPortalExitActor>(ValidatedOption.TargetRef.TargetActor.Get());
	if (!ExitPortal || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UInteractionStatics::BroadcastInteractionMessage(this, RpgGameplayTags::Rpg_Interaction_Message_Rejected, ValidatedOption, ExitingActor, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bExited = ExitPortal && ExitPortal->TryUseExitPortal(ExitingActor);
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		bExited ? RpgGameplayTags::Rpg_Interaction_Message_Ended : RpgGameplayTags::Rpg_Interaction_Message_Rejected,
		ValidatedOption,
		ExitingActor,
		bExited);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bExited);
}

ARpgPortalExitActor* URpgGameplayAbility_ExitPortal::ResolveExitPortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		if (ARpgPortalExitActor* ExitPortal = Cast<ARpgPortalExitActor>(const_cast<AActor*>(ToRawPtr(TriggerEventData->Target))))
		{
			return ExitPortal;
		}
	}

	if (ActorInfo)
	{
		if (ARpgPortalExitActor* ExitPortal = Cast<ARpgPortalExitActor>(ActorInfo->AvatarActor.Get()))
		{
			return ExitPortal;
		}

		if (ARpgPortalExitActor* ExitPortal = Cast<ARpgPortalExitActor>(ActorInfo->OwnerActor.Get()))
		{
			return ExitPortal;
		}
	}

	return nullptr;
}
