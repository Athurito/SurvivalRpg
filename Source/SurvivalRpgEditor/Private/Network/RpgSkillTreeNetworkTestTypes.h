#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"

#include "RpgSkillTreeNetworkTestTypes.generated.h"

/** Plain game mode whose players get the real RPG player state, so client requests run through their own connection. */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationProgressionGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationProgressionGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Records the rejections a client's skill tree component hears from the server. Test-only. */
UCLASS(NotBlueprintable, Transient)
class URpgNetworkAutomationRejectionListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleRejected(FGameplayTag TreeTag, FGameplayTag NodeTag, ERpgSkillTreeRequestRejection Reason)
	{
		NodeTags.Add(NodeTag);
		Reasons.Add(Reason);
	}

	TArray<FGameplayTag> NodeTags;
	TArray<ERpgSkillTreeRequestRejection> Reasons;
};
