// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgBasePlayerState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"

const FName ARpgBasePlayerState::NAME_RpgAbilityReady(TEXT("RpgAbilitiesReady"));

namespace
{
const IGenericTeamAgentInterface* GetTeamAgentForActor(const AActor& Actor)
{
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(&Actor))
	{
		return TeamAgent;
	}

	if (const APawn* Pawn = Cast<const APawn>(&Actor))
	{
		return Cast<const IGenericTeamAgentInterface>(Pawn->GetController());
	}

	return nullptr;
}
}

ARpgBasePlayerState::ARpgBasePlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The local HUD reads attributes from the PlayerState-owned ASC, so keep updates responsive.
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(33.0f);

	AbilitySystemComponent = CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));
}

void ARpgBasePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARpgBasePlayerState, PawnData);
	DOREPLIFETIME(ARpgBasePlayerState, TeamId);
}

void ARpgBasePlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());
}

void ARpgBasePlayerState::SendAbilitiesChangedEvent()
{
	FGameplayEventData EventData;
	EventData.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Abilities.Changed"));
	EventData.Instigator = this;
	EventData.Target = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);

	// Saved ability bindings must become available immediately after progression/GameFeature grants change.
	if (HasAuthority())
	{
		if (const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(GetPlayerController()))
		{
			if (URpgActionBarComponent* ActionBar = RpgPlayerController->GetActionBarComponent())
			{
				ActionBar->RefreshBindings();
			}
		}
	}
}

URpgAbilitySystemComponent* ARpgBasePlayerState::GetRpgAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAbilitySystemComponent* ARpgBasePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

int32 ARpgBasePlayerState::GetTeamId() const
{
	return (TeamId == FGenericTeamId::NoTeam.GetId()) ? INDEX_NONE : static_cast<int32>(TeamId);
}

FGenericTeamId ARpgBasePlayerState::GetGenericTeamId() const
{
	return FGenericTeamId(TeamId);
}

ETeamAttitude::Type ARpgBasePlayerState::GetTeamAttitudeTowardsActor(FGenericTeamId OwnTeamId, const AActor& Other)
{
	const IGenericTeamAgentInterface* OtherTeamAgent = GetTeamAgentForActor(Other);
	const FGenericTeamId OtherTeamId = OtherTeamAgent ? OtherTeamAgent->GetGenericTeamId() : FGenericTeamId::NoTeam;

	if (OwnTeamId == FGenericTeamId::NoTeam || OtherTeamId == FGenericTeamId::NoTeam)
	{
		return ETeamAttitude::Neutral;
	}

	return OwnTeamId == OtherTeamId ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void ARpgBasePlayerState::SetPawnData(const URpgPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogTemp, Error, TEXT("Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s]."),
			*GetNameSafe(InPawnData),
			*GetNameSafe(this),
			*GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;
	SetTeamIdFromPawnData(*PawnData);

	check(AbilitySystemComponent);

	for (const TObjectPtr<const URpgAbilitySet>& AbilitySet : PawnData->AbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr, this);
		}
	}

	if (const URpgAIPawnData* AIPawnData = Cast<URpgAIPawnData>(PawnData))
	{
		ApplyStartupLooseTags(AIPawnData->RoleTags);
		ApplyStartupLooseTags(AIPawnData->FactionTags);
	}

	if (const URpgHealthSet* CurrentHealthSet = AbilitySystemComponent->GetSet<URpgHealthSet>())
	{
		AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), CurrentHealthSet->GetMaxHealth());
	}

	SendAbilitiesChangedEvent();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, NAME_RpgAbilityReady);
	PawnDataChanged.Broadcast(PawnData);
	ForceNetUpdate();
}

void ARpgBasePlayerState::OnRep_PawnData()
{
	PawnDataChanged.Broadcast(PawnData);
}

void ARpgBasePlayerState::ApplyStartupLooseTags(const FGameplayTagContainer& TagContainer) const
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	for (const FGameplayTag& Tag : TagContainer)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(Tag, 1);
	}
}

void ARpgBasePlayerState::SetTeamIdFromPawnData(const URpgPawnData& InPawnData)
{
	TeamId = InPawnData.TeamId < 0
		? FGenericTeamId::NoTeam.GetId()
		: static_cast<uint8>(FMath::Clamp(InPawnData.TeamId, 0, 254));
}
