#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Combat/RpgAIAttackDriverComponent.h"
#include "Tasks/StateTreeAITask.h"
#include "RpgStateTreeCombatTasks.generated.h"

/*
 * Server-side StateTree tasks for melee and ranged enemies. They compose the AI drivers (URpgAIAttackDriverComponent,
 * URpgAIBlockDriverComponent) that GF_Combat_Core adds to every AI character: the combat control task hands the drivers
 * to the StateTree and owns target choice, the others move, attack, block or wait against the drivers' combat target.
 * Parameters are designer tuning per StateTree; the remaining instance fields are runtime state.
 */

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeCombatControlTaskInstanceData
{
	GENERATED_BODY()

	FRpgStateTreeCombatControlTaskInstanceData();

	/** How the AI chooses among hostile pawns; Nearest or the last attacker. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	ERpgAICombatTargetPolicy TargetPolicy = ERpgAICombatTargetPolicy::LastAttacker;

	/** Minimum seconds between two target switches while both targets stay valid, against ping-pong in co-op. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float TargetSwitchCooldown = 4.0f;

	/** Radius in cm in which a new target is found. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float SearchRadius = 3000.0f;

	/** Distance in cm beyond which the current target is dropped. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float LoseRadius = 4000.0f;

	/** StateTree event sent once whenever the combat target starts an ability tagged Ability.Type.Attack. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag TargetAttackEventTag;

	/** The combat target, for bindings; null without one. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> CombatTarget = nullptr;

	uint32 LastSeenTargetAttackCount = 0;
};

/**
 * Global task: while active, the attack driver attacks only on the StateTree's command and the block driver no longer
 * blocks by chance after damage. It takes control as soon as the drivers exist, which GameFeature actions may add after
 * the StateTree starts. Every tick it refreshes the combat target, keeps the controller's focus on it for all combat
 * states, and turns each new attack of the target into the TargetAttackEventTag event. While active, reached move goals
 * brake instead of stopping dead. Ends all of this on exit.
 */
USTRUCT(meta = (DisplayName = "Combat Control", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeCombatControlTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeCombatControlTaskInstanceData;

	FRpgStateTreeCombatControlTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeKeepCombatDistanceTaskInstanceData
{
	GENERATED_BODY()

	/** Inner edge of the distance band to the combat target, in cm. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float MinDistance = 250.0f;

	/** Outer edge of the distance band to the combat target, in cm. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float MaxDistance = 400.0f;

	/** Walk instead of run; needs a movement profile on the character's movement component. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bWalk = true;

	/** Circle the target sideways while inside the band; otherwise hold position there. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStrafe = true;

	/** Angle in degrees each strafe move advances around the target. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees"))
	float StrafeAngleStep = 35.0f;

	/** Shortest and longest time in seconds before the strafe direction may flip. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", Units = "s"))
	float StrafeSwitchIntervalMin = 1.5f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", Units = "s"))
	float StrafeSwitchIntervalMax = 3.0f;

	/** Shortest and longest run time in seconds before the task succeeds; both zero runs until another rule ends it. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float DurationMin = 0.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float DurationMax = 0.0f;

	/** Succeed as soon as the distance lies inside the band, for approaching or backing off. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bSucceedInBand = false;

	/** Seconds between move requests. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.05", Units = "s"))
	float RepathInterval = 0.3f;

	float TimeLeft = 0.0f;
	float TimeUntilRepath = 0.0f;
	float TimeUntilStrafeSwitch = 0.0f;
	float StrafeSign = 1.0f;
};

/**
 * Moves into and keeps a distance band to the combat target: approaches from outside, backs off from inside, and circles
 * sideways when bStrafe is set. Facing comes from the combat control task, and the next state takes over the movement,
 * so the task never stops the pawn. Fails without a combat target.
 */
USTRUCT(meta = (DisplayName = "Keep Combat Distance", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeKeepCombatDistanceTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeKeepCombatDistanceTaskInstanceData;

	FRpgStateTreeKeepCombatDistanceTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeMeleeAttackTaskInstanceData
{
	GENERATED_BODY()

	/**
	 * Relative chance of a sequence of 1, 2, 3, ... combo steps; entry i weighs i + 1 steps. Designer tuning; the
	 * weapon's chain may end earlier than the chosen count.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<float> StepCountWeights = { 0.35f, 0.35f, 0.2f, 0.1f };

	/** Run instead of walk while closing in. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRunToTarget = true;

	/** Seconds to reach attack range before the task fails. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", Units = "s"))
	float ApproachTimeout = 3.0f;

	/** Share of the attack driver's range at which the attack starts, below 1 so the target stays in reach. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StartRangeFraction = 0.85f;

	int32 StepCount = 1;
	float TimeLeft = 0.0f;
	float TimeUntilRepath = 0.0f;
	bool bAttacking = false;
};

/**
 * Closes in on the combat target, then has the attack driver run one attack sequence of a weighted number of combo
 * steps and succeeds when the attack ability ends. Fails without a target, on timeout, or when the attack cannot start.
 */
USTRUCT(meta = (DisplayName = "Melee Attack Sequence", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeMeleeAttackTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeMeleeAttackTaskInstanceData;

	FRpgStateTreeMeleeAttackTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Picks a step count from the weights; RandomValue in [0, 1). One step when no weight is positive. */
	static int32 PickStepCount(TConstArrayView<float> StepCountWeights, float RandomValue);
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeRangedAttackTaskInstanceData
{
	GENERATED_BODY()

	/**
	 * Shortest and longest time in seconds the full draw is held before the release, so the target can read the shot
	 * and dodge it. A new combat target during the draw gets a new hold. Designer tuning.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float AimHoldMin = 0.4f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float AimHoldMax = 0.8f;

	/** Seconds the target may stay out of the line of fire before the drawn shot is lowered and the task fails. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float LineOfFireGrace = 1.0f;

	/**
	 * Seconds from the start of the draw until an unreleased shot is lowered and the task fails. Keep it above the
	 * weapon's full draw time plus AimHoldMax, or the archer never shoots.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1", Units = "s"))
	float Timeout = 6.0f;

	float HoldLeft = 0.0f;
	/** The combat target HoldLeft was rolled for. */
	TWeakObjectPtr<AActor> HoldTarget;
	float TimeOutOfLine = 0.0f;
	float TimeLeft = 0.0f;
	bool bReleased = false;
};

/**
 * Stops, draws the main-hand ranged weapon's shot through the attack driver, holds the full draw for a readable moment
 * and releases while the combat target is in the line of fire. When the combat target changes during the draw, such as
 * to the next hostile after the old target died or to a player who hit the archer, the new target gets its own hold. Meanwhile it points the controller's view at the
 * ballistic aim above the target. Succeeds when the released shot's ability ends. Fails without a target or a line of
 * fire at the start, when the shot cannot start or is interrupted, after LineOfFireGrace without a line of fire, or on
 * timeout; an unreleased shot is lowered without cost.
 */
USTRUCT(meta = (DisplayName = "Ranged Attack", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeRangedAttackTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeRangedAttackTaskInstanceData;

	FRpgStateTreeRangedAttackTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeHoldBlockTaskInstanceData
{
	GENERATED_BODY()

	/** Shortest and longest block in seconds. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.05", Units = "s"))
	float DurationMin = 0.6f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.05", Units = "s"))
	float DurationMax = 1.0f;
};

/** Stops and raises the block through the block driver, facing the combat target, and succeeds when it is released. */
USTRUCT(meta = (DisplayName = "Hold Block", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeHoldBlockTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeHoldBlockTaskInstanceData;

	FRpgStateTreeHoldBlockTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeWaitWhileOwnerTaggedTaskInstanceData
{
	GENERATED_BODY()

	FRpgStateTreeWaitWhileOwnerTaggedTaskInstanceData();

	/** The task runs while the pawn's ability system holds any of these tags, for example State.Staggered. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer Tags;
};

/** Stops movement and waits until the pawn no longer holds any of the tags, then succeeds. */
USTRUCT(meta = (DisplayName = "Wait While Tagged", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeWaitWhileOwnerTaggedTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeWaitWhileOwnerTaggedTaskInstanceData;

	FRpgStateTreeWaitWhileOwnerTaggedTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
