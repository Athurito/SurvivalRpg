#pragma once

#include "CoreMinimal.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "GameplayTagContainer.h"
#include "RpgStateTreeCombatConditions.generated.h"

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeCombatTargetDistanceConditionInstanceData
{
	GENERATED_BODY()

	/** Smallest planar distance in cm to the attack driver's combat target that passes. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistance = 0.0f;

	/** Largest planar distance in cm that passes; negative means no upper bound. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Units = "cm"))
	float MaxDistance = -1.0f;
};

/**
 * Passes while the AI has a combat target within the distance band. With no bounds it only asks whether there is a
 * target. Server-side, reads the pawn's URpgAIAttackDriverComponent.
 */
USTRUCT(meta = (DisplayName = "Combat Target Distance", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeCombatTargetDistanceCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeCombatTargetDistanceConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** Fails when there is a target in the band, passes otherwise, also without a target. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeOwnerHasTagsConditionInstanceData
{
	GENERATED_BODY()

	/** Passes when the pawn's ability system holds any of these tags. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};

/** Passes while the controlled pawn holds any of the tags, for example State.Staggered. Server-side. */
USTRUCT(meta = (DisplayName = "Owner Has Tags", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeOwnerHasTagsCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeOwnerHasTagsConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	/** Passes when the pawn holds none of the tags. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};
