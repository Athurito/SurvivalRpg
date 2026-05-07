// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgBasePlayerState.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"

const FName ARpgBasePlayerState::NAME_RpgAbilityReady(TEXT("RpgAbilitiesReady"));

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
}

URpgAbilitySystemComponent* ARpgBasePlayerState::GetRpgAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAbilitySystemComponent* ARpgBasePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
	ForceNetUpdate();
}

void ARpgBasePlayerState::OnRep_PawnData()
{
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
