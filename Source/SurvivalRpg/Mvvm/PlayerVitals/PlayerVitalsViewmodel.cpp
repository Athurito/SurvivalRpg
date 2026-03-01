// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsViewmodel.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"


void UPlayerVitalsViewmodel::Initialize(UAbilitySystemComponent* InASC)
{
	if (ASC == InASC)
		return;

	UnInitialize();

	ASC = InASC;

	if (ASC)
	{
		BindASC();
		InitialRefresh();
	}
}

void UPlayerVitalsViewmodel::UnInitialize()
{
	UnbindASC();
	ASC = nullptr;

	UE_MVVM_SET_PROPERTY_VALUE(Health, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, 0.f);
}

void UPlayerVitalsViewmodel::BindASC()
{
	if (!ASC)
		return;

	HealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(
			URpgHealthSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::OnHealthChanged);

	MaxHealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(
			URpgHealthSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::OnMaxHealthChanged);
}

void UPlayerVitalsViewmodel::UnbindASC()
{
	if (!ASC)
		return;

	ASC->GetGameplayAttributeValueChangeDelegate(
		URpgHealthSet::GetHealthAttribute())
		.Remove(HealthChangedHandle);

	ASC->GetGameplayAttributeValueChangeDelegate(
		URpgHealthSet::GetMaxHealthAttribute())
		.Remove(MaxHealthChangedHandle);

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
}

void UPlayerVitalsViewmodel::InitialRefresh()
{
	// EINMAL initial refresh
	const float Current =
		ASC->GetNumericAttribute(URpgHealthSet::GetHealthAttribute());

	const float Max =
		ASC->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, Current);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Max);

	const float Percent = (Max > 0.f) ? Current / Max : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, Percent);
}

void UPlayerVitalsViewmodel::RefreshHealth()
{
	if (!ASC)
		return;

	const float Current =
		ASC->GetNumericAttribute(URpgHealthSet::GetHealthAttribute());

	const float Max =
		ASC->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, Current);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Max);

	const float Percent = (Max > 0.f) ? Current / Max : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, Percent);
}

void UPlayerVitalsViewmodel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, Data.NewValue);

	const float Max = MaxHealth; // Wir nutzen die gespeicherte Property

	const float Percent = (Max > 0.f) ? Data.NewValue / Max : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, Percent);
}

void UPlayerVitalsViewmodel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Data.NewValue);

	const float Current = Health; // gespeicherter Wert

	const float Percent = (Data.NewValue > 0.f) ? Current / Data.NewValue : 0.f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, Percent);
}
