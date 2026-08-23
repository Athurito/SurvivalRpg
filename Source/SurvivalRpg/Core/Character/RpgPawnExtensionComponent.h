// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "RpgPawnExtensionComponent.generated.h"


class URpgPawnData;
class UAbilitySystemComponent;
class URpgAbilitySystemComponent;

/**
 * Core pawn extension used by player and AI pawns.
 *
 * It owns pawn init-state coordination and binds an externally owned ASC to this pawn as avatar.
 * Ability grants stay in PawnData/PlayerState paths; this component only connects the avatar.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
	
public:
	URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);
	
	/** The name of this component-implemented actor feature. */
	static const FName Name_ActorFeatureName;

	//~ IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return Name_ActorFeatureName; };
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface
	
	/** Returns the pawn extension component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	static URpgPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgPawnExtensionComponent>() : nullptr); }
	
	/** Gets the pawn data, which is used to specify pawn properties in data */
	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }
	
	/** Sets the current pawn data. Authority-only and expected to happen once per pawn. */
	void SetPawnData(const URpgPawnData* InPawnData);

	/** Registers for PawnData-ready notifications and executes immediately when data already exists. */
	void OnPawnDataReady_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);
	
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

	/** Delegate fired whenever replicated or authority-assigned PawnData becomes locally available. */
	FSimpleMulticastDelegate OnPawnDataReady;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void OnRep_PawnData();
	
	/** Pointer to the ability system component that is cached for convenience. */
	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
	
	/** Pawn data used to configure this pawn instance. Set during spawn or on placed instances. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Lyra|Pawn")
	TObjectPtr<const URpgPawnData> PawnData;
};
