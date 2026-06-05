#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RpgAIAttackDriverComponent.generated.h"

class URpgAbilitySystemComponent;
class URpgHealthComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent, DisplayName = "AI Melee Attack Driver"))
class SURVIVALRPG_API URpgAIAttackDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgAIAttackDriverComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void SetCombatTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|AI Combat")
	void ClearCombatTarget();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rpg|AI Combat")
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

private:
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;
	bool IsOwnerAlive() const;
	bool IsValidCombatTarget(const AActor* Target) const;
	bool CanAttemptAttack() const;
	bool IsInAttackRangeAndCone(const AActor* Target) const;
	bool HasActiveMainHandPrimaryAttack() const;
	AActor* FindNearbyHostileTarget() const;
	float GetNextAttackInterval() const;
	void FaceTarget(AActor* Target) const;
	void StartTargetAcquisitionIfNeeded();
	void StopDriverActivity();
	void ScheduleNextAttack(float Delay);
	void ScheduleTargetAcquisition(float Delay);
	void TryAttack();
	void TryAcquireNearbyTarget();
	void ReleaseAttackInput();
	bool ShouldDebugLog() const;

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (Categories = "InputTag"))
	FGameplayTag AttackInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AttackConeDegrees = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float MinAttackInterval = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackIntervalRandomJitter = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float RetaliationDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float AttackRetryInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bAcquireNearbyHostileTargets = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.0"))
	float TargetAcquisitionRadius = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat", meta = (ClampMin = "0.05"))
	float TargetAcquisitionInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bUseLastHostileDamageInstigator = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|AI Combat")
	bool bFaceTargetBeforeAttack = true;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> CachedHealthComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CombatTarget;

	FTimerHandle AttackTimerHandle;
	FTimerHandle TargetAcquisitionTimerHandle;
	FTimerHandle ReleaseAttackInputTimerHandle;

	float LastAttackTime = -1000.0f;
	bool bAttackInputHeld = false;
};
