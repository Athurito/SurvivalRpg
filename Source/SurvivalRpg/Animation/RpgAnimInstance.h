// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameplayEffectTypes.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;

/**
 * Game-thread snapshot consumed by URpgAnimInstance during parallel animation updates.
 *
 * UObject and movement-component access is deliberately confined to PreUpdate; the animation
 * worker thread only reads the plain values stored here.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FRpgAnimInstanceProxy() = default;
	explicit FRpgAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	FVector WorldVelocity = FVector::ZeroVector;
	FVector LocalVelocity = FVector::ZeroVector;
	FVector WorldAcceleration = FVector::ZeroVector;
	FVector LocalAcceleration = FVector::ZeroVector;
	float GroundSpeed = 0.0f;
	float VerticalVelocity = 0.0f;
	float GroundDistance = -1.0f;
	float AimYaw = 0.0f;
	float AimPitch = 0.0f;
	bool bHasVelocity = false;
	bool bHasAcceleration = false;
	bool bIsFalling = false;
	bool bIsMovingOnGround = false;
	bool bIsCrouching = false;
};


/**
 * URpgAnimInstance
 *
 * Thread-safe base animation instance shared by RPG locomotion and montage-capable character AnimBPs.
 */
UCLASS(Config = Game)
class SURVIVALRPG_API URpgAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	explicit URpgAnimInstance(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);

	/**
	 * Keeps listen-server copies of remote autonomous characters time-correct when several
	 * client moves tick their pose in one server frame; all regular animation work stays parallel.
	 */
	virtual bool CanRunParallelWork() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	/** Gameplay tags mirrored from the owning ASC into AnimBP variables; do not query the ASC from worker-thread graph logic. */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	/** Character velocity in world space, snapshotted on the game thread and read-only to AnimBPs. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector WorldVelocity = FVector::ZeroVector;

	/** Character velocity transformed into character-local space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector LocalVelocity = FVector::ZeroVector;

	/** Current movement input acceleration in world space, including replicated data for simulated proxies. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector WorldAcceleration = FVector::ZeroVector;

	/** Current movement input acceleration transformed into character-local space. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	FVector LocalAcceleration = FVector::ZeroVector;

	/** Horizontal speed in centimeters per second. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm/s"))
	float LocomotionGroundSpeed = 0.0f;

	/** Vertical velocity in centimeters per second; positive values move upward. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm/s"))
	float VerticalVelocity = 0.0f;

	/** Distance from the capsule bottom to the ground in centimeters; -1 means no valid owner snapshot. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion", Meta = (Units = "cm"))
	float GroundDistance = -1.0f;

	/** Controller aim yaw relative to the character in degrees, normalized to [-180, 180]. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Aim", Meta = (Units = "deg"))
	float AimYaw = 0.0f;

	/** Controller aim pitch relative to the character in degrees, normalized to [-180, 180]. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Aim", Meta = (Units = "deg"))
	float AimPitch = 0.0f;

	/** True when the velocity snapshot is non-zero. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bHasVelocity = false;

	/** True when movement input acceleration is present. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bHasAcceleration = false;

	/** True while CharacterMovement is in the falling movement mode. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bLocomotionIsFalling = false;

	/** True while CharacterMovement considers the character grounded. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bIsMovingOnGround = false;

	/** Replicated crouch state owned by the character. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Animation|Locomotion")
	bool bIsCrouching = false;
};
