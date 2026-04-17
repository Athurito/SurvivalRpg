// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"

#include "RpgPawnGameplayComponent.generated.h"


class URpgInputConfig;
class URpgPawnData;
class URpgAbilitySystemComponent;
struct FGameplayTag;
struct FInputActionValue;

USTRUCT()
struct FRpgPawnGameplayAbilitySetGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const URpgAbilitySet> AbilitySet = nullptr;

	UPROPERTY()
	FRpgAbilitySet_GrantedHandles GrantedHandles;
};

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnGameplayComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
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
	// Sets default values for this component's properties
	explicit URpgPawnGameplayComponent(const FObjectInitializer& ObjectInitializer);
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	void Input_AbilityInputTagPressed(FGameplayTag InputTag) const;
	void Input_AbilityInputTagReleased(FGameplayTag InputTag) const;

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_LookMouse(const FInputActionValue& InputActionValue);
	void Input_LookStick(const FInputActionValue& InputActionValue);
	void Input_Crouch(const FInputActionValue& InputActionValue);
	void Input_AutoRun(const FInputActionValue& InputActionValue);
	void Input_Jump(const FInputActionValue& InputActionValue);
	void Input_StopJump(const FInputActionValue& InputActionValue);

protected:

	virtual void OnRegister() override;

private:
	void GrantPawnDataAbilitySets(URpgAbilitySystemComponent* AbilitySystemComponent, const URpgPawnData* PawnData, APawn* Pawn);
	void ResetCurrentHealthToMaxHealth(URpgAbilitySystemComponent* AbilitySystemComponent) const;
	void RemovePawnDataAbilitySets();
	void HandleAbilitySystemUninitialized();

	UPROPERTY()
	TArray<FRpgPawnGameplayAbilitySetGrant> GrantedPawnAbilitySets;

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> GrantedAbilitySystemComponent = nullptr;
};
