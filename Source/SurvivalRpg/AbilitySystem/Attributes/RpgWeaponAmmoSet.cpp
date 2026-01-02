// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgWeaponAmmoSet.h"

#include "Net/UnrealNetwork.h"

URpgWeaponAmmoSet::URpgWeaponAmmoSet()
{
	Ammo.SetBaseValue(0.f); Ammo.SetCurrentValue(0.f);
	MaxAmmo.SetBaseValue(20.f); MaxAmmo.SetCurrentValue(20.f);

	AmmoReserve.SetBaseValue(0.f); AmmoReserve.SetCurrentValue(0.f);
	MaxAmmoReserve.SetBaseValue(200.f); MaxAmmoReserve.SetCurrentValue(200.f);
}

void URpgWeaponAmmoSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetMaxAmmoAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		SetAmmo(FMath::Clamp(GetAmmo(), 0.f, NewValue));
	}
	else if (Attribute == GetMaxAmmoReserveAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		SetAmmoReserve(FMath::Clamp(GetAmmoReserve(), 0.f, NewValue));
	}
}

void URpgWeaponAmmoSet::OnRep_Ammo(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgWeaponAmmoSet, Ammo, OldValue);
}

void URpgWeaponAmmoSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgWeaponAmmoSet, MaxAmmo, OldValue);
}

void URpgWeaponAmmoSet::OnRep_AmmoReserve(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgWeaponAmmoSet, AmmoReserve, OldValue);
}

void URpgWeaponAmmoSet::OnRep_MaxAmmoReserve(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgWeaponAmmoSet, MaxAmmoReserve, OldValue);
}

void URpgWeaponAmmoSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgWeaponAmmoSet, Ammo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgWeaponAmmoSet, MaxAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgWeaponAmmoSet, AmmoReserve, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgWeaponAmmoSet, MaxAmmoReserve, COND_None, REPNOTIFY_Always);
}
