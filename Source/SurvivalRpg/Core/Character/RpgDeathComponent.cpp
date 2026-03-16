// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDeathComponent.h"

#include "RpgHealthComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Death.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"


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

	// Später:
	// if (CanEnterDowned(Info))
	// {
	//     EnterDowned(Info);
	//     return;
	// }

	FGameplayEventData Payload;
	Payload.EventTag = RpgGameplayTags::GameplayEvent_Death;
	Payload.Instigator = Info.DamageInstigator;
	Payload.Target = Owner;
	Payload.EventMagnitude = Info.DamageMagnitude;

	if (Info.DamageEffectSpec)
	{
		Payload.OptionalObject = Info.DamageEffectSpec->Def;
		Payload.ContextHandle = Info.DamageEffectSpec->GetEffectContext();

		if (const FGameplayTagContainer* SourceTags = Info.DamageEffectSpec->CapturedSourceTags.GetAggregatedTags())
		{
			Payload.InstigatorTags = *SourceTags;
		}

		if (const FGameplayTagContainer* TargetTags = Info.DamageEffectSpec->CapturedTargetTags.GetAggregatedTags())
		{
			Payload.TargetTags = *TargetTags;
		}
	}
	FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
}
