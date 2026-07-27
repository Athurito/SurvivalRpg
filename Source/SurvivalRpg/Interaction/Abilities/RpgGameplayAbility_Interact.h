// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"

#include "RpgGameplayAbility_Interact.generated.h"

class UAbilityTask_GrantNearbyInteraction;
class UAbilityTask_WaitForInteractableTargets_FocusSweep;
class UAbilityTask_WaitForInteractableTargets_Nearby;
class UAbilityTask_WaitInputPress;
class UIndicatorDescriptor;
class URpgInteractionPromptData;
class URpgIndicatorManagerComponent;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgInteractionFocusChanged, bool, bHasFocusedOption, const FInteractionOption&, FocusedOption);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgInteractionPromptStateChanged, ERpgInteractionPromptState, PromptState, const FInteractionOption&, FocusedOption);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgNearbyInteractionsChanged, const TArray<FInteractionOption>&, NearbyOptions);

/**
 * Persistent Lyra-style interaction ability that owns focus, nearby presentation, temporary grants, and input routing.
 * The owning client presents options; authority independently re-scans and validates before triggering gameplay.
 */
UCLASS(Abstract)
class SURVIVALRPG_API URpgGameplayAbility_Interact : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Compatibility entry point for legacy GA_Interaction Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction")
	void UpdateInteractions(const TArray<FInteractionOption>& InteractiveOptions);

	/** Requests execution of the current option; gameplay is ignored unless this instance has authority. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction")
	void TriggerInteraction();

	/** Emitted only when the stable focused option changes or disappears. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Interaction")
	FRpgInteractionFocusChanged OnFocusedOptionChanged;

	/** Emitted when the focused option moves between out-of-range, blocked, and ready. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Interaction")
	FRpgInteractionPromptStateChanged OnPromptStateChanged;

	/** Emitted when the bounded nearby marker set changes semantically. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Interaction")
	FRpgNearbyInteractionsChanged OnNearbyOptionsChanged;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Maximum camera-focus sweep distance in centimeters. Option focus ranges are clamped by this scan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Scan", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractionScanRange = 650.0f;

	/** Focus sweep interval in seconds. The default is 20 Hz and does not use Actor tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Scan", meta = (ClampMin = "0.01", Units = "s"))
	float InteractionScanRate = 0.05f;

	/** Maximum local nearby-marker and server ability-grant radius in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Scan", meta = (ClampMin = "0.0", Units = "cm"))
	float AwarenessScanRange = 1000.0f;

	/** Nearby-marker and ability-grant query interval in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Scan", meta = (ClampMin = "0.05", Units = "s"))
	float NearbyScanRate = 0.25f;

	/** Radius of the camera sphere sweep in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Scan", meta = (ClampMin = "0.0", Units = "cm"))
	float FocusSweepRadius = 12.0f;

	/** Maximum number of focus hits scored per scan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Performance", meta = (ClampMin = "1"))
	int32 MaxFocusCandidates = 32;

	/** Maximum number of non-focused nearby markers kept alive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|Performance", meta = (ClampMin = "1"))
	int32 MaxNearbyIndicators = 12;

	/** Default projected widget used by focused options without an override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|UI")
	TSoftClassPtr<UUserWidget> DefaultInteractionWidgetClass;

	/** Default projected widget used by nearby options without an override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Interaction|UI")
	TSoftClassPtr<UUserWidget> DefaultNearbyWidgetClass;

	/** Current focused option retained for legacy Blueprint reads. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Interaction")
	TArray<FInteractionOption> CurrentOptions;

private:
	UFUNCTION()
	void HandleFocusedOptionsChanged(const TArray<FInteractionOption>& InteractiveOptions);

	UFUNCTION()
	void HandleNearbyOptionsChanged(const TArray<FInteractionOption>& InteractiveOptions);

	UFUNCTION()
	void HandleInputPressed(float TimeWaited);

	void StartWaitingForInput();
	void RefreshInteractionIndicators();
	void ClearInteractionIndicators();
	bool TriggerValidatedInteraction(const FInteractionOption& FocusedOption);
	static FString MakeOptionKey(const FInteractionOption& Option);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitForInteractableTargets_FocusSweep> FocusTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitForInteractableTargets_Nearby> NearbyTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_GrantNearbyInteraction> GrantTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> InputPressTask;

	UPROPERTY()
	TArray<FInteractionOption> CurrentNearbyOptions;

	UPROPERTY()
	TObjectPtr<UIndicatorDescriptor> FocusIndicator;

	UPROPERTY()
	TObjectPtr<URpgInteractionPromptData> FocusPromptData;

	UPROPERTY()
	TMap<FString, TObjectPtr<UIndicatorDescriptor>> NearbyIndicators;

	UPROPERTY()
	TMap<FString, TObjectPtr<URpgInteractionPromptData>> NearbyPromptData;

	ERpgInteractionPromptState LastPromptState = ERpgInteractionPromptState::Hidden;
	float LastAuthorityTriggerTimeSeconds = -1.0f;
};
