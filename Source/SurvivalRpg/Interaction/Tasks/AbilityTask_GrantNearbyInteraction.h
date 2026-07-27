// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Abilities/Tasks/AbilityTask.h"

#include "AbilityTask_GrantNearbyInteraction.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;
class UObject;
struct FFrame;
struct FGameplayAbilitySpecHandle;
struct FObjectKey;

UCLASS()
class UAbilityTask_GrantNearbyInteraction : public UAbilityTask
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Wait until an overlap occurs. This will need to be better fleshed out so we can specify game specific collision requirements */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_GrantNearbyInteraction* GrantAbilitiesForNearbyInteractors(UGameplayAbility* OwningAbility, float InteractionScanRange, float InteractionScanRate);
	
	virtual void OnDestroy(bool AbilityEnded) override;

#if WITH_DEV_AUTOMATION_TESTS
	/** Creates an unactivated task bound directly to an ASC for synchronous lifecycle tests. */
	static UAbilityTask_GrantNearbyInteraction* CreateForTesting(
		UAbilitySystemComponent* InAbilitySystemComponent,
		float InInteractionScanRange);

	/** Reconciles an explicit multiset of required classes without depending on a physics scene. */
	void ReconcileAbilityClassesForTesting(const TArray<TSubclassOf<UGameplayAbility>>& RequiredAbilityClasses);

	/** Starts the production query timer without requiring an owning gameplay ability fixture. */
	void StartQueryTimerForTesting();

	/** Returns whether the periodic query timer is currently registered with the task world. */
	bool IsQueryTimerActiveForTesting() const;
#endif

protected:
	virtual void Activate() override;
	
private:
	void QueryInteractables();
	void StartQueryTimer();
	void ReconcileAbilities(TMap<TSubclassOf<UGameplayAbility>, int32> DesiredAbilityReferenceCounts);

	float InteractionScanRange = 100;
	float InteractionScanRate = 0.100f;

	FTimerHandle QueryTimerHandle;

	/** One temporary spec per required ability class, shared by every nearby target that references it. */
	TMap<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle> InteractionAbilityCache;

	/** Number of currently overlapping options that depend on each cached ability class. */
	TMap<TSubclassOf<UGameplayAbility>, int32> InteractionAbilityReferenceCounts;
};
