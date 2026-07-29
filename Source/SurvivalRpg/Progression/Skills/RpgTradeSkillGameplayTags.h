#pragma once

#include "NativeGameplayTags.h"

namespace RpgTradeSkillGameplayTags
{
	/** Use-based gathering skill for plants, berries, herbs, and other forage. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Gathering_Foraging);

	/** Use-based gathering skill for trees and other wood-producing resources. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Gathering_Logging);

	/** Use-based gathering skill for stone, ore, and mineral resources. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Gathering_Mining);

	/** Use-based gathering skill for recovering meat, hide, bone, and essences from animal corpses. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Gathering_Skinning);

	/** Processing skill for turning timber into crafted components and equipment. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Crafting_Woodworking);

	/** Processing skill for forging metal components, tools, weapons, and armor. */
	SURVIVALRPG_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Crafting_Blacksmithing);
}
