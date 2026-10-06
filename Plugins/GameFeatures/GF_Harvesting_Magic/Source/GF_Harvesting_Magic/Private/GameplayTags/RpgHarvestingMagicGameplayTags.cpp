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
		Ability_ActivateFail_Harvesting_SkillLevel,
		"Ability.ActivateFail.Harvesting.SkillLevel",
		"A harvest ability is locked until its required trade-skill level is reached.");

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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_AreaRadius,
		"Ability.Tuning.Harvest.AreaRadius",
		"Area radius in cm of an area harvest around its aim point; the preview ring follows it.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_Reach,
		"Ability.Tuning.Harvest.Reach",
		"Farthest distance in cm from the harvester to its hit or aim point; the aim ray grows by the same amount.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_MaxTargets,
		"Ability.Tuning.Harvest.MaxTargets",
		"Most targets an area harvest takes, rounded to a whole number of at least one.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_Sections,
		"Ability.Tuning.Harvest.Sections",
		"Stock sections a harvest takes per target, or a swarm creature per strike; rounded, at least one.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_Creatures,
		"Ability.Tuning.Harvest.Creatures",
		"Creatures a swarm summons, rounded and kept between 1 and 16.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_StrikeInterval,
		"Ability.Tuning.Harvest.StrikeInterval",
		"Seconds a swarm creature rests after each strike.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_StrikeRadius,
		"Ability.Tuning.Harvest.StrikeRadius",
		"Radius in cm around each swarm strike in which every other resource of the swarm is struck once too.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Tuning_Harvest_Cooldown,
		"Ability.Tuning.Harvest.Cooldown",
		"Duration in seconds of a harvest ability's cooldown effect.");
}
