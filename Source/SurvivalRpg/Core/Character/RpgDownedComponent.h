// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgDownedComponent.generated.h"

class URpgAbilitySystemComponent;
class URpgHealthComponent;

UENUM(BlueprintType)
enum class ERpgDownedState : uint8
{
	NotDowned = 0,
	Downed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgDowned_StateChanged, ERpgDownedState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgDowned_ReviveEvent, AActor*, Reviver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgDowned_BleedoutExpired);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgDownedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgDownedComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	static URpgDownedComponent* FindDownedComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgDownedComponent>() : nullptr); }
	
	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void UninitializeFromAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	bool TryEnterDowned();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void ForceDeathFromDowned();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	bool CanBeRevivedBy(const AActor* Reviver) const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	bool BeginRevive(AActor* Reviver);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void CancelRevive(AActor* Reviver);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Downed")
	void CompleteRevive(AActor* Reviver);

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	bool IsDowned() const;

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	bool IsBeingRevived() const { return CurrentReviver.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	ERpgDownedState GetDownedState() const { return DownedState; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	AActor* GetCurrentReviver() const { return CurrentReviver.Get(); }

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	float GetBleedoutTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	float GetBleedoutNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	float GetReviveHealthPercent() const { return ReviveHealthPercent; }

	UFUNCTION(BlueprintPure, Category = "Rpg|Downed")
	bool IsInitialized() const { return (AbilitySystemComponent != nullptr) && (HealthComponent != nullptr); }

	UPROPERTY(BlueprintAssignable)
	FRpgDowned_StateChanged OnDownedStateChanged;

	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveStarted;

	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveCancelled;

	UPROPERTY(BlueprintAssignable)
	FRpgDowned_ReviveEvent OnReviveCompleted;

	UPROPERTY(BlueprintAssignable)
	FRpgDowned_BleedoutExpired OnBleedoutExpired;

protected:
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_DownedState(ERpgDownedState OldState);

	void ExitDowned();
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

	UPROPERTY(ReplicatedUsing = OnRep_DownedState)
	ERpgDownedState DownedState = ERpgDownedState::NotDowned;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Downed", meta = (ClampMin = "1.0"))
	float BleedoutDuration = 45.0f;

	FTimerHandle BleedoutTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Downed", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ReviveHealthPercent = 0.3f;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentReviver = nullptr;

	bool bPendingDeath = false;
};
