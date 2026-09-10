// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "RpgAnimInstance.generated.h"

class UAbilitySystemComponent;


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

protected:

	// Gameplay tags that can be mapped to blueprint variables. The variables will automatically update as the tags are added or removed.
	// These should be used instead of manually querying for the gameplay tags.
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	UPROPERTY(BlueprintReadOnly, Category = "Character State Data")
	float GroundDistance = -1.0f;
};
