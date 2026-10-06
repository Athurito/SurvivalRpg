#pragma once

#include "NativeGameplayTags.h"

namespace RpgHarvestingMagicGameplayTags
{
	/** Root tag shared by all magical and manual harvesting abilities in this feature. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting);

	/** Stable ability id supplied when the generic interaction ability manually harvests a resource instance. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Manual);

	/** Activation failure reported when a harvest ability's trade-skill unlock level is not reached. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_ActivateFail_Harvesting_SkillLevel);

	/** Server-only corpse-processing ability started through the interaction system. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Skinning);

	/** Semantic interaction action used to validate manual resource-instance harvesting on the server. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rpg_Interaction_Action_Harvest_Manual);

	/** Semantic interaction action used to reserve and process an available corpse. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rpg_Interaction_Action_Harvest_Corpse);

	/** Root tag for physical tools accepted by harvesting definitions. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Harvesting);

	/** Tool category required by animal-corpse skinning profiles. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tool_Harvesting_Skinning);

	/** Montage event at which a reserved corpse reward commits authoritatively. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayEvent_Harvesting_Commit);

	/** Persistent cosmetic cue that presents the automatically selected skinning tool. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Harvesting_Skinning_Tool);

	/** External corpse-lifecycle requirement completed after successful reward delivery. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rpg_Corpse_Completion_Harvest);

	/** Skill tree tuning of an area harvest's radius around its aim point, in centimeters. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_AreaRadius);

	/** Skill tree tuning of the farthest distance from the harvester to its hit or aim point, in centimeters. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_Reach);

	/** Skill tree tuning of the most targets an area harvest takes. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_MaxTargets);

	/** Skill tree tuning of the stock sections taken per target, or per creature strike of a swarm. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_Sections);

	/** Skill tree tuning of the creatures a swarm summons. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_Creatures);

	/** Skill tree tuning of the rest of a swarm creature after each strike, in seconds. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_StrikeInterval);

	/** Skill tree tuning of the radius around a swarm strike in which the other swarm targets are struck too, in centimeters. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_StrikeRadius);

	/** Skill tree tuning of a harvest ability's cooldown duration, in seconds. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_Cooldown);
}
