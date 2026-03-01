// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgHealthComponent.h"

#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"


// Sets default values for this component's properties
URpgHealthComponent::URpgHealthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	HealthSet = nullptr;
	DeathState = ERpgDeathState::NotDead;
}


void URpgHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URpgHealthComponent, DeathState);
}

void URpgHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void URpgHealthComponent::InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Health component for owner [%s] has already been initialized with an ability system."), *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Cannot initialize health component for owner [%s] with NULL ability system."), *GetNameSafe(Owner));
		return;
	}

	HealthSet = AbilitySystemComponent->GetSet<URpgHealthSet>();
	if (!HealthSet)
	{
		UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Cannot initialize health component for owner [%s] with NULL health set on the ability system."), *GetNameSafe(Owner));
		return;
	}

	// Register to listen for attribute changes.
	HealthSet->OnHealthChanged.AddUObject(this, &ThisClass::HandleHealthChanged);
	HealthSet->OnMaxHealthChanged.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);

	// TEMP: Reset attributes to default values.  Eventually this will be driven by a spread sheet.
	AbilitySystemComponent->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());

	ClearGameplayTags();

	OnHealthChanged.Broadcast(this, HealthSet->GetHealth(), HealthSet->GetHealth(), nullptr);
	OnMaxHealthChanged.Broadcast(this, HealthSet->GetHealth(), HealthSet->GetHealth(), nullptr);
}

void URpgHealthComponent::UninitializeFromAbilitySystem()
{
	ClearGameplayTags();

	if (HealthSet)
	{
		HealthSet->OnHealthChanged.RemoveAll(this);
		HealthSet->OnMaxHealthChanged.RemoveAll(this);
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	HealthSet = nullptr;
	AbilitySystemComponent = nullptr;
}

void URpgHealthComponent::ClearGameplayTags()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 0);
	}
}

float URpgHealthComponent::GetHealth() const
{
	return (HealthSet ? HealthSet->GetHealth() : 0.0f);
}

float URpgHealthComponent::GetMaxHealth() const
{
	return (HealthSet ? HealthSet->GetMaxHealth() : 0.0f);
}

float URpgHealthComponent::GetHealthNormalized() const
{
	if (HealthSet)
	{
		const float Health = HealthSet->GetHealth();
		const float MaxHealth = HealthSet->GetMaxHealth();

		return ((MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f);
	}

	return 0.0f;
}

void URpgHealthComponent::HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void URpgHealthComponent::HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void URpgHealthComponent::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
#if WITH_SERVER_CODE
	if (AbilitySystemComponent && DamageEffectSpec)
	{
		// Send the "GameplayEvent.Death" gameplay event through the owner's ability system.  This can be used to trigger a death gameplay ability.
		{
			FGameplayEventData Payload;
			Payload.EventTag = RpgGameplayTags::GameplayEvent_Death;
			Payload.Instigator = DamageInstigator;
			Payload.Target = AbilitySystemComponent->GetAvatarActor();
			Payload.OptionalObject = DamageEffectSpec->Def;
			Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
			Payload.InstigatorTags = *DamageEffectSpec->CapturedSourceTags.GetAggregatedTags();
			Payload.TargetTags = *DamageEffectSpec->CapturedTargetTags.GetAggregatedTags();
			Payload.EventMagnitude = DamageMagnitude;

			FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
			AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
		}
	}

#endif // #if WITH_SERVER_CODE
}

void URpgHealthComponent::OnRep_DeathState(ERpgDeathState OldDeathState)
{
	const ERpgDeathState NewDeathState = DeathState;

	// Revert the death state for now since we rely on StartDeath and FinishDeath to change it.
	DeathState = OldDeathState;

	if (OldDeathState > NewDeathState)
	{
		// The server is trying to set us back but we've already predicted past the server state.
		UE_LOG(LogRpg, Warning, TEXT("RpgHealthComponent: Predicted past server death state [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == ERpgDeathState::NotDead)
	{
		if (NewDeathState == ERpgDeathState::DeathStarted)
		{
			StartDeath();
		}
		else if (NewDeathState == ERpgDeathState::DeathFinished)
		{
			StartDeath();
			FinishDeath();
		}
		else
		{
			UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Invalid death transition [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == ERpgDeathState::DeathStarted)
	{
		if (NewDeathState == ERpgDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: Invalid death transition [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState), TEXT("RpgHealthComponent: Death transition failed [%d] -> [%d] for owner [%s]."), static_cast<uint8>(OldDeathState), static_cast<uint8>(NewDeathState), *GetNameSafe(GetOwner()));
}

void URpgHealthComponent::StartDeath()
{
	if (DeathState != ERpgDeathState::NotDead)
	{
		return;
	}

	DeathState = ERpgDeathState::DeathStarted;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathStarted.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void URpgHealthComponent::FinishDeath()
{
	if (DeathState != ERpgDeathState::DeathStarted)
	{
		return;
	}

	DeathState = ERpgDeathState::DeathFinished;

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathFinished.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void URpgHealthComponent::DamageSelfDestruct(bool bFellOutOfWorld)
{
	if ((DeathState == ERpgDeathState::NotDead) && AbilitySystemComponent)
	{
		if (!DamageGameplayEffect_SetByCaller)
		{
			UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: DamageSelfDestruct failed for owner [%s]."), *GetNameSafe(GetOwner()));
			return;
		}

		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageGameplayEffect_SetByCaller, 1.0f, AbilitySystemComponent->MakeEffectContext());
		FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

		if (!Spec)
		{
			UE_LOG(LogRpg, Error, TEXT("RpgHealthComponent: DamageSelfDestruct failed for owner [%s]. Unable to make outgoing spec for [%s]."), *GetNameSafe(GetOwner()), *GetNameSafe(DamageGameplayEffect_SetByCaller));
			return;
		}

		Spec->AddDynamicAssetTag(TAG_Gameplay_DamageSelfDestruct);

		if (bFellOutOfWorld)
		{
			Spec->AddDynamicAssetTag(TAG_Gameplay_FellOutOfWorld);
		}

		const float DamageAmount = GetMaxHealth();

		Spec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, DamageAmount);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
	}
}
