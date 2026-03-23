// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgDeathComponent.generated.h"


struct FRpgOutOfHealthInfo;
class URpgHealthComponent;
class URpgAbilitySystemComponent;
class URpgDownedComponent;
class URpgGameplayAbility_SelfRevive;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgDeathComponent();
	
	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Rpg|Health")
	static URpgDeathComponent* FindDeathComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgDeathComponent>() : nullptr); }

	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	void InitializeWithAbilitySystem(URpgAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Rpg|Health")
	void UninitializeFromAbilitySystem();
	
protected:
	virtual void OnUnregister() override;

	UFUNCTION()
	virtual void HandleOutOfHealth(FRpgOutOfHealthInfo& Info);

	virtual bool ShouldEnterDowned() const;
	virtual bool TrySoloSelfRevive() const;
	bool HasOtherLivingPlayers() const;
	bool IsPlayerCharacter() const;
	
protected:
	
	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<URpgHealthComponent> HealthComponent;

	UPROPERTY()
	TObjectPtr<URpgDownedComponent> DownedComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Death")
	TSubclassOf<URpgGameplayAbility_SelfRevive> SoloSelfReviveAbilityClass;
};
