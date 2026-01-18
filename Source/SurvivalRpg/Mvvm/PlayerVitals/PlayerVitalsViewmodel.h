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
	UFUNCTION(BlueprintCallable)
	void InitializeFromAbilitySystemComponent(UAbilitySystemComponent* InAsc);
	void RefreshAll();

	UFUNCTION(BlueprintCallable)
	void Deinitialize(); // unsub
	
	UFUNCTION(BlueprintCallable)
	void BindToPlayer(APlayerController* PC);
	
private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	// Handles, um sauber zu unsubscriben
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	
	FDelegateHandle ManaChangedHandle;
	FDelegateHandle MaxManaChangedHandle;
	
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;

	void BindASC();
	void UnbindASC();
private:
	void RefreshHealth();
	void RefreshStamina();
	void RefreshMana();

	static void UpdatePercent(float Current, float Max, float& OutPercent);

	// Callbacks
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
	
	void OnManaChanged(const FOnAttributeChangeData& Data);
	void OnMaxManaChanged(const FOnAttributeChangeData& Data);
	
	void OnStaminaChanged(const FOnAttributeChangeData& Data);
	void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);
	
private:
	// === PawnExtension Binding (ASC ready) ===
	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundExt;

	FDelegateHandle H_OnAscReady;

	void BindToPawnExtension(URpgPawnExtensionComponent* Ext);
	void UnbindFromPawnExtension();
	void HandleAscReady(UAbilitySystemComponent* ReadyASC);
	
};
