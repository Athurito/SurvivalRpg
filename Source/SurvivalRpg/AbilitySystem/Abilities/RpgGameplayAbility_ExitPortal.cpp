#include "RpgGameplayAbility_ExitPortal.h"

#include "SurvivalRpg/Portals/RpgPortalExitActor.h"

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

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ARpgPortalExitActor* ExitPortal = ResolveExitPortalTarget(ActorInfo, TriggerEventData);
	AActor* ExitingActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Instigator)) : nullptr;
	if (!ExitingActor && ActorInfo->AvatarActor.IsValid())
	{
		ExitingActor = ActorInfo->AvatarActor.Get();
	}

	const bool bExited = ExitPortal && ExitPortal->TryUseExitPortal(ExitingActor);
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
