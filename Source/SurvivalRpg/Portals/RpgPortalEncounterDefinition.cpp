#include "RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalEncounterDefinition)

#define LOCTEXT_NAMESPACE "RpgPortalEncounterDefinition"

URpgPortalEncounterDefinition::URpgPortalEncounterDefinition()
{
	CloseInteractionText = LOCTEXT("ClosePortal", "Close Portal");
	CloseInteractionSubText = LOCTEXT("ClosePortalSubText", "Stabilize the rift");
}

#undef LOCTEXT_NAMESPACE
