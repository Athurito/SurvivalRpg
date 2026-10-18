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

	/** Designer-authored XP per enemy level, one curve per reward row of URpgExperienceRewardComponent. Server only. */
	UPROPERTY(EditAnywhere, Category = "XP", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UCurveTable> EnemyXPRewardCurveTable;

	/**
	 * Cosmetic GameplayCue the server executes on the killer's ability system when a kill awards XP. The cue
	 * parameters carry the victim's location in Location and the awarded XP in RawMagnitude. Every client with the
	 * killer's player state and character plays it on the character, the killer's own client included. Empty means
	 * no cue.
	 */
	UPROPERTY(EditAnywhere, Category = "XP|Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag XPAwardGameplayCueTag;

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
	void ExecuteXPAwardCue(ARpgPlayerState& KillerPlayerState, AActor& Victim, float XPReward) const;
	ARpgPlayerState* ResolveKillerPlayerState(const FRpgCombatActorKilledMessage& Message) const;
	ARpgPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;

	UPROPERTY(Transient)
	TObjectPtr<UCurveTable> LoadedEnemyXPRewardCurveTable;
};
