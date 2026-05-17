// Fill out your copyright notice in the Description page of Project Settings.

#include "SurvivalRpg.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, SurvivalRpg, "SurvivalRpg" );

// General
DEFINE_LOG_CATEGORY(LogRpg);
DEFINE_LOG_CATEGORY(LogRpgExperience);

// Subsystems
DEFINE_LOG_CATEGORY(LogRpgAbilitySystem);
DEFINE_LOG_CATEGORY(LogRpgCharacter);
DEFINE_LOG_CATEGORY(LogRpgInput);
DEFINE_LOG_CATEGORY(LogRpgProgression);
DEFINE_LOG_CATEGORY(LogRpgUI);
DEFINE_LOG_CATEGORY(LogRpgWeapons);
