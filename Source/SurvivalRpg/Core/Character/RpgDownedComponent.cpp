// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgDownedComponent.h"

#include "RpgHealthComponent.h"
#include "TimerManager.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"

URpgDownedComponent::URpgDownedComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void URpgDownedComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();
	Super::OnUnregister();
}

void URpgDownedComponent::InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (FindDownedComponent(Owner) != this)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgDownedComponent: Ignoring duplicate component [%s] on owner [%s]."), *GetNameSafe(this), *GetNameSafe(Owner));
		return;
	}

	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		UninitializeFromAbilitySystem();
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgDownedComponent: Cannot initialize for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}

	HealthComponent = Owner->FindComponentByClass<URpgHealthComponent>();
	if (!HealthComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgDownedComponent: Cannot initialize for owner [%s] without a health component."), *GetNameSafe(Owner));
		AbilitySystemComponent = nullptr;
		return;
	}

	bPendingDeath = false;
}

void URpgDownedComponent::UninitializeFromAbilitySystem()
{
	StopBleedoutTimer();
	ClearDownedTags();

	CurrentReviver = nullptr;
	AbilitySystemComponent = nullptr;
	HealthComponent = nullptr;
	DownedState = ERpgDownedState::NotDowned;
	bPendingDeath = false;
}

bool URpgDownedComponent::TryEnterDowned()
{
	if (!AbilitySystemComponent || !HealthComponent)
	{
		return false;
	}

	if (DownedState != ERpgDownedState::NotDowned || bPendingDeath)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_CannotBeRevived) || HealthComponent->IsDeadOrDying())
	{
		return false;
	}

	SetDownedState(ERpgDownedState::Downed);
	CurrentReviver = nullptr;

	AbilitySystemComponent->ResetForRevive();
	AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 1);
	AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 1);

	StartBleedoutTimer();

	FGameplayEventData Payload;
	Payload.EventTag = RpgGameplayTags::GameplayEvent_Downed;
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] entered downed state. Bleedout %.1fs."), *GetNameSafe(GetOwner()), BleedoutDuration);
	return true;
}

void URpgDownedComponent::ForceDeathFromDowned()
{
	if (DownedState == ERpgDownedState::NotDowned)
	{
		return;
	}

	StopBleedoutTimer();
	ClearDownedTags();
	CurrentReviver = nullptr;
	DownedState = ERpgDownedState::NotDowned;
	bPendingDeath = true;

	if (HealthComponent)
	{
		HealthComponent->DamageSelfDestruct();
	}

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] transitioned from downed to final death."), *GetNameSafe(GetOwner()));
}

bool URpgDownedComponent::CanBeRevivedBy(const AActor* Reviver) const
{
	if (!Reviver || !AbilitySystemComponent || DownedState != ERpgDownedState::Downed)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_CannotBeRevived))
	{
		return false;
	}

	return !CurrentReviver.IsValid() || CurrentReviver.Get() == Reviver;
}

bool URpgDownedComponent::BeginRevive(AActor* Reviver)
{
	if (!CanBeRevivedBy(Reviver))
	{
		return false;
	}

	if (CurrentReviver.Get() == Reviver)
	{
		return true;
	}

	CurrentReviver = Reviver;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 1);
	}

	OnReviveStarted.Broadcast(Reviver);
	return true;
}

void URpgDownedComponent::CancelRevive(AActor* Reviver)
{
	if (!CurrentReviver.IsValid())
	{
		return;
	}

	if (Reviver && CurrentReviver.Get() != Reviver)
	{
		return;
	}

	AActor* PreviousReviver = CurrentReviver.Get();
	CurrentReviver = nullptr;

	if (AbilitySystemComponent && DownedState == ERpgDownedState::Downed)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 1);
	}

	OnReviveCancelled.Broadcast(PreviousReviver);
}

void URpgDownedComponent::CompleteRevive(AActor* Reviver)
{
	if (!CanBeRevivedBy(Reviver))
	{
		return;
	}

	if (AbilitySystemComponent && HealthComponent)
	{
		const float HealAmount = HealthComponent->GetMaxHealth() * ReviveHealthPercent;
		AbilitySystemComponent->ResetForRevive();
		AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealAmount);
	}

	ExitDowned();

	OnReviveCompleted.Broadcast(Reviver);

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] revived by [%s]."), *GetNameSafe(GetOwner()), *GetNameSafe(Reviver));
}

float URpgDownedComponent::GetBleedoutTimeRemaining() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimerManager().GetTimerRemaining(BleedoutTimerHandle);
	}

	return 0.0f;
}

float URpgDownedComponent::GetBleedoutNormalized() const
{
	if (BleedoutDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetBleedoutTimeRemaining() / BleedoutDuration, 0.0f, 1.0f);
}

void URpgDownedComponent::ExitDowned()
{
	StopBleedoutTimer();
	ClearDownedTags();
	CurrentReviver = nullptr;
	bPendingDeath = false;
	SetDownedState(ERpgDownedState::NotDowned);
}

void URpgDownedComponent::SetDownedState(ERpgDownedState NewState)
{
	if (DownedState == NewState)
	{
		return;
	}

	DownedState = NewState;
	OnDownedStateChanged.Broadcast(NewState);
}

void URpgDownedComponent::StartBleedoutTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BleedoutTimerHandle,
			this,
			&URpgDownedComponent::OnBleedoutTimerExpired,
			BleedoutDuration,
			false);
	}
}

void URpgDownedComponent::StopBleedoutTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BleedoutTimerHandle);
	}
}

void URpgDownedComponent::OnBleedoutTimerExpired()
{
	OnBleedoutExpired.Broadcast();

	if (AbilitySystemComponent)
	{
		FGameplayEventData Payload;
		Payload.EventTag = RpgGameplayTags::GameplayEvent_BleedoutExpired;
		Payload.Instigator = GetOwner();
		Payload.Target = GetOwner();
		AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	ForceDeathFromDowned();
}

void URpgDownedComponent::ClearDownedTags()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
	AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
	AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
}
