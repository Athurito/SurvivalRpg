#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "RpgGameplayAbility_ExitPortal.generated.h"

class ARpgPortalExitActor;

/**
 * Generic interaction ability for using a spawned dungeon exit portal.
 *
 * The spawned ARpgPortalExitActor forwards to its owning overworld portal so the
 * authoritative portal flow owns return teleporting and sealable-state updates.
 */
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

	/** Resolves the exit portal from interaction event data or ability actor context. */
	ARpgPortalExitActor* ResolveExitPortalTarget(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* TriggerEventData) const;
};
