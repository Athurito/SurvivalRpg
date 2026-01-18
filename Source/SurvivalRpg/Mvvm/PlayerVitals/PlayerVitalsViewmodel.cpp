// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsViewmodel.h"

#include "AbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgVitalSet.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

void UPlayerVitalsViewmodel::InitializeFromAbilitySystemComponent(UAbilitySystemComponent* InAsc)
{
	if (!InAsc)
	{
		Deinitialize();
		return;
	}

	// Idempotent
	if (ASC == InAsc)
	{
		RefreshAll();
		return;
	}

	UnbindASC();
	ASC = InAsc;
	BindASC();
	RefreshAll();
}
void UPlayerVitalsViewmodel::RefreshAll()
{
	RefreshHealth();
	RefreshMana();
	RefreshStamina();
}


void UPlayerVitalsViewmodel::Deinitialize()
{
	UnbindFromPawnExtension();
	UnbindASC();
	ASC = nullptr;
}

void UPlayerVitalsViewmodel::BindToPlayer(APlayerController* PC)
{
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	URpgPawnExtensionComponent* Ext = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!Ext) return;

	BindToPawnExtension(Ext);

	// Wenn ASC schon da ist -> direkt init
	if (UAbilitySystemComponent* ExistingASC = Ext->GetRpgAbilitySystemComponent())
	{
		InitializeFromAbilitySystemComponent(ExistingASC);
		return;
	}

	// Sonst Triggern (falls Ext noch nicht init gemacht hat)
	Ext->TryInitialize();
}

void UPlayerVitalsViewmodel::BindASC()
{
	HealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetHealthAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnHealthChanged);

	MaxHealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxHealthAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnMaxHealthChanged);
	
	ManaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetManaAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnManaChanged);
	
	MaxManaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxManaAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnMaxManaChanged);
	
	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetStaminaAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnStaminaChanged);
	
	MaxStaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxStaminaAttribute())
		.AddUObject(this, &UPlayerVitalsViewmodel::OnMaxStaminaChanged);
}

void UPlayerVitalsViewmodel::UnbindASC()
{
	if (!ASC) return;
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetHealthAttribute()).Remove(HealthChangedHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetManaAttribute()).Remove(MaxHealthChangedHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(URpgVitalSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ManaChangedHandle.Reset();
	MaxManaChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
}

void UPlayerVitalsViewmodel::RefreshHealth()
{
	if (!ASC) return;

	const float Current = ASC->GetNumericAttribute(URpgVitalSet::GetHealthAttribute());
	const float Max     = ASC->GetNumericAttribute(URpgVitalSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, Current);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Max);

	float Pct = 0.f;
	UpdatePercent(Current, Max, Pct);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, Pct);
}

void UPlayerVitalsViewmodel::RefreshStamina()
{
	if (!ASC) return;

	const float Current = ASC->GetNumericAttribute(URpgVitalSet::GetStaminaAttribute());
	const float Max     = ASC->GetNumericAttribute(URpgVitalSet::GetMaxStaminaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Stamina, Current);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, Max);

	float Pct = 0.f;
	UpdatePercent(Current, Max, Pct);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, Pct);
}

void UPlayerVitalsViewmodel::RefreshMana()
{
	if (!ASC) return;

	const float Current = ASC->GetNumericAttribute(URpgVitalSet::GetManaAttribute());
	const float Max     = ASC->GetNumericAttribute(URpgVitalSet::GetMaxManaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Mana, Current);
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, Max);

	float Pct = 0.f;
	UpdatePercent(Current, Max, Pct);
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, Pct);
}

void UPlayerVitalsViewmodel::UpdatePercent(float Current, float Max, float& OutPercent)
{
	OutPercent = (Max > 0.f) ? (Current / Max) : 0.f;
}

void UPlayerVitalsViewmodel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(Health, Data.NewValue);
	RefreshHealth();
}

void UPlayerVitalsViewmodel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, Data.NewValue);
	RefreshHealth();
}

void UPlayerVitalsViewmodel::OnManaChanged(const FOnAttributeChangeData& Data)
{	
	UE_MVVM_SET_PROPERTY_VALUE(Mana, Data.NewValue);
	RefreshMana();
}

void UPlayerVitalsViewmodel::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, Data.NewValue);
	RefreshMana();
}

void UPlayerVitalsViewmodel::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(Stamina, Data.NewValue);
	RefreshStamina();
}

void UPlayerVitalsViewmodel::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, Data.NewValue);
	RefreshStamina();
}

void UPlayerVitalsViewmodel::BindToPawnExtension(URpgPawnExtensionComponent* Ext)
{
	if (!Ext) return;

	// Wenn gleich wie vorher: nichts neu binden (aber: optional "TryInitialize" / sofort init)
	if (BoundExt == Ext)
	{
		return;
	}

	// Wechsel (Respawn / Pawnwechsel) -> alten Delegate sauber lösen
	UnbindFromPawnExtension();

	BoundExt = Ext;

	// Sauber: AddUObject liefert einen Handle, den wir entfernen können
	H_OnAscReady = BoundExt->OnAscReady.AddUObject(this, &UPlayerVitalsViewmodel::HandleAscReady);
}

void UPlayerVitalsViewmodel::UnbindFromPawnExtension()
{
	if (BoundExt && H_OnAscReady.IsValid())
	{
		BoundExt->OnAscReady.Remove(H_OnAscReady);
	}
	H_OnAscReady.Reset();
	BoundExt = nullptr;
}

void UPlayerVitalsViewmodel::HandleAscReady(UAbilitySystemComponent* ReadyASC)
{
	InitializeFromAbilitySystemComponent(ReadyASC);
}
