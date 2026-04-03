#include "RpgGameplayAbility_ActivateWeaponSet.h"

#include "SurvivalRpg/Equipment/RpgEquipmentComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

URpgGameplayAbility_ActivateWeaponSet::URpgGameplayAbility_ActivateWeaponSet()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URpgGameplayAbility_ActivateWeaponSet::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (const AActor* OwnerActor = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr)
	{
		if (const ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(OwnerActor))
		{
			if (URpgEquipmentComponent* EquipmentComponent = PlayerState->GetEquipmentComponent())
			{
				EquipmentComponent->TryActivateWeaponSet(WeaponSetIndex);
			}
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
