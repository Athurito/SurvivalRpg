#pragma once

#include "NativeGameplayTags.h"

namespace RpgHarvestingMagicGameplayTags
{
	/** Root tag shared by all magical and manual harvesting abilities in this feature. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting);

	/** Stable ability id supplied when the generic interaction ability manually harvests a resource instance. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Manual);

	/** Stable quick-access/progression id for the Stoneburst harvesting spell. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Harvesting_Stoneburst);

	/** Semantic interaction action used to validate manual resource-instance harvesting on the server. */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rpg_Interaction_Action_Harvest_Manual);
}
