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
		Ability_Harvesting_Skinning,
		"Ability.Harvesting.Skinning",
		"Server-only ability that processes a reserved animal corpse with a valid skinning tool.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Rpg_Interaction_Action_Harvest_Manual,
		"Rpg.Interaction.Action.Harvest.Manual",
		"Semantic action used by a player to manually harvest one resource-mesh instance.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Rpg_Interaction_Action_Harvest_Corpse,
		"Rpg.Interaction.Action.Harvest.Corpse",
		"Semantic action used by a player to reserve and process one available corpse.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Tool_Harvesting,
		"Tool.Harvesting",
		"Root tag for physical harvesting tool categories.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Tool_Harvesting_Skinning,
		"Tool.Harvesting.Skinning",
		"Tool category accepted by animal-corpse skinning profiles.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayEvent_Harvesting_Commit,
		"GameplayEvent.Harvesting.Commit",
		"Authoritative montage notify at which a reserved harvest produces its reward.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		GameplayCue_Harvesting_Skinning_Tool,
		"GameplayCue.Harvesting.Skinning.Tool",
		"Persistent cosmetic presentation of the selected skinning tool during corpse harvesting.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Rpg_Corpse_Completion_Harvest,
		"Rpg.Corpse.Completion.Harvest",
		"External corpse-lifecycle requirement fulfilled after a harvest reward is delivered.");
}
