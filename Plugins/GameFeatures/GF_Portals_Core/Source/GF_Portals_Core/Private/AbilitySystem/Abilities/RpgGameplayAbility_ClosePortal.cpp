#include "AbilitySystem/Abilities/RpgGameplayAbility_ClosePortal.h"

#include "Portals/RpgPortalActor.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_ClosePortal)

URpgGameplayAbility_ClosePortal::URpgGameplayAbility_ClosePortal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_ClosePortal::ActivateAbility(
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

	AActor* ClosingActor = ActorInfo->AvatarActor.Get();
	ARpgPortalActor* Portal = Cast<ARpgPortalActor>(ValidatedOption.TargetRef.TargetActor.Get());
	if (!Portal || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UInteractionStatics::BroadcastInteractionMessage(this, RpgGameplayTags::Rpg_Interaction_Message_Rejected, ValidatedOption, ClosingActor, false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bClosed = Portal && Portal->TryClosePortal(ClosingActor);
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		bClosed ? RpgGameplayTags::Rpg_Interaction_Message_Ended : RpgGameplayTags::Rpg_Interaction_Message_Rejected,
		ValidatedOption,
		ClosingActor,
		bClosed);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, !bClosed);
}

ARpgPortalActor* URpgGameplayAbility_ClosePortal::ResolvePortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const
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
