#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_ClosePortal.generated.h"

class ARpgPortalActor;

/**
 * Generic interaction ability for closing a sealable portal.
 *
 * The interactable portal supplies itself through GameplayEventData; the ability
 * stays content-agnostic and only asks the target ARpgPortalActor to close on the
 * server, matching the Lyra-style "option grants ability" interaction flow.
 */
UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_ClosePortal : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_ClosePortal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Resolves the portal from interaction event data or ability actor context. */
	ARpgPortalActor* ResolvePortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const;
};
