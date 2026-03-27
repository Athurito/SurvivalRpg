// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgAttributeSet.h"
#include "RpgHealthComponent.generated.h"


struct FRpgOutOfHealthInfo;
class UGameplayEffect;
class URpgHealthSet;
class URpgAbilitySystemComponent;
struct FGameplayEffectSpec;



USTRUCT()
struct FRpgOutOfHealthInfo
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* DamageInstigator = nullptr;
	UPROPERTY()
	AActor* DamageCauser = nullptr;
	const FGameplayEffectSpec* DamageEffectSpec = nullptr;
	float DamageMagnitude = 0.0f;
	float OldValue = 0.0f;
	float NewValue = 0.0f;
};

UENUM(BlueprintType)
enum class ERpgDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgHealth_DeathEvent, AActor*, OwningActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FRpgHealth_AttributeChanged, URpgHealthComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);
DECLARE_MULTICAST_DELEGATE_OneParam(FRpgOutOfHealthSignature, FRpgOutOfHealthInfo&);
/**
 * URpgHealthComponent
 *
 *	An actor component used to handle anything related to health.
 */

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgHealthComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	
	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Rpg|Health")
	static URpgHealthComponent* FindHealthComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgHealthComponent>() : nullptr); }

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	void InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	void UninitializeFromAbilitySystem();

	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	float GetHealth() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	float GetMaxHealth() const;

	// Returns the current health in the range [0.0, 1.0].
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	float GetHealthNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	ERpgDeathState GetDeathState() const { return DeathState; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Health", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > ERpgDeathState::NotDead); }

	// Begins the death sequence for the owner.
	virtual void StartDeath();

	// Ends the death sequence for the owner.
	virtual void FinishDeath();

	// Applies enough damage to kill the owner.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Health")
	virtual void DamageSelfDestruct(bool bFellOutOfWorld = false);
	
public:

	// Delegate fired when the health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FRpgHealth_AttributeChanged OnHealthChanged;

	// Delegate fired when the max health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FRpgHealth_AttributeChanged OnMaxHealthChanged;

	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FRpgHealth_DeathEvent OnDeathStarted;

	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FRpgHealth_DeathEvent OnDeathFinished;
	
	FRpgOutOfHealthSignature OnOutOfHealth;


protected:
	virtual void OnUnregister() override;

	void ApplyDeathGameplayTags(ERpgDeathState StateToApply) const;
	void ClearGameplayTags();

	virtual void HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);

	UFUNCTION()
	virtual void OnRep_DeathState(ERpgDeathState OldDeathState);
	
protected:

	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	// Health set used by this component.
	UPROPERTY()
	TObjectPtr<const URpgHealthSet> HealthSet;

	// Replicated state used to handle dying.
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	ERpgDeathState DeathState;
	
	
private:
	
	// Gameplay effect used to apply damage.  Uses SetByCaller for the damage magnitude.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "Damage Gameplay Effect (SetByCaller)"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffect_SetByCaller;

	// Gameplay effect used to apply healing.  Uses SetByCaller for the healing magnitude.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "Heal Gameplay Effect (SetByCaller)"))
	TSubclassOf<UGameplayEffect> HealGameplayEffect_SetByCaller;

	// Gameplay effect used to add and remove dynamic tags.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects")
	TSubclassOf<UGameplayEffect> DynamicTagGameplayEffect;
};
