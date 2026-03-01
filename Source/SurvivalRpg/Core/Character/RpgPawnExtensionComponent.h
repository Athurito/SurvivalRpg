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
	URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);
	
	//~ IGameFrameworkInitStateInterface start
	static const FName Name_ActorFeatureName;
	virtual FName GetFeatureName() const override { return Name_ActorFeatureName; };
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ IGameFrameworkInitStateInterface end
	
	/** Returns the pawn extension component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	static URpgPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgPawnExtensionComponent>() : nullptr); }
	
	/** Gets the pawn data, which is used to specify pawn properties in data */
	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }
	
	/** Sets the current pawn data */
	void SetPawnData(const UBasePawnData* InPawnData);
	
	/** Gets the current ability system component, which may be owned by a different actor */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const { return AbilitySystemComponent; }
	
	/** Should be called by the owning pawn to become the avatar of the ability system. */
	void InitializeAbilitySystemComponent(URpgAbilitySystemComponent* InAsc, AActor* InOwner);
	
	/** Should be called by the owning pawn to remove itself as the avatar of the ability system. */
	void UninitializeAbilitySystem();
	
	/** Should be called by the owning pawn when the pawn's controller changes. */
	void HandleControllerChanged();

	/** Should be called by the owning pawn when the player state has been replicated. */
	void HandlePlayerStateReplicated();

	/** Should be called by the owning pawn when the input component is setup. */
	void SetupPlayerInputComponent();

	/** Register with the OnAbilitySystemInitialized delegate and broadcast if our pawn has been registered with the ability system component */
	void OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);

	/** Register with the OnAbilitySystemUninitialized delegate fired when our pawn is removed as the ability system's avatar actor */
	void OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate);
	
	/** Delegate fired when our pawn becomes the ability system's avatar actor */
	FSimpleMulticastDelegate OnAbilitySystemInitialized;

	/** Delegate fired when our pawn is removed as the ability system's avatar actor */
	FSimpleMulticastDelegate OnAbilitySystemUninitialized;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void OnRep_PawnData();
	
	/** Pointer to the ability system component that is cached for convenience. */
	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Lyra|Pawn")
	TObjectPtr<const UBasePawnData> PawnData;
};
