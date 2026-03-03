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
	void BindASC(UAbilitySystemComponent* InASC);
	void UnbindASC();

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetHealthPercent() const { return (MaxHealth > 0.f) ? Health / MaxHealth : 0.f; }

private:
	UPROPERTY(BlueprintReadOnly, FieldNotify, meta=(AllowPrivateAccess="true"))
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, meta=(AllowPrivateAccess="true"))
	float MaxHealth = 0.f;

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;

	void RefreshOnce();

	void SetHealth(float NewValue);
	void SetMaxHealth(float NewValue);
};
