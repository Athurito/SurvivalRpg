#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgGameFeatureAction_WorldActionBase.h"
#include "RpgGameFeatureAction_AwardKillXP.generated.h"

class ARpgPlayerState;
class UCurveTable;
class URpgExperienceRewardComponent;
struct FRpgCombatActorKilledMessage;

/**
 * Progression feature action that listens to combat kill messages and awards XP.
 */
UCLASS(meta = (DisplayName = "Award Rpg Kill XP"))
class SURVIVALRPG_API URpgGameFeatureAction_AwardKillXP final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

	UPROPERTY(EditAnywhere, Category = "XP", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UCurveTable> EnemyXPRewardCurveTable;

private:
	struct FPerContextData
	{
		TArray<FGameplayMessageListenerHandle> ListenerHandles;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	void Reset(FPerContextData& ActiveData);
	void HandleActorKilled(FGameplayTag Channel, const FRpgCombatActorKilledMessage& Message);
	float ResolveXPReward(const URpgExperienceRewardComponent* RewardComponent) const;
	ARpgPlayerState* ResolveKillerPlayerState(const FRpgCombatActorKilledMessage& Message) const;
	ARpgPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;

	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> LoadedEnemyXPRewardCurveTable;
};
