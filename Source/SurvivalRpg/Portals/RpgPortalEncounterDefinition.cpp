#include "RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalEncounterDefinition)

#define LOCTEXT_NAMESPACE "RpgPortalEncounterDefinition"

URpgPortalEncounterDefinition::URpgPortalEncounterDefinition()
{
	EnterInteractionText = LOCTEXT("EnterPortal", "Enter Portal");
	EnterInteractionSubText = LOCTEXT("EnterPortalSubText", "Cross into the rift");
	ExitInteractionText = LOCTEXT("ExitPortal", "Exit Portal");
	ExitInteractionSubText = LOCTEXT("ExitPortalSubText", "Return to the overworld");
	CloseInteractionText = LOCTEXT("ClosePortal", "Close Portal");
	CloseInteractionSubText = LOCTEXT("ClosePortalSubText", "Stabilize the rift");
}

#undef LOCTEXT_NAMESPACE
