#include "AbilitySystem/Abilities/RpgGameplayAbility_EnterPortal.h"

#include "Portals/RpgPortalActor.h"

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

	ARpgPortalActor* Portal = ResolvePortalTarget(ActorInfo, TriggerEventData);
	AActor* EnteringActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Instigator)) : nullptr;
	if (!EnteringActor && ActorInfo->AvatarActor.IsValid())
	{
		EnteringActor = ActorInfo->AvatarActor.Get();
	}

	const bool bEntered = Portal && Portal->TryEnterPortal(EnteringActor);
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
