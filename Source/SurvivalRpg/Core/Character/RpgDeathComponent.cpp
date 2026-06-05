// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDeathComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RpgDownedComponent.h"
#include "RpgHealthComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Death.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_SelfRevive.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Combat/RpgCombatMessages.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"


// Sets default values for this component's properties
URpgDeathComponent::URpgDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgDeathComponent::InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (FindDeathComponent(Owner) != this)
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgDeathComponent: Ignoring duplicate component [%s] on owner [%s]."), *GetNameSafe(this), *GetNameSafe(Owner));
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
		UE_LOG(LogRpg, Error, TEXT("RpgDeathComponent: Cannot initialize death component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}
	
	HealthComponent = URpgHealthComponent::FindHealthComponent(GetOwner());
	if (!HealthComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgDeathComponent: Cannot initialize death component for owner [%s] with NULL health component on the ability system."), *GetNameSafe(Owner));
		return;
	}

	DownedComponent = URpgDownedComponent::FindDownedComponent(Owner);
	HealthComponent->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
}

void URpgDeathComponent::UninitializeFromAbilitySystem()
{
	if (HealthComponent)
	{
		HealthComponent->OnOutOfHealth.RemoveAll(this);
	}

	HealthComponent = nullptr;
	AbilitySystemComponent = nullptr;
	DownedComponent = nullptr;
}

void URpgDeathComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void URpgDeathComponent::HandleOutOfHealth(FRpgOutOfHealthInfo& Info)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!AbilitySystemComponent || !HealthComponent)
	{
		return;
	}

	if (HealthComponent->IsDeadOrDying())
	{
		return;
	}

	if (ShouldEnterDowned() && DownedComponent && DownedComponent->TryEnterDowned())
	{
		return;
	}

	if (TrySoloSelfRevive())
	{
		return;
	}

	FRpgCombatActorKilledMessage KillMessage;
	KillMessage.Victim = Owner;
	KillMessage.Killer = Info.DamageInstigator;
	KillMessage.DamageCauser = Info.DamageCauser;
	KillMessage.DamageMagnitude = Info.DamageMagnitude;

	FGameplayEventData Payload;
	Payload.EventTag = RpgGameplayTags::GameplayEvent_Death;
	Payload.Instigator = Info.DamageInstigator;
	Payload.Target = Owner;
	Payload.EventMagnitude = Info.DamageMagnitude;

	if (Info.DamageEffectSpec)
	{
		Payload.OptionalObject = Info.DamageEffectSpec->Def;
		Payload.ContextHandle = Info.DamageEffectSpec->GetEffectContext();
		KillMessage.DamageEffect = Info.DamageEffectSpec->Def ? Info.DamageEffectSpec->Def->GetClass() : nullptr;
		KillMessage.bWasSelfDestruct = Info.DamageEffectSpec->GetDynamicAssetTags().HasTagExact(TAG_Gameplay_DamageSelfDestruct);

		if (const FGameplayTagContainer* SourceTags = Info.DamageEffectSpec->CapturedSourceTags.GetAggregatedTags())
		{
			Payload.InstigatorTags = *SourceTags;
			KillMessage.SourceTags = *SourceTags;
		}

		if (const FGameplayTagContainer* TargetTags = Info.DamageEffectSpec->CapturedTargetTags.GetAggregatedTags())
		{
			Payload.TargetTags = *TargetTags;
			KillMessage.TargetTags = *TargetTags;
		}
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		MessageSubsystem.BroadcastMessage(RpgGameplayTags::Rpg_Combat_Message_ActorKilled, KillMessage);
	}

	FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
}

bool URpgDeathComponent::ShouldEnterDowned() const
{
	if (!AbilitySystemComponent || !HealthComponent || !DownedComponent || !IsPlayerCharacter())
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(RpgGameplayTags::Status_CannotBeRevived) || DownedComponent->IsDowned())
	{
		return false;
	}

	return HasOtherLivingPlayers();
}

bool URpgDeathComponent::TrySoloSelfRevive() const
{
	if (!AbilitySystemComponent || !IsPlayerCharacter() || HasOtherLivingPlayers() || !SoloSelfReviveAbilityClass)
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateFirstAbilityByClass(SoloSelfReviveAbilityClass, true);
}

bool URpgDeathComponent::HasOtherLivingPlayers() const
{
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!OwningPawn)
	{
		return false;
	}

	const AController* OwningController = OwningPawn->GetController();
	if (!OwningController)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (!PlayerController || PlayerController == OwningController)
		{
			continue;
		}

		const APawn* OtherPawn = PlayerController->GetPawn();
		if (!OtherPawn)
		{
			continue;
		}

		const URpgHealthComponent* OtherHealth = URpgHealthComponent::FindHealthComponent(OtherPawn);
		const URpgDownedComponent* OtherDowned = URpgDownedComponent::FindDownedComponent(OtherPawn);
		if (OtherHealth && !OtherHealth->IsDeadOrDying() && (!OtherDowned || !OtherDowned->IsDowned()))
		{
			return true;
		}
	}

	return false;
}

bool URpgDeathComponent::IsPlayerCharacter() const
{
	if (const APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		return OwningPawn->IsPlayerControlled();
	}

	return false;
}
