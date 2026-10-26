#include "RpgStateTreeCombatTasks.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "StateTreeExecutionContext.h"
#include "SurvivalRpg/Combat/RpgAIBlockDriverComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStateTreeCombatTasks)

namespace RpgStateTreeCombat
{
	AAIController* ResolveController(const FStateTreeExecutionContext& Context)
	{
		return Cast<AAIController>(Context.GetOwner());
	}

	/** The controlled pawn on the server while it lives; null otherwise. */
	APawn* ResolveLivingPawn(const AAIController* Controller)
	{
		APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || !Pawn->HasAuthority())
		{
			return nullptr;
		}
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Pawn);
		return Health && Health->IsDeadOrDying() ? nullptr : Pawn;
	}

	URpgAIAttackDriverComponent* ResolveAttackDriver(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<URpgAIAttackDriverComponent>() : nullptr;
	}

	AActor* ResolveCombatTarget(const APawn* Pawn)
	{
		const URpgAIAttackDriverComponent* Driver = ResolveAttackDriver(Pawn);
		return Driver ? Driver->GetCombatTarget() : nullptr;
	}

	float PlanarDistance(const AActor& A, const AActor& B)
	{
		return FVector::Dist2D(A.GetActorLocation(), B.GetActorLocation());
	}

	/** Walking needs the movement profile; toggles only when the wish differs, because the toggle is owner input. */
	void SetWalking(const APawn& Pawn, const bool bWalk)
	{
		if (URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Pawn.GetMovementComponent()))
		{
			if (Movement->IsWalkToggled() != bWalk)
			{
				Movement->ToggleWalk();
			}
		}
	}

	float RandomInRange(const float Min, const float Max)
	{
		return FMath::FRandRange(FMath::Min(Min, Max), FMath::Max(Min, Max));
	}

	/** Goals nearer than this in cm are not requested: the engine would finish them at once and zero the velocity. */
	constexpr float MinimumMoveDistance = 40.0f;

	/**
	 * Requests a move to Goal without the engine's hard stops. AAIController::MoveTo finishes a request that fails, for
	 * example on a goal off the navmesh, or that counts as reached, which includes the capsule radius, at once and zeroes
	 * the velocity. So the goal is projected first and must lie farther than MinimumMoveDistance, it counts as reached
	 * only at the goal itself, and a reached goal brakes. False when the goal is not on the navmesh; the running move
	 * then continues.
	 */
	bool RequestCombatMove(AAIController& Controller, const APawn& Pawn, const FVector& Goal)
	{
		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller.GetWorld());
		FNavLocation Projected;
		if (!NavigationSystem ||
			!NavigationSystem->ProjectPointToNavigation(Goal, Projected, INVALID_NAVEXTENT, &Controller.GetNavAgentPropertiesRef()))
		{
			return false;
		}
		if (FVector::Dist2D(Projected.Location, Pawn.GetActorLocation()) < MinimumMoveDistance)
		{
			return true;
		}

		FAIMoveRequest Request(Projected.Location);
		Request.SetAcceptanceRadius(10.0f);
		Request.SetReachTestIncludesAgentRadius(false);
		Request.SetProjectGoalLocation(false);
		Request.SetUsePathfinding(true);
		Request.SetAllowPartialPath(true);
		Request.SetCanStrafe(true);
		const FPathFollowingRequestResult Result = Controller.MoveTo(Request);
		// Every stop (attack start, block) re-arms the hard stop on finish, so clear it with each request.
		if (UPathFollowingComponent* PathFollowing = Controller.GetPathFollowingComponent())
		{
			PathFollowing->SetStopMovementOnFinish(false);
		}
		return Result.Code != EPathFollowingRequestResult::Failed;
	}

	/**
	 * Hands the drivers to the StateTree once they exist. GameFeature actions add them after possession, which can be
	 * after the StateTree starts, so the combat control task retries every tick instead of failing the tree.
	 */
	URpgAIAttackDriverComponent* EnsureCombatControl(AAIController* Controller, APawn* Pawn, FRpgStateTreeCombatControlTaskInstanceData& InstanceData)
	{
		URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Pawn);
		if (!AttackDriver || AttackDriver->IsExternallyControlled())
		{
			return AttackDriver;
		}

		// A reached move goal brakes the character instead of zeroing its velocity, so chained moves do not stutter.
		if (UPathFollowingComponent* PathFollowing = Controller ? Controller->GetPathFollowingComponent() : nullptr)
		{
			PathFollowing->SetStopMovementOnFinish(false);
		}
		AttackDriver->BeginExternalControl(InstanceData.TargetPolicy, InstanceData.TargetSwitchCooldown);
		if (URpgAIBlockDriverComponent* BlockDriver = Pawn->FindComponentByClass<URpgAIBlockDriverComponent>())
		{
			BlockDriver->SetBlockOnDamageEnabled(false);
		}
		InstanceData.LastSeenTargetAttackCount = AttackDriver->GetTargetAttackCount();
		return AttackDriver;
	}
}

