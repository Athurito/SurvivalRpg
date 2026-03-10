// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgDownedComponent.generated.h"


class URpgAbilitySystemComponent;
class URpgHealthComponent;

/**
 * Downed state for co-op revive mechanics.
 * Flow: Health reaches 0 → TryEnterDowned → BleedingOut (timer) → if not revived → real death.
 * Another player can BeginRevive → progress fills → CompleteRevive → character stands back up.
 */

UENUM(BlueprintType)
enum class ERpgDownedState : uint8
{
	/** Character is alive and well. */
	NotDowned = 0,

	/** Character is downed and bleeding out, waiting for revive. */
	Downed,

	/** Another player is actively reviving this character. */
	BeingRevived,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgDowned_StateChanged, ERpgDownedState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgDowned_ReviveProgress, float, Progress, float, TimeRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgDowned_ReviveEvent, AActor*, Reviver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgDowned_BleedoutExpired);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgDownedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgDownedComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Returns the downed component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	static URpgDownedComponent* FindDownedComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgDownedComponent>() : nullptr); }

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void UninitializeFromAbilitySystem();

	/**
	 * Attempts to enter the downed state instead of dying.
	 * Called by the HealthComponent when health reaches 0.
	 * @return true if the character entered downed state, false if they should die immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	bool TryEnterDowned();

	/**
	 * Forces the character out of downed state into real death.
	 * Called when bleedout expires or when the character cannot be saved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void ForceDeathFromDowned();

	/** Exits downed state cleanly (e.g. after revive). Restores tags. */
	void ExitDowned();

	// --- Revive API ---

	/** Called by the reviving player to start the revive interaction. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void BeginRevive(AActor* Reviver);

	/** Called when the reviving player cancels or moves away. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void CancelRevive();

	/** Called when revive progress reaches 100%. Restores the character. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void CompleteRevive();

	// --- Queries ---

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	bool IsDowned() const { return DownedState != ERpgDownedState::NotDowned; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	bool IsBeingRevived() const { return DownedState == ERpgDownedState::BeingRevived; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	ERpgDownedState GetDownedState() const { return DownedState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	float GetBleedoutTimeRemaining() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	float GetBleedoutNormalized() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	float GetReviveProgress() const { return ReviveProgress; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|Downed")
	AActor* GetCurrentReviver() const { return CurrentReviver.Get(); }

public:
	// --- Delegates ---

	/** Fired whenever the downed state changes. */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_StateChanged OnDownedStateChanged;

	/** Fired every tick while being revived, with current progress [0..1] and time remaining. */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveProgress OnReviveProgressChanged;

	/** Fired when revive starts. */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveStarted;

	/** Fired when revive is cancelled. */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveCancelled;

	/** Fired when revive completes successfully. */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveCompleted;

	/** Fired when bleedout timer expires (character will now truly die). */
	UPROPERTY(BlueprintAssignable)
	FRpgDowned_BleedoutExpired OnBleedoutExpired;

protected:
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetDownedState(ERpgDownedState NewState);
	void StartBleedoutTimer();
	void StopBleedoutTimer();
	void OnBleedoutTimerExpired();
	void ClearDownedTags();

private:
	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<URpgHealthComponent> HealthComponent = nullptr;

	ERpgDownedState DownedState = ERpgDownedState::NotDowned;

	// --- Bleedout ---

	/** Total time in seconds before a downed character dies if not revived. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Downed", meta = (ClampMin = "1.0"))
	float BleedoutDuration = 45.0f;

	FTimerHandle BleedoutTimerHandle;

	// --- Revive ---

	/** Time in seconds it takes to complete a revive. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Downed", meta = (ClampMin = "0.5"))
	float ReviveDuration = 5.0f;

	/** Health percentage [0..1] to restore after revive. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Downed", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ReviveHealthPercent = 0.3f;

	/** Current revive progress [0..1]. */
	float ReviveProgress = 0.0f;

	/** The actor currently performing the revive. */
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentReviver = nullptr;
};
