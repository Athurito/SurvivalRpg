#include "Network/RpgSkillTreeNetworkTestTypes.h"

#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgSkillTreeNetworkTestTypes)

ARpgNetworkAutomationProgressionGameMode::ARpgNetworkAutomationProgressionGameMode(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The harvester fixture keeps the real progression components but skips Experience-bound PawnData grants.
	PlayerStateClass = ARpgNetworkAutomationHarvesterState::StaticClass();
}