FRpgStateTreeCombatControlTaskInstanceData::FRpgStateTreeCombatControlTaskInstanceData()
	: TargetAttackEventTag(RpgGameplayTags::AI_Event_TargetAttackStarted)
{
}

FRpgStateTreeCombatControlTask::FRpgStateTreeCombatControlTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRpgStateTreeCombatControlTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CombatTarget = nullptr;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FRpgStateTreeCombatControlTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	URpgAIAttackDriverComponent* AttackDriver = EnsureCombatControl(Controller, ResolveLivingPawn(Controller), InstanceData);
	if (!AttackDriver)
	{
		InstanceData.CombatTarget = nullptr;
		return EStateTreeRunStatus::Running;
	}

	InstanceData.CombatTarget = AttackDriver->RefreshCombatTarget(InstanceData.SearchRadius, InstanceData.LoseRadius);
	// One owner for facing: every combat state faces the target, so state changes never drop the focus for a frame.
	if (Controller->GetFocusActorForPriority(EAIFocusPriority::Gameplay) != InstanceData.CombatTarget)
	{
		if (InstanceData.CombatTarget)
		{
			Controller->SetFocus(InstanceData.CombatTarget);
		}
		else
		{
			Controller->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	const uint32 TargetAttackCount = AttackDriver->GetTargetAttackCount();
	if (TargetAttackCount != InstanceData.LastSeenTargetAttackCount)
	{
		// One event however many attacks started since the last tick: the StateTree reacts once.
		InstanceData.LastSeenTargetAttackCount = TargetAttackCount;
		if (InstanceData.CombatTarget && InstanceData.TargetAttackEventTag.IsValid())
		{
			Context.SendEvent(InstanceData.TargetAttackEventTag);
		}
	}
	return EStateTreeRunStatus::Running;
}

void FRpgStateTreeCombatControlTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.CombatTarget = nullptr;
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (Controller)
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		if (UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent())
		{
			PathFollowing->SetStopMovementOnFinish(true);
		}
	}
	if (URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Pawn))
	{
		AttackDriver->EndExternalControl();
	}
	if (URpgAIBlockDriverComponent* BlockDriver = Pawn ? Pawn->FindComponentByClass<URpgAIBlockDriverComponent>() : nullptr)
	{
		BlockDriver->ReleaseBlock();
		BlockDriver->SetBlockOnDamageEnabled(true);
	}
}

