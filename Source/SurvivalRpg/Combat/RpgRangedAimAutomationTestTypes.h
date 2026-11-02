#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Combat/RpgRangedAim.h"

#include "RpgRangedAimAutomationTestTypes.generated.h"

class URpgAbilityTask_RangedAim;

/**
 * Held aim for the ranged aim tests: an independent, locally predicted ability that holds Hold Ranged Aim with its
 * definition while its input is held, as GA_BowAim does, and records interruptions.
 */
UCLASS(NotBlueprintable, Transient)
class URpgRangedAimAutomationAbility : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgRangedAimAutomationAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FRpgRangedAimDefinition AimDefinition;

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilityTask_RangedAim> AimTask;

	int32 InterruptedCount = 0;
	FName LastInterruption;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void HandleInterrupted(FName Reason);
};

/** A drawn shot's aim for the tests: the same stance in the exclusive replaceable group, like GA_BowShot. */
UCLASS(NotBlueprintable, Transient)
class URpgRangedAimAutomationDrawAbility final : public URpgRangedAimAutomationAbility
{
	GENERATED_BODY()

public:
	explicit URpgRangedAimAutomationDrawAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** An exclusive ability without the ranged aim tag that stays active until cancelled, like a dodge or an attack. */
UCLASS(NotBlueprintable, Transient)
class URpgRangedAimAutomationExclusiveAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgRangedAimAutomationExclusiveAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** An independent ability that turns exclusive after it started, as stagger and death do. */
UCLASS(NotBlueprintable, Transient)
class URpgRangedAimAutomationSwitchingAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgRangedAimAutomationSwitchingAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** An independent ability that stays active until cancelled, to hold leases such as a sprint in the tests. */
UCLASS(NotBlueprintable, Transient)
class URpgRangedAimAutomationIdleAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgRangedAimAutomationIdleAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
