#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"

namespace RpgHarvestingMagicGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Harvesting,
		"Ability.Harvesting",
		"Root tag for harvesting ability ids owned by GF_Harvesting_Magic.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Harvesting_Manual,
		"Ability.Harvesting.Manual",
		"Stable id for a manual harvest committed through the generic interaction ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Harvesting_Stoneburst,
		"Ability.Harvesting.Stoneburst",
		"Stable progression and quick-access id for the Stoneburst magical harvesting ability.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Rpg_Interaction_Action_Harvest_Manual,
		"Rpg.Interaction.Action.Harvest.Manual",
		"Semantic action used by a player to manually harvest one resource-mesh instance.");
}
