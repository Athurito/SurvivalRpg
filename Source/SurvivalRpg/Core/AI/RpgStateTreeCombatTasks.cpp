#include "RpgStateTreeCombatTasks.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
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

	/**
	 * Hands the drivers to the StateTree once they exist. GameFeature actions add them after possession, which can be
	 * after the StateTree starts, so the combat control task retries every tick instead of failing the tree.
	 */
	URpgAIAttackDriverComponent* EnsureCombatControl(APawn* Pawn, FRpgStateTreeCombatControlTaskInstanceData& InstanceData)
	{
		URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Pawn);
		if (!AttackDriver || AttackDriver->IsExternallyControlled())
		{
			return AttackDriver;
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
	APawn* Pawn = ResolveLivingPawn(ResolveController(Context));
	if (URpgAIAttackDriverComponent* AttackDriver = EnsureCombatControl(Pawn, InstanceData))
	{
		InstanceData.CombatTarget = AttackDriver->RefreshCombatTarget(InstanceData.SearchRadius, InstanceData.LoseRadius);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FRpgStateTreeCombatControlTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	using namespace RpgStateTreeCombat;
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	URpgAIAttackDriverComponent* AttackDriver = EnsureCombatControl(ResolveLivingPawn(ResolveController(Context)), InstanceData);
	if (!AttackDriver)
	{
		InstanceData.CombatTarget = nullptr;
		return EStateTreeRunStatus::Running;
	}

	InstanceData.CombatTarget = AttackDriver->RefreshCombatTarget(InstanceData.SearchRadius, InstanceData.LoseRadius);
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
	const AAIController* Controller = ResolveController(Context);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
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

	Controller->SetFocus(Target);
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
		Controller->StopMovement();
		return EStateTreeRunStatus::Running;
	}

	// Back off or close in to a quarter of the band inside its edge, or keep the radius and step around the target.
	const float BandInset = 0.25f * (MaxDistance - MinDistance);
	const float Radius = Distance < MinDistance ? MinDistance + BandInset
		: Distance > MaxDistance ? MaxDistance - BandInset
		: Distance;
	FVector FromTarget = (Pawn->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	if (FromTarget.IsNearlyZero())
	{
		FromTarget = -Pawn->GetActorForwardVector().GetSafeNormal2D();
	}
	if (bInBand && InstanceData.bStrafe)
	{
		FromTarget = FromTarget.RotateAngleAxis(InstanceData.StrafeSign * InstanceData.StrafeAngleStep, FVector::UpVector);
	}

	const FVector Destination = Target->GetActorLocation() + FromTarget * Radius;
	const EPathFollowingRequestResult::Type Result =
		Controller->MoveToLocation(Destination, 30.0f, false, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed && bInBand && InstanceData.bStrafe)
	{
		// Blocked on this side, for example by a wall: circle the other way next time.
		InstanceData.StrafeSign = -InstanceData.StrafeSign;
	}
	return EStateTreeRunStatus::Running;
}

void FRpgStateTreeKeepCombatDistanceTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = RpgStateTreeCombat::ResolveController(Context))
	{
		Controller->StopMovement();
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
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

	Controller->SetFocus(Target);
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
	// A leaving state stops further steps; the running swing plays out.
	if (URpgAIAttackDriverComponent* AttackDriver = ResolveAttackDriver(Controller->GetPawn()))
	{
		AttackDriver->CancelAttackSequence();
	}
	Controller->StopMovement();
	Controller->ClearFocus(EAIFocusPriority::Gameplay);
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

	// The block covers a frontal angle, so face the attacker first.
	if (AActor* Target = ResolveCombatTarget(Pawn))
	{
		Controller->SetFocus(Target);
	}
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
	AAIController* Controller = RpgStateTreeCombat::ResolveController(Context);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (URpgAIBlockDriverComponent* BlockDriver = Pawn ? Pawn->FindComponentByClass<URpgAIBlockDriverComponent>() : nullptr)
	{
		BlockDriver->ReleaseBlock();
	}
	if (Controller)
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
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
