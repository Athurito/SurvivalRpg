#include "GameplayTags/RpgPortalGameplayTags.h"

namespace RpgPortalGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Interaction_Action_Portal_Enter, "Rpg.Interaction.Action.Portal.Enter", "Enter a server-authoritative portal realm.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Interaction_Action_Portal_Exit, "Rpg.Interaction.Action.Portal.Exit", "Exit a completed portal realm.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Interaction_Action_Portal_Close, "Rpg.Interaction.Action.Portal.Close", "Close a sealable portal encounter.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Portal_Message_Completed, "Rpg.Portal.Message.Completed", "Gameplay message sent when a portal encounter is completed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Portal_Encounter, "Feature.Portal.Encounter", "Placed feature region and encounter point tag for portal encounters.");
}
