#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_EnterPortal.generated.h"

class ARpgPortalActor;

/**
 * Generic interaction ability for entering a dungeon portal.
 *
 * The portal owns the streaming, marker resolution and teleport authority; this
 * ability only resolves the interacted portal target and forwards the server
 * request.
 */
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

	/** Resolves the portal from interaction event data or ability actor context. */
	ARpgPortalActor* ResolvePortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const;
};
