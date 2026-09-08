// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * when you modify this, please note that this information can be saved with instances
 * also DefaultEngine.ini [/Script/Engine.CollisionProfile] should match with this list
 * GASP-Migration: Collision Backup 2026-09-06 202324.ini (export filename timestamp;
 * D:/Steven/Downloads), verified against D:/Repos/GameAnimationSample/Config/DefaultEngine.ini,
 * source project EngineAssociation=5.8 (no sample build stamp in the export).
 * Keep GASP Traversable/Mouse/Obstacle at source slots 1/2/3 so imported queries retain their meaning.
 * Move the existing RPG trace roles from slots 1-4 to 6-9; slot 5 remains reserved below.
 * Project Blueprint raw channel/query values must follow this migration; named profile responses retain their roles.
 **/

// Trace against Actors/Components which provide interactions.
#define Rpg_TraceChannel_Interaction				ECC_GameTraceChannel6

// Trace used by weapons, will hit physics assets instead of capsules
#define Rpg_TraceChannel_Weapon						ECC_GameTraceChannel7

// Trace used by by weapons, will hit pawn capsules instead of physics assets
#define Rpg_TraceChannel_Weapon_Capsule				ECC_GameTraceChannel8

// Trace used by by weapons, will trace through multiple pawns rather than stopping on the first hit
#define Rpg_TraceChannel_Weapon_Multi				ECC_GameTraceChannel9

// Allocated to aim assist by the ShooterCore game feature
// ECC_GameTraceChannel5
