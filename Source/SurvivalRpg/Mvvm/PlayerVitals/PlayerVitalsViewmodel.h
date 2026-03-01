// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "PlayerVitalsViewmodel.generated.h"

class URpgPawnExtensionComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;
/**
 * 
 */
UCLASS()
class SURVIVALRPG_API UPlayerVitalsViewmodel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// --- Bindable UI Properties ---
	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float Stamina = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float MaxStamina = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float StaminaPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float Mana = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float MaxMana = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify)
	float ManaPercent = 0.f;
	
public:
	void Initialize(UAbilitySystemComponent* InASC);
	void UnInitialize();
	
private:

	UPROPERTY()
	UAbilitySystemComponent* ASC = nullptr;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

private:
	void BindASC();
	void UnbindASC();

	void InitialRefresh();
	void RefreshHealth();

	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
};
