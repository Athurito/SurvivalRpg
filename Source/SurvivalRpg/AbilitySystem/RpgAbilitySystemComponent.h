// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAbilitySet.h"

#include "RpgAbilitySystemComponent.generated.h"


class URpgAbilitySet;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URpgAbilitySystemComponent();
	
	void ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject);
	void RemoveDefaultAbilitySetup();
	

protected:
	virtual void BeginPlay() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<const URpgAbilitySet> DefaultAbilitySetup;
	
private:
	UPROPERTY(Transient)
	bool bDefaultSetupApplied = false;

	UPROPERTY(Transient)
	FRpgAbilitySet_GrantedHandles DefaultGrantedHandles;
};
