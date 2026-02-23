// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "RpgPawnExtensionComponent.generated.h"


class UBasePawnData;
class UAbilitySystemComponent;
class URpgAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	/** IGameFrameworkInitStateInterface start **/
	static const FName Name_ActorFeatureName;
	virtual FName GetFeatureName() const override { return Name_ActorFeatureName; };
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	/** IGameFrameworkInitStateInterface end **/
	
public:
	void InitializeAbilitySystemComponent(URpgAbilitySystemComponent* InAsc, AActor* InOwner);
	void UninitializeAbilitySystemComponent();

public:
	// Sets default values for this component's properties
	URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);
	
	/** Returns the pawn extension component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	static URpgPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgPawnExtensionComponent>() : nullptr); }
	
	/** Gets the current ability system component, which may be owned by a different actor */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const { return AbilitySystemComponent; }

	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }
	void SetPawnData(const UBasePawnData* InPawnData);
	
	void SetupPlayerInputComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnRegister() override;
	
	/** Pointer to the ability system component that is cached for convenience. */
	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

private:
	UPROPERTY()
	TObjectPtr<const UBasePawnData> PawnData;
};
