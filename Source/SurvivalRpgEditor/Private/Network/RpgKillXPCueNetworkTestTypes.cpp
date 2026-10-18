#include "Network/RpgKillXPCueNetworkTestTypes.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "SurvivalRpg/Progression/RpgExperienceRewardComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgKillXPCueNetworkTestTypes)

ARpgNetworkAutomationKillXPGameMode::ARpgNetworkAutomationKillXPGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStateClass = ARpgNetworkAutomationKillerState::StaticClass();
}

void ARpgNetworkAutomationKillerState::PostInitializeComponents()
{
	// Skip the Experience-bound PawnData grant of ARpgPlayerState, but keep the real ASC lifecycle.
	ARpgBasePlayerState::PostInitializeComponents();
}

ARpgNetworkAutomationKillVictim::ARpgNetworkAutomationKillVictim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	Reward = CreateDefaultSubobject<URpgExperienceRewardComponent>(TEXT("Reward"));
}

URpgNetworkAutomationKillAbility::URpgNetworkAutomationKillAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URpgNetworkAutomationKillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UWorld* World = GetWorld();
	if (World && ActorInfo && ActorInfo->IsNetAuthority())
	{
		TActorIterator<ARpgNetworkAutomationKillVictim> Victim(World);
		if (Victim)
		{
			FRpgCombatActorKilledMessage Message;
			Message.Victim = *Victim;
			Message.Killer = ActorInfo->OwnerActor.Get();
			Message.DamageMagnitude = 100.0f;
			UGameplayMessageSubsystem::Get(World).BroadcastMessage(
				FGameplayTag::RequestGameplayTag(TEXT("Rpg.Combat.Message.ActorKilled")), Message);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
