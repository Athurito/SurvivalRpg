#include "RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalEncounterDefinition)

#define LOCTEXT_NAMESPACE "RpgPortalEncounterDefinition"

URpgPortalEncounterDefinition::URpgPortalEncounterDefinition()
{
	DungeonEntryTransform = FTransform(FRotator::ZeroRotator, FVector(10000.0f, 0.0f, 250.0f));
	DungeonBossSpawnTransform = FTransform(FRotator::ZeroRotator, FVector(10600.0f, 0.0f, 250.0f));
	DungeonExitSpawnTransform = FTransform(FRotator::ZeroRotator, FVector(11200.0f, 0.0f, 250.0f));
	DungeonLevelInstanceTransform = FTransform(FRotator::ZeroRotator, FVector(10000.0f, 0.0f, 0.0f));
	OverworldReturnTransformOffset = FTransform(FRotator::ZeroRotator, FVector(250.0f, 0.0f, 0.0f));

	EnterInteractionText = LOCTEXT("EnterPortal", "Enter Portal");
	EnterInteractionSubText = LOCTEXT("EnterPortalSubText", "Cross into the rift");
	ExitInteractionText = LOCTEXT("ExitPortal", "Exit Portal");
	ExitInteractionSubText = LOCTEXT("ExitPortalSubText", "Return to the overworld");
	CloseInteractionText = LOCTEXT("ClosePortal", "Close Portal");
	CloseInteractionSubText = LOCTEXT("ClosePortalSubText", "Stabilize the rift");
}

#undef LOCTEXT_NAMESPACE
