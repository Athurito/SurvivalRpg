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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Basic, "Ability.Attack.Basic", "Semantic tag for a basic attack ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Support_Heal, "Ability.Support.Heal", "Semantic tag for a support healing ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Defense_Block, "Ability.Defense.Block", "Semantic tag for a defensive block ability.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AI_Role_Grunt, "AI.Role.Grunt", "Semantic tag for a basic grunt AI role.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Archetype_BasicSword, "Enemy.Archetype.BasicSword", "Enemy combat archetype that equips a basic sword.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Archetype_SwordShield, "Enemy.Archetype.SwordShield", "Enemy combat archetype that equips a sword and shield.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Archetype_TwoHanded, "Enemy.Archetype.TwoHanded", "Enemy combat archetype that equips a basic two-handed weapon.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Faction_Player, "Faction.Player", "Semantic faction tag for player-aligned actors.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Faction_Enemy, "Faction.Enemy", "Semantic faction tag for enemy-aligned actors.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "Move input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Look (mouse) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look_Stick, "InputTag.Look.Stick", "Look (stick) input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Crouch, "InputTag.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_AutoRun, "InputTag.AutoRun", "Auto-run input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Jump, "InputTag.Jump", "Jump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_StopJump, "InputTag.StopJump", "StopJump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UI_Inventory, "InputTag.UI.Inventory", "Open the owning player's inventory screen.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UI_QuickAccessRadial_Hold, "InputTag.UI.QuickAccessRadial.Hold", "Hold to open the owning player's quick-access radial.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UI_QuickAccessRadial_Select, "InputTag.UI.QuickAccessRadial.Select", "Select a quick-access radial segment with the right stick.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UI_QuickAccessRadial_Cancel, "InputTag.UI.QuickAccessRadial.Cancel", "Close the quick-access radial without activating a slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_1, "InputTag.ActionBar.Slot.1", "Activate general action bar slot 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_2, "InputTag.ActionBar.Slot.2", "Activate general action bar slot 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_3, "InputTag.ActionBar.Slot.3", "Activate general action bar slot 3.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_4, "InputTag.ActionBar.Slot.4", "Activate general action bar slot 4.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_5, "InputTag.ActionBar.Slot.5", "Activate general action bar slot 5.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_6, "InputTag.ActionBar.Slot.6", "Activate general action bar slot 6.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_7, "InputTag.ActionBar.Slot.7", "Activate general action bar slot 7.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_ActionBar_Slot_8, "InputTag.ActionBar.Slot.8", "Activate general action bar slot 8.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Ability_1, "InputTag.Weapon.Ability.1", "Activate selected weapon or rune ability slot 1.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Ability_2, "InputTag.Weapon.Ability.2", "Activate selected weapon or rune ability slot 2.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Ability_3, "InputTag.Weapon.Ability.3", "Activate selected weapon or rune ability slot 3.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Primary, "InputTag.Weapon.Primary", "Primary weapon attack input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Secondary, "InputTag.Weapon.Secondary", "Secondary weapon attack input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Weapon_Block, "InputTag.Weapon.Block", "Hold block input for the equipped weapon.");
	
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Death, "GameplayEvent.Death", "Event that fires on death. This event only fires on the server.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_HitReaction, "GameplayEvent.HitReaction", "Event that requests a target hit reaction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Block, "GameplayEvent.Block", "Event that fires when a melee hit is blocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_PerfectBlock, "GameplayEvent.PerfectBlock", "Event that fires when a melee hit is perfect blocked.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Stagger, "GameplayEvent.Stagger", "Event that requests a target stagger or guard break.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_ItemUse_Apply, "GameplayEvent.ItemUse.Apply", "Event sent by item-use montages when configured item effects should execute.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Weapon_Attack_Window_Start, "GameplayEvent.Weapon.Attack.Window.Start", "Event sent by melee attack montages when weapon damage tracing starts.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Weapon_Attack_Window_End, "GameplayEvent.Weapon.Attack.Window.End", "Event sent by melee attack montages when weapon damage tracing ends.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Reset, "GameplayEvent.Reset", "Event that fires once a player reset is executed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_RequestReset, "GameplayEvent.RequestReset", "Event to request a player's pawn to be instantly replaced with a new one at a valid spawn location.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Downed, "GameplayEvent.Downed", "Event that fires when a target enters the downed state.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Revive, "GameplayEvent.Revive", "Event that fires when a target is revived.");
	
	// ---------------- Status ----------------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Crouching, "Status.Crouching", "Target is crouching.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_AutoRunning, "Status.AutoRunning", "Target is auto-running.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Target is dead or currently dying.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Target is actively blocking.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PerfectBlockWindow, "State.PerfectBlockWindow", "Target is inside the perfect block timing window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Target is staggered.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_GuardBroken, "State.GuardBroken", "Target's guard is broken.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_StaggerImmune, "State.StaggerImmune", "Target is temporarily immune to new stagger buildup.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Staggerable, "Trait.Staggerable", "Target can build stagger and enter stagger reactions.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death, "Status.Death", "Target has the death status.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed, "Status.Downed", "Target is downed but not finally dead.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed_BleedingOut, "Status.Downed.BleedingOut", "Target is downed and bleeding out.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Downed_Reviving, "Status.Downed.Reviving", "Target is currently being revived.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_CannotBeRevived, "Status.CannotBeRevived", "Target cannot enter or receive revive.");
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "SetByCaller tag used by damage gameplay effects.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Heal, "SetByCaller.Heal", "SetByCaller tag used by healing gameplay effects.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_StaggerDamage, "SetByCaller.StaggerDamage", "SetByCaller tag used by stagger gameplay effects.");
	
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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Combat_Message_ActorKilled, "Rpg.Combat.Message.ActorKilled", "Gameplay message sent by combat when an actor is killed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_ActionBar_Message_SlotsChanged, "Rpg.ActionBar.Message.SlotsChanged", "Gameplay message sent when a controller-owned general action bar slot changes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_WeaponAbilityLoadout_Message_SlotsChanged, "Rpg.WeaponAbilityLoadout.Message.SlotsChanged", "Gameplay message sent when a controller-owned weapon ability binding changes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_EquipmentLoadout_Message_SlotsChanged, "Rpg.EquipmentLoadout.Message.SlotsChanged", "Gameplay message sent when a controller-owned dedicated equipment slot assignment changes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_InventoryLayout_Message_Changed, "Rpg.InventoryLayout.Message.Changed", "Gameplay message sent when the player inventory layout or slot capacity changes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Message_ActionFeedback, "Rpg.Inventory.Message.ActionFeedback", "Owning-client gameplay message for inventory shortcut or server validation feedback.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_Transfer, "Rpg.Inventory.Action.Transfer", "Inventory UI action tag for transfer requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_Split, "Rpg.Inventory.Action.Split", "Inventory UI action tag for split-stack requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_Use, "Rpg.Inventory.Action.Use", "Inventory UI action tag for item use requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_Drop, "Rpg.Inventory.Action.Drop", "Inventory UI action tag for manual item drop requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_Equip, "Rpg.Inventory.Action.Equip", "Inventory UI action tag for convenience equip requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_EquipAndActivate, "Rpg.Inventory.Action.EquipAndActivate", "Inventory UI action tag for explicit fast-equip and activation requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Rpg_Inventory_Action_MoveToCarry, "Rpg.Inventory.Action.MoveToCarry", "Inventory UI action tag for moving an item to Carry without activation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature, "Feature", "Root tag for placed GameFeature-driven world spawning markers.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_MainHand, "Equipment.Slot.MainHand", "Equipment is assigned to the main hand slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_OffHand, "Equipment.Slot.OffHand", "Equipment is assigned to the off hand slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_BothHands, "Equipment.Slot.BothHands", "Equipment occupies both hand slots.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Head, "Equipment.Slot.Head", "Equipment is assigned to the head armor slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Chest, "Equipment.Slot.Chest", "Equipment is assigned to the chest armor slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Hands, "Equipment.Slot.Hands", "Equipment is assigned to the hands armor slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Legs, "Equipment.Slot.Legs", "Equipment is assigned to the legs armor slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Feet, "Equipment.Slot.Feet", "Equipment is assigned to the feet armor slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Attack_Primary, "Weapon.Attack.Primary", "Default primary weapon attack definition.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Attack_Secondary, "Weapon.Attack.Secondary", "Default secondary weapon attack definition.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Melee, "Weapon.Type.Melee", "Melee weapon type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Ranged, "Weapon.Type.Ranged", "Ranged weapon type.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Family_Sword, "Weapon.Family.Sword", "Sword weapon family.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Melee_Sword, "Weapon.Melee.Sword", "Sword melee weapon tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Melee, "Damage.Type.Melee", "Melee damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Projectile, "Damage.Type.Projectile", "Projectile damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Magic, "Damage.Type.Magic", "Magic damage.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_Respawn, "GameplayEvent.Respawn", "Event that fires when a player respawns.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayEvent_BleedoutExpired, "GameplayEvent.BleedoutExpired", "Event that fires when bleedout timer expires and the player truly dies.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Dead_WaitingForRespawn, "Status.Dead.WaitingForRespawn", "Target is dead and waiting for respawn.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Game, "UI.Layer.Game", "HUD and persistent in-game widgets.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_GameMenu, "UI.Layer.GameMenu", "In-game menus such as inventory, storage, crafting, and character panels.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Menu, "UI.Layer.Menu", "Full-screen menu layer above in-game menus.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Modal, "UI.Layer.Modal", "Blocking modal dialogs above every other UI layer.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Boot, "UI.Screen.Boot", "Boot flow screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_MainMenu, "UI.Screen.MainMenu", "Main menu composition root opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Inventory, "UI.Screen.Inventory", "Inventory screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Storage, "UI.Screen.Storage", "Storage screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Loot, "UI.Screen.Loot", "Loot screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Crafting, "UI.Screen.Crafting", "Crafting screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_BaseTerminal, "UI.Screen.BaseTerminal", "Base terminal screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Respawn, "UI.Screen.Respawn", "Blocking respawn screen opened while the owning player is waiting to respawn.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Pause, "UI.Screen.Pause", "Pause screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Screen_Settings, "UI.Screen.Settings", "Settings screen opened through the project UI screen registry.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_HUD_Slot_ActionBar, "UI.HUD.Slot.ActionBar", "UIExtension slot for the persistent general action bar HUD widget.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_HUD_Slot_QuickAccessRadial, "UI.HUD.Slot.QuickAccessRadial", "Fullscreen UIExtension slot for the persistent quick-access radial presenter.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_HUD_Slot_WeaponAbilityBar, "UI.HUD.Slot.WeaponAbilityBar", "UIExtension slot for the persistent weapon ability HUD widget.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Base_Storage_Upgrade_AutoSort, "Base.Storage.Upgrade.AutoSort", "Base storage upgrade tag that unlocks shared auto-sort convenience.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Base_Storage_Upgrade_RemoteAccess, "Base.Storage.Upgrade.RemoteAccess", "Base storage upgrade tag that unlocks remote access convenience.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Base_Storage_Upgrade_CraftingOutputAutoDeposit, "Base.Storage.Upgrade.CraftingOutputAutoDeposit", "Base storage upgrade tag that lets crafting outputs flow back into linked base storage.");

}

