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
	
	/** BP-friendly: kann von Client aufgerufen werden, läuft server-autoritatv */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool RemoveAbilitySet(const URpgAbilitySet* AbilitySet);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool HasAbilitySet(const URpgAbilitySet* AbilitySet) const;
	
	void ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject);
	void RemoveDefaultAbilitySetup();
	

protected:
	virtual void BeginPlay() override;
	
	/** Server-RPCs */
	UFUNCTION(Server, Reliable)
	void Server_GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(Server, Reliable)
	void Server_RemoveAbilitySet(const URpgAbilitySet* AbilitySet);
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<const URpgAbilitySet> DefaultAbilitySetup;
	
private:
	UPROPERTY(Transient)
	bool bDefaultSetupApplied = false;

	UPROPERTY(Transient)
	FRpgAbilitySet_GrantedHandles DefaultGrantedHandles;
	
	UPROPERTY()
	TMap<TObjectPtr<const URpgAbilitySet>, FRpgAbilitySet_GrantedHandles> GrantedAbilitySets;

	bool GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject);
	bool RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet);
};
