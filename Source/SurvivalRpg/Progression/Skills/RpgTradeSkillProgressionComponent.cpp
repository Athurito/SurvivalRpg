// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgTradeSkillProgressionComponent.h"

#include "Data/RpgTradeSkillConfigData.h"
#include "Data/RpgTradeSkillState.h"
#include "Net/UnrealNetwork.h"

// Called when the game starts
void URpgTradeSkillProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server init
	if (GetOwner()->HasAuthority())
	{
		constexpr int32 NumSkills = static_cast<int32>(ETradeSkill::MAX);
		SkillStates.SetNum(NumSkills);
		// Defaults: Level=1, XP=0
	}
	
}

void URpgTradeSkillProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(URpgTradeSkillProgressionComponent, SkillStates, COND_OwnerOnly, REPNOTIFY_Always);
}

URpgTradeSkillProgressionComponent::URpgTradeSkillProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

int32 URpgTradeSkillProgressionComponent::GetSkillLevel(ETradeSkill Skill) const
{
	const int32 Index = static_cast<int32>(Skill);
	return SkillStates.IsValidIndex(Index)
		? SkillStates[Index].Level
		: 1;
}

float URpgTradeSkillProgressionComponent::GetSkillXP(ETradeSkill Skill) const
{
	const int32 Index = static_cast<int32>(Skill);
	return SkillStates.IsValidIndex(Index)
		? SkillStates[Index].XP
		: 0.f;
}

void URpgTradeSkillProgressionComponent::AddSkillXP(ETradeSkill Skill, float Amount)
{
	if (!GetOwner()->HasAuthority()) return;
	if (Amount <= 0.f) return;

	const int32 Index = static_cast<int32>(Skill);
	if (!SkillStates.IsValidIndex(Index)) return;

	const FTradeSkillConfig* Config = GetConfig(Skill);
	if (!Config || !Config->XPToNextLevel) return;

	FTradeSkillState& State = SkillStates[Index];
	State.XP += Amount;

	TryLevelUp(Skill);

	// Rep notify for UI
	OnTradeSkillChanged.Broadcast(Skill, State);
}



void URpgTradeSkillProgressionComponent::OnRep_SkillStates()
{
	// Notify UI for all skills (simple & safe)
	for (int32 i = 0; i < SkillStates.Num(); ++i)
	{
		OnTradeSkillChanged.Broadcast(
			static_cast<ETradeSkill>(i),
			SkillStates[i]
		);
	}
}

const FTradeSkillConfig* URpgTradeSkillProgressionComponent::GetConfig(ETradeSkill Skill) const
{
	if (!ConfigData) return nullptr;
	return ConfigData->SkillConfigs.Find(Skill);
}

void URpgTradeSkillProgressionComponent::TryLevelUp(ETradeSkill Skill)
{
	const int32 Index = static_cast<int32>(Skill);
	FTradeSkillState& State = SkillStates[Index];
	const FTradeSkillConfig* Config = GetConfig(Skill);
	if (!Config) return;

	while (State.Level < Config->MaxLevel)
	{
		const float XPNeeded =
			Config->XPToNextLevel->GetFloatValue(State.Level);

		if (XPNeeded <= 0.f || State.XP < XPNeeded)
			break;

		State.XP -= XPNeeded;
		State.Level++;

		HandleSkillLevelUp(Skill, State.Level);
	}
}

void URpgTradeSkillProgressionComponent::HandleSkillLevelUp(ETradeSkill Skill, int32 NewLevel)
{
	// Hier:
	// - Station tiers freischalten
	// - Rezepte unlocken
	// - permanente GameplayEffects geben
	// - GameplayTags setzen (z.B. Skill.Blacksmithing.Tier.3)

	// Absichtlich leer – projektabhängig
}



