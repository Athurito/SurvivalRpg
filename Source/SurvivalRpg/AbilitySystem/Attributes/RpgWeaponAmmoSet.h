// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAttributeSet.h"
#include "RpgWeaponAmmoSet.generated.h"

/**
 * 
 */
UCLASS()
class SURVIVALRPG_API URpgWeaponAmmoSet : public URpgAttributeSet
{
	GENERATED_BODY()
	
public:
	URpgWeaponAmmoSet();
	
	// “Clip/Quiver”
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Ammo", ReplicatedUsing=OnRep_Ammo)
	FGameplayAttributeData Ammo;
	ATTRIBUTE_ACCESSORS_BASIC(URpgWeaponAmmoSet, Ammo);

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Ammo", ReplicatedUsing=OnRep_MaxAmmo)
	FGameplayAttributeData MaxAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(URpgWeaponAmmoSet, MaxAmmo);

	// Optional: “Reserve” (wenn du 2 Pools willst)
	UPROPERTY(BlueprintReadOnly, Category="Weapon|Ammo", ReplicatedUsing=OnRep_AmmoReserve)
	FGameplayAttributeData AmmoReserve;
	ATTRIBUTE_ACCESSORS_BASIC(URpgWeaponAmmoSet, AmmoReserve);

	UPROPERTY(BlueprintReadOnly, Category="Weapon|Ammo", ReplicatedUsing=OnRep_MaxAmmoReserve)
	FGameplayAttributeData MaxAmmoReserve;
	ATTRIBUTE_ACCESSORS_BASIC(URpgWeaponAmmoSet, MaxAmmoReserve);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	UFUNCTION() void OnRep_Ammo(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_AmmoReserve(const FGameplayAttributeData& OldValue) const;
	UFUNCTION() void OnRep_MaxAmmoReserve(const FGameplayAttributeData& OldValue) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
};
