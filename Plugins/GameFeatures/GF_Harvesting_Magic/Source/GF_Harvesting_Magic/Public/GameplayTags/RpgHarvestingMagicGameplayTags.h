#pragma once

#include "NativeGameplayTags.h"

namespace RpgHarvestingMagicGameplayTags
{
	/** Root tag shared by all magical and manual harvesting abilities in this feature. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting);

	/** Stable ability id supplied when the generic interaction ability manually harvests a resource instance. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Manual);

	/** Stable quick-access/progression id for the Stoneburst harvesting spell. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Stoneburst);

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
}
