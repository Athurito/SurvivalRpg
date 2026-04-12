// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RpgAbilitySet.h"
#include "Abilities/RpgGameplayAbility.h"

#include "RpgAbilitySystemComponent.generated.h"


class URpgAbilityTagRelationshipMapping;
class URpgAbilitySet;
class UGameplayAbility;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
	
	
public:
	explicit URpgAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	void TryActivateAbilitiesOnSpawn();

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	// If set, this table is used to look up tag relationships for activate and cancel
	UPROPERTY()
	TObjectPtr<URpgAbilityTagRelationshipMapping> TagRelationshipMapping;

	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// Number of abilities running in each activation group.
	int32 ActivationGroupCounts[static_cast<uint8>(ERpgAbilityActivationGroup::MAX)];
	
private:
	
	

public:
	
	/** BP-friendly: kann von Client aufgerufen werden, läuft server-autoritatv */
	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool RemoveAbilitySet(const URpgAbilitySet* AbilitySet);

	UFUNCTION(BlueprintCallable, Category="RPG|AbilitySet")
	bool HasAbilitySet(const URpgAbilitySet* AbilitySet) const;

	bool HasGrantAuthority() const;
	
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

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetForceGrantAuthorityForTests(bool bInForceGrantAuthority) { bForceGrantAuthorityForTests = bInForceGrantAuthority; }

private:
	bool bForceGrantAuthorityForTests = false;
#endif

	bool GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject);
	bool RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet);
	void ClearLifecycleTags();
	void ClearLifecycleEffects();
};
