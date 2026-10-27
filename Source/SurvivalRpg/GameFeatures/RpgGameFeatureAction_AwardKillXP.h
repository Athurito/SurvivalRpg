#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RpgGameFeatureAction_WorldActionBase.h"
#include "RpgGameFeatureAction_AwardKillXP.generated.h"

class ARpgPlayerState;
class UCurveTable;
class URpgExperienceRewardComponent;
struct FRpgCombatActorKilledMessage;

/** One skill a kill advances besides character XP, for example a combat mastery. Designer-tuned static data. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgKillSkillXPReward
{
	GENERATED_BODY()

	/** Trade skill that earns XP from kills, for example Skill.Mastery.Combat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "XP", meta = (Categories = "Skill"))
	FGameplayTag SkillTag;

	/** Skill XP per point of the kill's character XP; 1 awards the same amount. Zero awards nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "XP", meta = (ClampMin = "0", UIMin = "0", UIMax = "5"))
	float XPScale = 1.0f;
};

/**
 * Progression feature action that listens to combat kill messages and awards the killer character XP and the
 * configured skill XP.
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
	 * Trade skills every rewarded kill also advances, scaled from the kill's character XP. The server adds the XP to the
	 * killer once per kill, together with the character XP; kills that award no character XP award none either.
	 */
	UPROPERTY(EditAnywhere, Category = "XP", meta = (TitleProperty = "SkillTag"))
	TArray<FRpgKillSkillXPReward> SkillXPRewards;

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
