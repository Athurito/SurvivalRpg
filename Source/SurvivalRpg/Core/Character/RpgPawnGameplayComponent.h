// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddInputContextMapping.h"

#include "RpgPawnGameplayComponent.generated.h"


class URpgCameraMode;
class URpgInputConfig;
class URpgPawnData;
class URpgAbilitySystemComponent;
struct FGameplayTag;
struct FInputActionValue;

/**
 * Player-only pawn gameplay component.
 *
 * This is the Lyra HeroComponent-style layer for local input, camera selection, routed gameplay hotkeys,
 * and player ASC avatar binding. AI pawns should use PawnExtension directly instead.
 */
UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPawnGameplayComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()
public:
	
	/** The extension event sent when ability inputs are ready to bind. */
	static const FName NAME_BindInputsNow;
	/** The name of this component-implemented actor feature. */
	static const FName Name_ActorFeatureName;

	//~ IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return Name_ActorFeatureName; };
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface

public:
	explicit URpgPawnGameplayComponent(const FObjectInitializer& ObjectInitializer);
	
	/** Returns the hero component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Hero")
	static URpgPawnGameplayComponent* FindPawnGameplayComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<URpgPawnGameplayComponent>() : nullptr); }
	
	/** Overrides the camera from an active gameplay ability */
	void SetAbilityCameraMode(TSubclassOf<URpgCameraMode> CameraMode, const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** Clears the camera override if it is set */
	void ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** Adds mode- or feature-specific input config after the base pawn input is ready. */
	void AddAdditionalInputConfig(const URpgInputConfig* InputConfig);
	/** Removes a mode- or feature-specific input config if it has been added. */
	void RemoveAdditionalInputConfig(const URpgInputConfig* InputConfig);
	/** True when this locally controlled player pawn is ready for additional ability input bindings. */
	bool IsReadyToBindInputs() const;
	
	
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface
	
	/** Initializes Enhanced Input and binds native plus ability input actions for the owning local player. */
	virtual void InitializePlayerInput(UInputComponent* PlayerInputComponent);

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_LookMouse(const FInputActionValue& InputActionValue);
	void Input_LookStick(const FInputActionValue& InputActionValue);
	void Input_Crouch(const FInputActionValue& InputActionValue);
	void Input_AutoRun(const FInputActionValue& InputActionValue);
	void Input_Jump(const FInputActionValue& InputActionValue);
	void Input_StopJump(const FInputActionValue& InputActionValue);
	void Input_GameplayHotkeyPressed(FGameplayTag InputTag);
	void Input_GameplayHotkeyReleased(FGameplayTag InputTag);
	void Input_QuickAccessRadialStarted(const FInputActionValue& InputActionValue);
	void Input_QuickAccessRadialCompleted(const FInputActionValue& InputActionValue);
	void Input_QuickAccessRadialCanceled(const FInputActionValue& InputActionValue);
	void Input_QuickAccessRadialSelection(const FInputActionValue& InputActionValue);
	void Input_QuickAccessRadialSelectionEnded(const FInputActionValue& InputActionValue);
	
	TSubclassOf<URpgCameraMode> DetermineCameraMode() const;

protected:

	virtual void OnRegister() override;
	
protected:
	
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FRpgInputMappingContextAndPriority> DefaultInputMappings;
	
	/** Camera mode set by an ability. */
	UPROPERTY()
	TSubclassOf<URpgCameraMode> AbilityCameraMode;

	/** Spec handle for the last ability to set a camera mode. */
	FGameplayAbilitySpecHandle AbilityCameraModeOwningSpecHandle;

	/** True after player input bindings have been applied. This should remain false for AI pawns. */
	bool bReadyToBindInputs = false;

private:
	void HandleAbilitySystemUninitialized();
	void BindRoutedGameplayHotkeys(const URpgInputConfig* InputConfig, class URpgInputComponent* RpgIC, TArray<uint32>* BindHandles = nullptr);
	class URpgPlayerGameplayInputRouterComponent* GetGameplayInputRouter() const;
	static bool IsRoutedGameplayHotkeyTag(FGameplayTag InputTag);

	TMap<const URpgInputConfig*, TArray<uint32>> AdditionalInputConfigBindHandles;
};
