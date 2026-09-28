// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;
class UAnimationAsset;
class UChooserTable;


/**
 * URpgAnimInstance
 *
 * Shared animation base for RPG characters, including GAS tag bindings and network pose timing.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	explicit URpgAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

	/** Processes each remote autonomous pose tick immediately on a listen server, preserving its animation time. */
	virtual bool CanRunParallelWork() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Evaluates a designer-selected table against this instance on the game thread. Selection rules and output tuning remain in the table; only already-loaded animation assets are returned. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Selection")
	UAnimationAsset* EvaluateAnimationChooser(UChooserTable* Chooser);

	/** True only for an asset authored on this compiled AnimBP's target skeleton; does not infer retarget compatibility or load assets. */
	UFUNCTION(BlueprintPure, Category = "Animation|Validation")
	bool IsAnimationCompatible(const UAnimationAsset* Animation) const;

	/** Game-thread snapshot of the effective movement policy; animation may present it but never activate block. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	bool bBlockLocomotionActive = false;

	/** Actual planar velocity in the pawn's horizontal facing frame, in cm/s; includes simulated proxies. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	FVector BlockLocalVelocity = FVector::ZeroVector;

	/** Gameplay mesh component world yaw in degrees, including visual smoothing. Cosmetic turn compensation uses its deltas; a constant mesh basis cancels. Never feeds movement. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	float BlockFacingYaw = 0.f;

	/** Movement intent sampled from the applicable movement component, for cosmetic starts and interruptions. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	bool bBlockHasMovementInput = false;

	/** Whether this pawn's movement simulation is grounded; no world queries occur on animation workers. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	bool bBlockGrounded = false;

	/** Higher-priority lifecycle or movement state suppresses the cosmetic block layer. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	bool bBlockInterrupted = true;

	/** Effective activation-sampled limit in cm/s. Zero also means unavailable on CMC simulated proxies; playback must use actual velocity and authored reference speeds. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Block Animation")
	float BlockMovementSpeedLimit = 0.f;

protected:

	/**
	 * Optional designer-owned asset checks. Editor validation calls this on an uninitialized transient copy,
	 * never the asset CDO or a live pawn. Report each missing selection or incompatible asset in OutErrors;
	 * use only authored data and chooser inputs, without world, animation lifecycle, or asset mutations.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animation|Validation", meta = (DevelopmentOnly))
	void ValidateAnimationSet(TArray<FText>& OutErrors);

	// Gameplay tags that can be mapped to blueprint variables. The variables will automatically update as the tags are added or removed.
	// These should be used instead of manually querying for the gameplay tags.
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.0f;

private:
	/** Refreshes only value inputs on the game thread, including reinitialization of an existing linked instance. */
	void CaptureBlockAnimationState();
};
