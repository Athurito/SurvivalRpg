// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsViewmodel.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"


void UPlayerVitalsViewmodel::BindASC(UAbilitySystemComponent* InASC)
{
	if (ASC.Get() == InASC)
	{
		if (ASC.IsValid())
		{
			RefreshOnce();
		}
		return;
	}

	UnbindASC();

	ASC = InASC;
	if (!ASC.IsValid())
	{
		return;
	}

	RefreshOnce();

	HealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetHealthAttribute())
		.AddLambda([this](const FOnAttributeChangeData& Data)
		{
			SetHealth(Data.NewValue);
		});

	MaxHealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetMaxHealthAttribute())
		.AddLambda([this](const FOnAttributeChangeData& Data)
		{
			SetMaxHealth(Data.NewValue);
		});
}

void UPlayerVitalsViewmodel::UnbindASC()
{
	if (ASC.IsValid())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(URpgHealthSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ASC.Reset();
}

void UPlayerVitalsViewmodel::RefreshOnce()
{
	if (!ASC.IsValid()) return;

	SetHealth(ASC->GetNumericAttribute(URpgHealthSet::GetHealthAttribute()));
	SetMaxHealth(ASC->GetNumericAttribute(URpgHealthSet::GetMaxHealthAttribute()));
}

void UPlayerVitalsViewmodel::SetHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(Health, NewValue))
	{
		// computed fieldnotify function neu feuern
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}

void UPlayerVitalsViewmodel::SetMaxHealth(float NewValue)
{
	if (UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, NewValue))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthPercent);
	}
}
