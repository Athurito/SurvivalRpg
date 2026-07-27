#include "AbilitySystem/Abilities/RpgGameplayAbility_EnterPortal.h"

#include "Portals/RpgPortalActor.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_EnterPortal)

URpgGameplayAbility_EnterPortal::URpgGameplayAbility_EnterPortal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_EnterPortal::ActivateAbility(
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

	AActor* EnteringActor = ActorInfo->AvatarActor.Get();
	ARpgPortalActor* Portal = Cast<ARpgPortalActor>(ValidatedOption.TargetRef.TargetActor.Get());
	if (!Portal || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UInteractionStatics::BroadcastInteractionMessage(this, RpgGameplayTags::Rpg_Interaction_Message_Rejected, ValidatedOption, EnteringActor, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bEntered = Portal && Portal->TryEnterPortal(EnteringActor);
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		bEntered ? RpgGameplayTags::Rpg_Interaction_Message_Ended : RpgGameplayTags::Rpg_Interaction_Message_Rejected,
		ValidatedOption,
		EnteringActor,
		bEntered);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bEntered);
}

ARpgPortalActor* URpgGameplayAbility_EnterPortal::ResolvePortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		if (ARpgPortalActor* Portal = Cast<ARpgPortalActor>(const_cast<AActor*>(ToRawPtr(TriggerEventData->Target))))
		{
			return Portal;
		}
	}

	if (ActorInfo)
	{
		if (ARpgPortalActor* Portal = Cast<ARpgPortalActor>(ActorInfo->AvatarActor.Get()))
		{
			return Portal;
		}

		if (ARpgPortalActor* Portal = Cast<ARpgPortalActor>(ActorInfo->OwnerActor.Get()))
		{
			return Portal;
		}
	}

	return nullptr;
}
