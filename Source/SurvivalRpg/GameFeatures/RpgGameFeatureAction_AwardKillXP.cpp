#include "RpgGameFeatureAction_AwardKillXP.h"

#include "Engine/CurveTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Progression/RpgExperienceRewardComponent.h"
#include "SurvivalRpg/Progression/Player/RpgPlayerProgressionComponent.h"

void URpgGameFeatureAction_AwardKillXP::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);
	if (!ensure(ActiveData.ListenerHandles.IsEmpty()))
	{
		Reset(ActiveData);
	}

	LoadedEnemyXPRewardCurveTable = EnemyXPRewardCurveTable.IsNull() ? nullptr : EnemyXPRewardCurveTable.LoadSynchronous();

	Super::OnGameFeatureActivating(Context);
}

void URpgGameFeatureAction_AwardKillXP::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}

	LoadedEnemyXPRewardCurveTable = nullptr;
}

void URpgGameFeatureAction_AwardKillXP::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	ActiveData.ListenerHandles.Add(MessageSubsystem.RegisterListener<FRpgCombatActorKilledMessage>(
		RpgGameplayTags::Rpg_Combat_Message_ActorKilled,
		this,
		&ThisClass::HandleActorKilled));
}

void URpgGameFeatureAction_AwardKillXP::Reset(FPerContextData& ActiveData)
{
	for (FGameplayMessageListenerHandle& Handle : ActiveData.ListenerHandles)
	{
		Handle.Unregister();
	}

	ActiveData.ListenerHandles.Reset();
}

void URpgGameFeatureAction_AwardKillXP::HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message)
{
	if (!Message.Victim || Message.bWasSelfDestruct || Message.Killer == Message.Victim)
	{
		return;
	}

	const URpgExperienceRewardComponent* RewardComponent = Message.Victim->FindComponentByClass<URpgExperienceRewardComponent>();
	const float XPReward = ResolveXPReward(RewardComponent);
	if (XPReward <= 0.0f)
	{
		return;
	}

	ARpgPlayerState* KillerPlayerState = ResolveKillerPlayerState(Message);
	if (!KillerPlayerState || !KillerPlayerState->HasAuthority())
	{
		return;
	}

	if (URpgPlayerProgressionComponent* ProgressionComponent = KillerPlayerState->GetPlayerProgressionComponent())
	{
		ProgressionComponent->AddXP(XPReward);
	}
}

float URpgGameFeatureAction_AwardKillXP::ResolveXPReward(const URpgExperienceRewardComponent* RewardComponent) const
{
	if (!RewardComponent)
	{
		return 0.0f;
	}

	const FName RowName = RewardComponent->GetXPRewardRowName();
	if (LoadedEnemyXPRewardCurveTable && RowName != NAME_None)
	{
		const FString ContextString = FString::Printf(TEXT("%s resolving enemy XP reward"), *GetNameSafe(this));
		if (const FRealCurve* RewardCurve = LoadedEnemyXPRewardCurveTable->FindCurve(RowName, ContextString, false))
		{
			return RewardCurve->Eval(static_cast<float>(RewardComponent->GetEnemyLevel()));
		}
	}

	return RewardComponent->GetFallbackXPReward();
}

ARpgPlayerState* URpgGameFeatureAction_AwardKillXP::ResolveKillerPlayerState(const FRpgCombatActorKilledMessage& Message) const
{
	if (ARpgPlayerState* PlayerState = ResolvePlayerStateFromActor(Message.Killer))
	{
		return PlayerState;
	}

	if (ARpgPlayerState* PlayerState = ResolvePlayerStateFromActor(Message.DamageCauser))
	{
		return PlayerState;
	}

	return nullptr;
}

ARpgPlayerState* URpgGameFeatureAction_AwardKillXP::ResolvePlayerStateFromActor(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (ARpgPlayerState* PlayerState = Cast<ARpgPlayerState>(Actor))
	{
		return PlayerState;
	}

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn->GetPlayerState<ARpgPlayerState>();
	}

	if (AController* Controller = Cast<AController>(Actor))
	{
		return Controller->GetPlayerState<ARpgPlayerState>();
	}

	return nullptr;
}
