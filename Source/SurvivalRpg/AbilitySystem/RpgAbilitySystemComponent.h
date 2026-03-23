// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAbilitySet.h"

#include "RpgAbilitySystemComponent.generated.h"


class URpgAbilitySet;
class UGameplayAbility;

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
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	void ActivateAbilitiesByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation);

	UFUNCTION(BlueprintCallable, Category="RPG|Lifecycle")
	void ResetForRevive();

	UFUNCTION(BlueprintCallable, Category="RPG|Lifecycle")
	void ResetForRespawn();

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool TryActivateFirstAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bAllowRemoteActivation);

protected:
	virtual void BeginPlay() override;
	
	/** Server-RPCs */
	UFUNCTION(Server, Reliable)
	void Server_GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(Server, Reliable)
	void Server_RemoveAbilitySet(const URpgAbilitySet* AbilitySet);
	
	virtual void OnRep_ActivateAbilities() override;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System")
	TObjectPtr<const URpgAbilitySet> DefaultAbilitySetup;
	
	TArray<FGameplayAbilitySpec> LastActiveAbilities;
	
private:
	UPROPERTY(Transient)
	bool bDefaultSetupApplied = false;

	UPROPERTY(Transient)
	FRpgAbilitySet_GrantedHandles DefaultGrantedHandles;
	
	UPROPERTY()
	TMap<TObjectPtr<const URpgAbilitySet>, FRpgAbilitySet_GrantedHandles> GrantedAbilitySets;
	
	UPROPERTY()
	TObjectPtr<class ARpgPlayerState> OwnerPlayerState = nullptr;

	bool GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject);
	bool RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet);
	void ClearLifecycleTags();
	void ClearLifecycleEffects();
};
