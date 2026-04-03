// Fill out your copyright notice in the Description page of Project Settings.


#include "GameplayTags.h"

namespace RpgGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Move input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Look (mouse) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Stick, "InputTag.Look.Stick", "Look (stick) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_AutoRun, "InputTag.AutoRun", "Auto-run input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump, "InputTag.Jump", "Jump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_StopJump, "InputTag.StopJump", "StopJump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSet_1, "InputTag.WeaponSet.1", "Activate weapon set 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_WeaponSet_2, "InputTag.WeaponSet.2", "Activate weapon set 2.");
	
	
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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_WeaponSet_1_MainHand, "Equipment.Slot.WeaponSet.1.MainHand", "Main-hand slot for weapon set 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_WeaponSet_1_OffHand, "Equipment.Slot.WeaponSet.1.OffHand", "Off-hand slot for weapon set 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_WeaponSet_2_MainHand, "Equipment.Slot.WeaponSet.2.MainHand", "Main-hand slot for weapon set 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_WeaponSet_2_OffHand, "Equipment.Slot.WeaponSet.2.OffHand", "Off-hand slot for weapon set 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_HandUsage_MainHand, "Equipment.HandUsage.MainHand", "Item can only be equipped in a main-hand slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_HandUsage_OffHand, "Equipment.HandUsage.OffHand", "Item can only be equipped in an off-hand slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_HandUsage_EitherHand, "Equipment.HandUsage.EitherHand", "Item can be equipped in either hand.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_HandUsage_TwoHanded, "Equipment.HandUsage.TwoHanded", "Item occupies both hands.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Trait_Shield, "Equipment.Trait.Shield", "Item behaves like a shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Trait_DualWield, "Equipment.Trait.DualWield", "Item participates in dual-wield compatibility rules.");

}

