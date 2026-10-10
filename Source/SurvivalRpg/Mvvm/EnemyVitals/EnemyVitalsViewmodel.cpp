#include "EnemyVitalsViewmodel.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

void UEnemyVitalsViewmodel::BindToActor(AActor* InObservedActor)
{
	if (ObservedActor.Get() == InObservedActor && BoundPawnExtension)
	{
		HandleAbilitySystemInitialized();
		return;
	}

	UnbindFromActor();
	ObservedActor = InObservedActor;

	if (!InObservedActor)
	{
		return;
	}

	BoundPawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(InObservedActor);
	if (BoundPawnExtension)
	{
		BoundPawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
		BoundPawnExtension->OnAbilitySystemUninitialized_Register(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
		return;
	}

	if (const IAbilitySystemInterface* AbilitySystemActor = Cast<IAbilitySystemInterface>(InObservedActor))
	{
		BindAbilitySystem(AbilitySystemActor->GetAbilitySystemComponent());
	}
}

void UEnemyVitalsViewmodel::UnbindFromActor()
{
	if (BoundPawnExtension)
	{
		BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
		BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
		BoundPawnExtension = nullptr;
	}

	UnbindAbilitySystem();
	ObservedActor.Reset();
}

void UEnemyVitalsViewmodel::BeginDestroy()
{
	UnbindFromActor();

	Super::BeginDestroy();
}

void UEnemyVitalsViewmodel::HandleAbilitySystemInitialized()
{
	if (BoundPawnExtension)
	{
		BindAbilitySystem(BoundPawnExtension->GetRpgAbilitySystemComponent());
	}
}

void UEnemyVitalsViewmodel::HandleAbilitySystemUninitialized()
{
	UnbindAbilitySystem();
}

void UEnemyVitalsViewmodel::BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem)
{
	if (AbilitySystem.Get() != InAbilitySystem)
	{
		UnbindAbilitySystem();
		AbilitySystem = InAbilitySystem;
		if (!InAbilitySystem)
		{
			return;
		}

		HealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetHealthAttribute())
			.AddUObject(this, &ThisClass::HandleHealthChanged);
		MaxHealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetMaxHealthAttribute())
			.AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	}

	if (InAbilitySystem)
	{
		SetHealth(InAbilitySystem->GetNumericAttribute(URpgHealthSet::GetHealthAttribute()));
		SetMaxHealth(InAbilitySystem->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute()));
	}
}

void UEnemyVitalsViewmodel::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* BoundAbilitySystem = AbilitySystem.Get())
	{
		BoundAbilitySystem->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
		BoundAbilitySystem->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedHandle);
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	AbilitySystem.Reset();
}

void UEnemyVitalsViewmodel::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	SetHealth(Data.NewValue);
}

void UEnemyVitalsViewmodel::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	SetMaxHealth(Data.NewValue);
}

void UEnemyVitalsViewmodel::SetHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}

void UEnemyVitalsViewmodel::SetMaxHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}
