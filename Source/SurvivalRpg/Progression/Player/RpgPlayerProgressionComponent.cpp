// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerProgressionComponent.h"

#include "AbilitySystemInterface.h"
#include "Data/RpgPlayerProgressionData.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

URpgPlayerProgressionComponent::URpgPlayerProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// Called when the game starts
void URpgPlayerProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void URpgPlayerProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(URpgPlayerProgressionComponent,State, COND_OwnerOnly, REPNOTIFY_Always);
}

void URpgPlayerProgressionComponent::AddXP(float Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;

	if (Amount <= 0.f || !ConfigData)
		return;

	State.XP += Amount;
	TryLevelUp();

	const float XPToNext = GetXPToNextLevel(State.Level);
	OnXPChanged.Broadcast(State.XP, XPToNext);
}

bool URpgPlayerProgressionComponent::SpendSkillPoints(int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return false;

	if (Amount <= 0)
		return true;

	if (State.UnspentSkillPoints < Amount)
		return false;

	State.UnspentSkillPoints -= Amount;
	OnSkillPointsChanged.Broadcast(State.UnspentSkillPoints);
	return true;
}



void URpgPlayerProgressionComponent::OnRep_State()
{
	const float XPToNext = GetXPToNextLevel(State.Level);

	OnXPChanged.Broadcast(State.XP, XPToNext);
	OnLevelChanged.Broadcast(State.Level);
	OnSkillPointsChanged.Broadcast(State.UnspentSkillPoints);
}

float URpgPlayerProgressionComponent::GetXPToNextLevel(int32 Level) const
{
	if (!ConfigData || !ConfigData->XPToNextLevel)
		return 0.f;

	return ConfigData->XPToNextLevel->GetFloatValue(static_cast<float>(Level));
}

void URpgPlayerProgressionComponent::TryLevelUp()
{
	if (!ConfigData) return;

	while (State.Level < ConfigData->MaxLevel)
	{
		const float XPNeeded = GetXPToNextLevel(State.Level);
		if (XPNeeded <= 0.f || State.XP < XPNeeded)
			break;

		const int32 OldLevel = State.Level;

		State.XP -= XPNeeded;
		State.Level++;
		State.UnspentSkillPoints += ConfigData->SkillPointsPerLevel;

		HandleLevelUp(OldLevel, State.Level);

		OnLevelChanged.Broadcast(State.Level);
		OnSkillPointsChanged.Broadcast(State.UnspentSkillPoints);
	}
}

void URpgPlayerProgressionComponent::HandleLevelUp(int32 OldLevel, int32 NewLevel)
{
	// Hier gehören Level-Rewards rein:
	// - permanente GameplayEffects (Stats)
	// - GameplayTags (Player.Tier.2 etc.)
	// - Freischaltungen

	APlayerState* PS = Cast<APlayerState>(GetOwner());
	if (!PS) return;

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS);
	if (!ASI) return;

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (!ASC) return;

	// Beispiel:
	// if (NewLevel == 10)
	// {
	//   ASC->AddLooseGameplayTag(
	//     FGameplayTag::RequestGameplayTag("Player.Tier.2")
	//   );
	// }
}





