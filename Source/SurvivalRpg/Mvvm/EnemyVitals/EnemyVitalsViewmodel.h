#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "EnemyVitalsViewmodel.generated.h"

class AActor;
class UAbilitySystemComponent;
class URpgPawnExtensionComponent;
struct FOnAttributeChangeData;

/**
 * A per-widget vitals view model for a world actor such as an enemy. It mirrors the health attributes of the
 * actor's ability system and follows the pawn extension when that ability system is initialized or torn down.
 * It only reads replicated attributes; the server stays the authority over them.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API UEnemyVitalsViewmodel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Observes InObservedActor's ability system, through its pawn extension when it has one. Null stops observing. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Vitals")
	void BindToActor(AActor* InObservedActor);

	/** Stops observing the actor. The last health values stay until the next bind. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Vitals")
	void UnbindFromActor();

	/** Health over maximum health in [0, 1]; 0 while the maximum is unknown. Derived, UI read-only. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Rpg|UI|Vitals")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

protected:
	virtual void BeginDestroy() override;

private:
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();

	void BindAbilitySystem(UAbilitySystemComponent* InAbilitySystem);
	void UnbindAbilitySystem();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);

	void SetHealth(float NewValue);
	void SetMaxHealth(float NewValue);

	/** Current health of the observed actor, mirrored from URpgHealthSet. Runtime, UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Rpg|UI|Vitals", meta = (AllowPrivateAccess = "true"))
	float Health = 0.f;

	/** Maximum health of the observed actor, mirrored from URpgHealthSet. Runtime, UI read-only. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Rpg|UI|Vitals", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.f;

	TWeakObjectPtr<AActor> ObservedActor;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};
