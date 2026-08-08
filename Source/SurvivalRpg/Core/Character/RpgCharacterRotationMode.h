// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "RpgCharacterRotationMode.generated.h"

/**
 * Character rotation intent spanning exploration and controller-facing combat modes.
 *
 * The server owns the replicated mode. Autonomous proxies may temporarily resolve predicted
 * GAS activation-owned tags for cosmetic presentation, while simulated proxies consume only
 * the replicated value.
 */
UENUM(BlueprintType)
enum class ERpgCharacterRotationMode : uint8
{
	/** Camera yaw is independent while movement direction turns the character. */
	Free,

	/** Character yaw follows control rotation for directional combat locomotion. */
	CombatStrafe,

	/** Highest-priority controller-facing mode reserved for explicit aiming. */
	Aim
};

/** Pure movement-component policy associated with one rotation mode. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCharacterRotationPolicy
{
	GENERATED_BODY()

	/** Whether the character directly consumes controller yaw. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Character|Rotation")
	bool bUseControllerRotationYaw = true;

	/** Whether CharacterMovement rotates toward acceleration. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Character|Rotation")
	bool bOrientRotationToMovement = false;

	/** Whether CharacterMovement independently rotates toward controller yaw. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Character|Rotation")
	bool bUseControllerDesiredRotation = false;

	/** CharacterMovement yaw rate in degrees per second; -1 means effectively immediate. */
	UPROPERTY(BlueprintReadOnly, Category = "Rpg|Character|Rotation")
	float RotationRateYaw = 720.0f;
};
