// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPlayerProgressionComponent.h"

#include "AbilitySystemInterface.h"
#include "Data/RpgPlayerProgressionData.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"

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

	if (!FMath::IsFinite(Amount) || Amount <= 0.f || !ConfigData)
	{
		if (FMath::IsFinite(Amount) && Amount > 0.f && !ConfigData)
		{
			UE_LOG(LogRpgProgression, Warning, TEXT("%s ignored %.2f XP because ConfigData is not set."), *GetNameSafe(this), Amount);
		}
		return;
	}
	if (State.Level >= ConfigData->MaxLevel)
	{
		State.Level = FMath::Max(1, ConfigData->MaxLevel);
		State.XP = 0.0f;
		return;
	}

	State.XP += Amount;
	TryLevelUp();

	const float XPToNext = GetXPToNextLevel(State.Level);
	UE_LOG(LogRpgProgression, Log, TEXT("%s gained %.2f XP. Level=%d XP=%.2f/%.2f SkillPoints=%d"),
		*GetNameSafe(GetOwner()),
		Amount,
		State.Level,
		State.XP,
		XPToNext,
		State.UnspentSkillPoints);
	OnXPChanged.Broadcast(State.XP, XPToNext);
	GetOwner()->ForceNetUpdate();
	MarkOwnerSaveDirty();
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
	GetOwner()->ForceNetUpdate();
	MarkOwnerSaveDirty();
	return true;
}

bool URpgPlayerProgressionComponent::RestoreProgressionState(
	const FPlayerProgressionState& InState)
{
	if ((GetOwner() && !GetOwner()->HasAuthority()) || !InState.IsValid())
	{
		return false;
	}

	State = InState;
	if (ConfigData)
	{
		State.Level = FMath::Clamp(State.Level, 1, FMath::Max(1, ConfigData->MaxLevel));
		if (State.Level >= ConfigData->MaxLevel)
		{
			State.XP = 0.0f;
		}
	}

	BroadcastStateChanged();
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
	return true;
}

void URpgPlayerProgressionComponent::ResetProgressionStateToDefaults()
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return;
	}

	State = FPlayerProgressionState();
	BroadcastStateChanged();
	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}



void URpgPlayerProgressionComponent::OnRep_State()
{
	BroadcastStateChanged();
}

void URpgPlayerProgressionComponent::BroadcastStateChanged()
{
	const float XPToNext = GetXPToNextLevel(State.Level);

	OnXPChanged.Broadcast(State.XP, XPToNext);
	OnLevelChanged.Broadcast(State.Level);
	OnSkillPointsChanged.Broadcast(State.UnspentSkillPoints);
}

float URpgPlayerProgressionComponent::GetXPToNextLevelForCurrentLevel() const
{
	return GetXPToNextLevel(State.Level);
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

	if (State.Level >= ConfigData->MaxLevel)
	{
		State.XP = 0.0f;
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

void URpgPlayerProgressionComponent::MarkOwnerSaveDirty() const
{
	const AActor* Owner = GetOwner();
	APlayerController* PlayerController = Owner ? Cast<APlayerController>(Owner->GetOwner()) : nullptr;
	if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
	{
		GameMode->MarkPlayerSaveDirty(PlayerController);
	}
}





