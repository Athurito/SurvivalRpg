// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameplayTags.h"

namespace RpgGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead", "Ability failed to activate because its owner is dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Ability failed to activate because it is on cool down.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "Ability failed to activate because it did not pass the cost checks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "Ability failed to activate because tags are blocking it.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "Ability failed to activate because tags are missing.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_Networking, "Ability.ActivateFail.Networking", "Ability failed to activate because it did not pass the network checks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "Ability failed to activate because of its activation group.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Move input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Look (mouse) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Stick, "InputTag.Look.Stick", "Look (stick) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_AutoRun, "InputTag.AutoRun", "Auto-run input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump, "InputTag.Jump", "Jump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_StopJump, "InputTag.StopJump", "StopJump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_1, "InputTag.QuickBar.Slot.1", "Activate quick bar slot 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_2, "InputTag.QuickBar.Slot.2", "Activate quick bar slot 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_3, "InputTag.QuickBar.Slot.3", "Activate quick bar slot 3.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_4, "InputTag.QuickBar.Slot.4", "Activate quick bar slot 4.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_5, "InputTag.QuickBar.Slot.5", "Activate quick bar slot 5.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_6, "InputTag.QuickBar.Slot.6", "Activate quick bar slot 6.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_7, "InputTag.QuickBar.Slot.7", "Activate quick bar slot 7.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_QuickBar_Slot_8, "InputTag.QuickBar.Slot.8", "Activate quick bar slot 8.");
	
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Death, "GameplayEvent.Death", "Event that fires on death. This event only fires on the server.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Reset, "GameplayEvent.Reset", "Event that fires once a player reset is executed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_RequestReset, "GameplayEvent.RequestReset", "Event to request a player's pawn to be instantly replaced with a new one at a valid spawn location.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Downed, "GameplayEvent.Downed", "Event that fires when a target enters the downed state.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Revive, "GameplayEvent.Revive", "Event that fires when a target is revived.");
	
	// ---------------- Status ----------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Crouching, "Status.Crouching", "Target is crouching.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_AutoRunning, "Status.AutoRunning", "Target is auto-running.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Target is dead or currently dying.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death, "Status.Death", "Target has the death status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed, "Status.Downed", "Target is downed but not finally dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed_BleedingOut, "Status.Downed.BleedingOut", "Target is downed and bleeding out.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed_Reviving, "Status.Downed.Reviving", "Target is currently being revived.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_CannotBeRevived, "Status.CannotBeRevived", "Target cannot enter or receive revive.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "SetByCaller tag used by damage gameplay effects.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Heal, "SetByCaller.Heal", "SetByCaller tag used by healing gameplay effects.");
	
	// ---------------- Lifecycle ----------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_Spawned, "InitState.Spawned", "Pawn spawned.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataAvailable, "InitState.DataAvailable", "Pawn data available.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_DataInitialized, "InitState.DataInitialized", "Pawn data initialized.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InitState_GameplayReady, "InitState.GameplayReady", "Pawn gameplay ready.");
	
	
	// ---------------- Cheating ----------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_GodMode, "Cheat.GodMode", "GodMode cheat is active on the owner.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cheat_UnlimitedHealth, "Cheat.UnlimitedHealth", "UnlimitedHealth cheat is active on the owner.");
	

	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath", "An ability with this type tag should not be canceled due to death.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Behavior_ClearOnRespawn, "Effect.Behavior.ClearOnRespawn", "GameplayEffects with this tag are removed when the owner respawns.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Respawn, "GameplayEvent.Respawn", "Event that fires when a player respawns.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_BleedoutExpired, "GameplayEvent.BleedoutExpired", "Event that fires when bleedout timer expires and the player truly dies.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Dead_WaitingForRespawn, "Status.Dead.WaitingForRespawn", "Target is dead and waiting for respawn.");

}

