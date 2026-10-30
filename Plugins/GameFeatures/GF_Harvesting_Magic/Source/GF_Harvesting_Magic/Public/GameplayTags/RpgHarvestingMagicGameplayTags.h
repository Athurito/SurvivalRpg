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

	/** Skill tree tuning of the work multiplier of every hit of a harvest ability; base 1. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_Work);

	/** Skill tree tuning of the multiplier on a weak-point hit's bonus work; base 1. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_WeakPointWork);

	/** Skill tree tuning of the work multiplier of the next hit on a target after a weak-point hit on it; base 1. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_FollowUpWork);

	/** Skill tree tuning of how long, in seconds, that follow-up waits for the next hit; base 0 turns it off. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_FollowUpWindow);

	/**
	 * Rune tuning of the burn a hit lights: work per second as a fraction of the hit's own work, for example 0.1 for
	 * 10 %; base 0 lights none.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_BurnWork);

	/** Rune tuning of how long, in seconds, a lit burn keeps working its resource; base 0 lights none. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_BurnDuration);

	/** Skill tree tuning: above 0, a further hit on a resource the player burns restarts the burn's time; base 0. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_BurnRefresh);

	/**
	 * Skill tree tuning: above 0, a further hit on a resource the player burns releases the burn's remaining work at
	 * once, times this value, and puts it out; base 0.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_BurnDischarge);

	/**
	 * Rune tuning of the echo every few own hits send onto the struck resource: its work as a fraction of the hit's own
	 * work, for example 0.5 for 50 %; base 0 echoes nothing.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_EchoWork);

	/** Skill tree tuning of how many accepted own hits charge one echo; base from the ability, usually 3. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_EchoHits);

	/**
	 * Rune tuning of the share of a depleting hit's actual overkill that carries to the nearest other resource of the
	 * tool, for example 0.5 for 50 %; base 0 carries nothing.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_TransferWork);

	/**
	 * Skill tree tuning: above 0, echoes and transfers also strike a second resource of the tool nearby, and each
	 * strike does this fraction of the work, for example 0.6; base 0 strikes one resource.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_RuneFlowSplit);

	/**
	 * Rune tuning of the leap every own hit sends on from resource to resource of the tool, such as Erntesprung: each
	 * leap's work as a fraction of the hit's own work, for example 0.3 for 30 %; base 0 leaps nowhere.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_LeapWork);

	/**
	 * Rune tuning of the strike every own hit drives through to the next resource behind the struck one, such as
	 * Durchschlag: its work as a fraction of the hit's own work, for example 0.5 for 50 %; base 0 pierces nothing.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_PierceWork);

	/** Skill tree tuning of how far leaps, pierces and wandering burns reach, as a multiplier; base 1. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_SpreadReach);

	/**
	 * Skill tree tuning: above 0, every leap step and every pierce strikes two resources, each with this fraction of the
	 * work, for example 0.6; base 0 strikes one.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_SpreadSplit);

	/**
	 * Skill tree tuning: above 0, a burn whose resource runs empty moves on to the nearest other resource of the tool
	 * within the leap's reach and burns there for its remaining time; base 0.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_BurnWander);

	/**
	 * Rune tuning of the dead workers every few own hits raise, such as Totenarbeiter: each worker strike's work as a
	 * fraction of the hit's own work, for example 0.4 for 40 %; base 0 raises none.
	 */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_WorkerWork);

	/** Skill tree tuning of how many workers rise at once, 1 to 4; base from the ability's worker swarm, usually 1. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_WorkerCount);

	/** Skill tree tuning of how many accepted own hits raise the workers; base from the ability, usually 5. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_WorkerHits);

	/** Skill tree tuning of how many seconds the workers keep working; base from the ability's worker swarm. */
	GF_HARVESTING_MAGIC_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tuning_Harvest_WorkerLifetime);
}