FRpgStateTreeKeepCombatDistanceTask::FRpgStateTreeKeepCombatDistanceTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRpgStateTreeKeepCombatDistanceTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = ResolveLivingPawn(Controller);
	AActor* Target = ResolveCombatTarget(Pawn);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	SetWalking(*Pawn, InstanceData.bWalk);
	InstanceData.TimeLeft = RandomInRange(InstanceData.DurationMin, InstanceData.DurationMax);
	InstanceData.TimeUntilRepath = 0.0f;
	InstanceData.TimeUntilStrafeSwitch = RandomInRange(InstanceData.StrafeSwitchIntervalMin, InstanceData.StrafeSwitchIntervalMax);
	InstanceData.StrafeSign = FMath::RandBool() ? 1.0f : -1.0f;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FRpgStateTreeKeepCombatDistanceTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = ResolveLivingPawn(Controller);
	AActor* Target = ResolveCombatTarget(Pawn);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float MinDistance = FMath::Min(InstanceData.MinDistance, InstanceData.MaxDistance);
	const float MaxDistance = FMath::Max(InstanceData.MinDistance, InstanceData.MaxDistance);
	const float Distance = PlanarDistance(*Pawn, *Target);
	const bool bInBand = Distance >= MinDistance && Distance <= MaxDistance;
	if (InstanceData.bSucceedInBand && bInBand)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const bool bTimed = InstanceData.DurationMax > 0.0f;
	InstanceData.TimeLeft -= DeltaTime;
	if (bTimed && InstanceData.TimeLeft <= 0.0f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.TimeUntilStrafeSwitch -= DeltaTime;
	if (InstanceData.TimeUntilStrafeSwitch <= 0.0f)
	{
		InstanceData.StrafeSign = FMath::RandBool() ? InstanceData.StrafeSign : -InstanceData.StrafeSign;
		InstanceData.TimeUntilStrafeSwitch = RandomInRange(InstanceData.StrafeSwitchIntervalMin, InstanceData.StrafeSwitchIntervalMax);
	}

	InstanceData.TimeUntilRepath -= DeltaTime;
	if (InstanceData.TimeUntilRepath > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}
	InstanceData.TimeUntilRepath = InstanceData.RepathInterval;

	if (bInBand && !InstanceData.bStrafe)
	{
		return EStateTreeRunStatus::Running;
	}

	// Outside the band head for its middle; inside it keep the radius. A strafing goal always lies a step around the
	// target, also while correcting the distance, so the pawn never reaches it before the next repath.
	const float BandMiddle = 0.5f * (MinDistance + MaxDistance);
	const float Radius = bInBand ? Distance : BandMiddle;
	FVector FromTarget = (Pawn->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	if (FromTarget.IsNearlyZero())
	{
		FromTarget = -Pawn->GetActorForwardVector().GetSafeNormal2D();
	}
	auto GoalAround = [&](const float Sign)
	{
		const FVector Direction = InstanceData.bStrafe
			? FromTarget.RotateAngleAxis(Sign * InstanceData.StrafeAngleStep, FVector::UpVector)
			: FromTarget;
		return Target->GetActorLocation() + Direction * Radius;
	};

	if (!RequestCombatMove(*Controller, *Pawn, GoalAround(InstanceData.StrafeSign)) && InstanceData.bStrafe)
	{
		// No navmesh on this side, for example at a wall: circle the other way.
		InstanceData.StrafeSign = -InstanceData.StrafeSign;
		RequestCombatMove(*Controller, *Pawn, GoalAround(InstanceData.StrafeSign));
	}
	return EStateTreeRunStatus::Running;
}

FRpgStateTreeMeleeAttackTask::FRpgStateTreeMeleeAttackTask()
{
	bShouldCallTick = true;
}

int32 FRpgStateTreeMeleeAttackTask::PickStepCount(const TConstArrayView<float> StepCountWeights, const float RandomValue)
{
	float TotalWeight = 0.0f;
	for (const float Weight : StepCountWeights)
	{
		TotalWeight += FMath::Max(0.0f, Weight);
	}
	if (TotalWeight <= 0.0f)
	{
		return 1;
	}

	float Remaining = FMath::Clamp(RandomValue, 0.0f, 1.0f) * TotalWeight;
	int32 LastPositive = 0;
	for (int32 Index = 0; Index < StepCountWeights.Num(); ++Index)
	{
		const float Weight = FMath::Max(0.0f, StepCountWeights[Index]);
		if (Weight <= 0.0f)
		{
			continue;
		}
		LastPositive = Index;
		if (Remaining < Weight)
		{
			return Index + 1;
		}
		Remaining -= Weight;
	}
	return LastPositive + 1;
}

EStateTreeRunStatus FRpgStateTreeMeleeAttackTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = ResolveLivingPawn(Controller);
	AActor* Target = ResolveCombatTarget(Pawn);
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	SetWalking(*Pawn, !InstanceData.bRunToTarget);
	InstanceData.StepCount = PickStepCount(InstanceData.StepCountWeights, FMath::FRand());
	InstanceData.TimeLeft = InstanceData.ApproachTimeout;
	InstanceData.TimeUntilRepath = 0.0f;
	InstanceData.bAttacking = false;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FRpgStateTreeMeleeAttackTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = ResolveLivingPawn(Controller);
	URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Pawn);
	if (!AttackDriver)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.bAttacking)
	{
		return AttackDriver->IsAttackSequenceActive() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}

	AActor* Target = AttackDriver->GetCombatTarget();
	if (!Target)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float StartRange = AttackDriver->GetAttackRange() * InstanceData.StartRangeFraction;
	if (PlanarDistance(*Pawn, *Target) <= StartRange)
	{
		Controller->StopMovement();
		if (!AttackDriver->StartAttackSequence(InstanceData.StepCount))
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.bAttacking = true;
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeLeft -= DeltaTime;
	if (InstanceData.TimeLeft <= 0.0f)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeUntilRepath -= DeltaTime;
	if (InstanceData.TimeUntilRepath <= 0.0f)
	{
		InstanceData.TimeUntilRepath = 0.25f;
		Controller->MoveToActor(Target, StartRange * 0.5f, true, true, true, nullptr, true);
	}
	return EStateTreeRunStatus::Running;
}

void FRpgStateTreeMeleeAttackTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	AAIController* Controller = ResolveController(Context);
	if (!Controller)
	{
		return;
	}
	// A leaving state stops further steps; the running swing plays out. The next state takes over the movement.
	if (URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Controller->GetPawn()))
	{
		AttackDriver->CancelAttackSequence();
	}
}

FRpgStateTreeHoldBlockTask::FRpgStateTreeHoldBlockTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRpgStateTreeHoldBlockTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	using namespace RpgStateTreeCombat;
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = ResolveController(Context);
	APawn* Pawn = ResolveLivingPawn(Controller);
	URpgAIBlockDriverComponent* BlockDriver = Pawn ? Pawn->FindComponentByClass<URpgAIBlockDriverComponent>() : nullptr;
	if (!BlockDriver)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The block covers a frontal angle; the combat control task keeps the focus on the attacker.
	Controller->StopMovement();
	return BlockDriver->HoldBlock(RandomInRange(InstanceData.DurationMin, InstanceData.DurationMax))
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FRpgStateTreeHoldBlockTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	using namespace RpgStateTreeCombat;
	const AAIController* Controller = ResolveController(Context);
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const URpgAIBlockDriverComponent* BlockDriver = Pawn ? Pawn->FindComponentByClass<URpgAIBlockDriverComponent>() : nullptr;
	return BlockDriver && BlockDriver->IsBlockHeld() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FRpgStateTreeHoldBlockTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const AAIController* Controller = RpgStateTreeCombat::ResolveController(Context);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (URpgAIBlockDriverComponent* BlockDriver = Pawn ? Pawn->FindComponentByClass<URpgAIBlockDriverComponent>() : nullptr)
	{
		BlockDriver->ReleaseBlock();
	}
}

FRpgStateTreeWaitWhileOwnerTaggedTaskInstanceData::FRpgStateTreeWaitWhileOwnerTaggedTaskInstanceData()
{
	Tags.AddTag(RpgGameplayTags::State_Staggered);
	Tags.AddTag(RpgGameplayTags::State_GuardBroken);
}

FRpgStateTreeWaitWhileOwnerTaggedTask::FRpgStateTreeWaitWhileOwnerTaggedTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRpgStateTreeWaitWhileOwnerTaggedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = RpgStateTreeCombat::ResolveController(Context))
	{
		Controller->StopMovement();
	}
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FRpgStateTreeWaitWhileOwnerTaggedTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIController* Controller = RpgStateTreeCombat::ResolveController(Context);
	const UAbilitySystemComponent* AbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Controller ? Controller->GetPawn() : nullptr);
	return AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(InstanceData.Tags)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Succeeded;
}
