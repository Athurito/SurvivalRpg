#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_EnterPortal.generated.h"

class ARpgPortalActor;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_EnterPortal : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_EnterPortal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	ARpgPortalActor* ResolvePortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const;
};
