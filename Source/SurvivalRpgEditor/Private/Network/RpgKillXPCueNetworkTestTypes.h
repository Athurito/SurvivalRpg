#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include "RpgKillXPCueNetworkTestTypes.generated.h"

class URpgExperienceRewardComponent;

/** Plain game mode whose players get ARpgNetworkAutomationKillerState. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationKillXPGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationKillXPGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Real RPG player state and ability system without Experience-bound PawnData; the test binds the pawn as avatar. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationKillerState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Server-side kill victim carrying the production XP reward component. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationKillVictim final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationKillVictim(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<URpgExperienceRewardComponent> Reward;
};

/**
 * Locally predicted ability whose server activation kills the first ARpgNetworkAutomationKillVictim, so the kill
 * message arrives inside the client's prediction window like a kill from a predicted attack.
 */
UCLASS(NotBlueprintable)
class URpgNetworkAutomationKillAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgNetworkAutomationKillAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
