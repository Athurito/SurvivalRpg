// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDownedComponent.h"

#include "RpgHealthComponent.h"
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

	if (AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgDownedComponent: Component for owner [%s] has already been initialized."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgDownedComponent: Cannot initialize for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}

	HealthComponent = Owner->FindComponentByClass<URpgHealthComponent>();
}

void URpgDownedComponent::UninitializeFromAbilitySystem()
{
	StopBleedoutTimer();
	ClearDownedTags();

	AbilitySystemComponent = nullptr;
	HealthComponent = nullptr;
	DownedState = ERpgDownedState::NotDowned;
	bPendingDeath = false;
}

// ---------------------------------------------------------------------------
// Downed State Machine
// ---------------------------------------------------------------------------

bool URpgDownedComponent::TryEnterDowned()
{
	if (!AbilitySystemComponent || !HealthComponent)
	{
		return false;
	}

	// Already downed or dead — cannot enter downed again.
	if (DownedState != ERpgDownedState::NotDowned)
	{
		return false;
	}

	// Prevent re-entering downed during the DamageSelfDestruct path after bleedout.
	if (bPendingDeath)
	{
		return false;
	}

	// Some characters (bosses, etc.) cannot be downed.
	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_CannotBeRevived))
	{
		return false;
	}

	// Already dying/dead via the health component.
	if (HealthComponent->IsDeadOrDying())
	{
		return false;
	}

	// Enter downed state.
	SetDownedState(ERpgDownedState::Downed);

	AbilitySystemComponent->AddLooseGameplayTag(RpgGameplayTags::Status_Downed);
	AbilitySystemComponent->AddLooseGameplayTag(RpgGameplayTags::Status_Downed_BleedingOut);

	StartBleedoutTimer();

	// Send gameplay event so abilities/UI can react.
	{
		FGameplayEventData Payload;
		Payload.EventTag = RpgGameplayTags::GameplayEvent_Downed;
		Payload.Instigator = GetOwner();
		Payload.Target = GetOwner();

		AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] entered downed state. Bleedout: %.1fs"), *GetNameSafe(GetOwner()), BleedoutDuration);
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

	DownedState = ERpgDownedState::NotDowned;

	// Flag to prevent HandleOutOfHealth from re-entering TryEnterDowned.
	bPendingDeath = true;

	// Now trigger real death through the health component's self-destruct,
	// which will send GameplayEvent.Death and activate the Death Ability.
	if (HealthComponent)
	{
		HealthComponent->DamageSelfDestruct();
	}

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] transitioned from downed to death."), *GetNameSafe(GetOwner()));
}

void URpgDownedComponent::ExitDowned()
{
	StopBleedoutTimer();
	ClearDownedTags();

	SetDownedState(ERpgDownedState::NotDowned);
}

// ---------------------------------------------------------------------------
// Revive
// ---------------------------------------------------------------------------

void URpgDownedComponent::CompleteRevive(AActor* Reviver)
{
	if (DownedState != ERpgDownedState::Downed)
	{
		return;
	}

	// Restore health.
	if (HealthComponent && AbilitySystemComponent)
	{
		const float HealAmount = HealthComponent->GetMaxHealth() * ReviveHealthPercent;
		AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealAmount);
	}

	ExitDowned();

	// Send gameplay event.
	if (AbilitySystemComponent)
	{
		FGameplayEventData Payload;
		Payload.EventTag = RpgGameplayTags::GameplayEvent_Revive;
		Payload.Instigator = Reviver;
		Payload.Target = GetOwner();

		AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
	}

	OnReviveCompleted.Broadcast(Reviver);

	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] revived by [%s]. Health restored to %.0f%%."), *GetNameSafe(GetOwner()), *GetNameSafe(Reviver), ReviveHealthPercent * 100.0f);
}

// ---------------------------------------------------------------------------
// Bleedout Timer
// ---------------------------------------------------------------------------

void URpgDownedComponent::StartBleedoutTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BleedoutTimerHandle,
			this,
			&URpgDownedComponent::OnBleedoutTimerExpired,
			BleedoutDuration,
			false
		);
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
	UE_LOG(LogRpg, Log, TEXT("RpgDownedComponent: [%s] bleedout expired."), *GetNameSafe(GetOwner()));

	OnBleedoutExpired.Broadcast();

	// Send gameplay event.
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
	if (BleedoutDuration <= 0.0f) return 0.0f;
	return FMath::Clamp(GetBleedoutTimeRemaining() / BleedoutDuration, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------------------------

void URpgDownedComponent::SetDownedState(ERpgDownedState NewState)
{
	if (DownedState == NewState) return;

	DownedState = NewState;
	OnDownedStateChanged.Broadcast(NewState);
}

void URpgDownedComponent::ClearDownedTags()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
	}
}
