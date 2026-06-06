#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_ExitPortal.generated.h"

class ARpgPortalExitActor;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_ExitPortal : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_ExitPortal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	ARpgPortalExitActor* ResolveExitPortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const;
};
