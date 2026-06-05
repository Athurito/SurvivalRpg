#include "RpgStateTreeFindHostileTargetTask.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "SurvivalRpg/Combat/RpgAIAttackDriverComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Factions/RpgFactionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStateTreeFindHostileTargetTask)

#define LOCTEXT_NAMESPACE "RpgAI"

FRpgStateTreeFindHostileTargetTask::FRpgStateTreeFindHostileTargetTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FRpgStateTreeFindHostileTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.TimeUntilNextMoveRequest = 0.0f;
	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FRpgStateTreeFindHostileTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AAIController* Controller = ResolveAIController(Context);
	APawn* OwnerPawn = Controller ? Controller->GetPawn() : nullptr;
	if (!OwnerPawn || !OwnerPawn->HasAuthority() || !IsAlive(OwnerPawn))
	{
		if (Controller)
		{
			Controller->StopMovement();
		}
		if (OwnerPawn && InstanceData.bSetAttackDriverTarget)
		{
			SetAttackDriverTarget(OwnerPawn, nullptr);
		}
		InstanceData.TargetActor = nullptr;
		return EStateTreeRunStatus::Failed;
	}

	if (!IsValidHostileTarget(OwnerPawn, InstanceData.TargetActor))
	{
		InstanceData.TargetActor = FindNearestHostileTarget(OwnerPawn, InstanceData.SearchRadius);
		if (InstanceData.bSetAttackDriverTarget)
		{
			SetAttackDriverTarget(OwnerPawn, InstanceData.TargetActor);
		}
	}

	AActor* Target = InstanceData.TargetActor;
	if (!Target)
	{
		if (InstanceData.bSetAttackDriverTarget)
		{
			SetAttackDriverTarget(OwnerPawn, nullptr);
		}
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bSetAttackDriverTarget)
	{
		SetAttackDriverTarget(OwnerPawn, Target);
	}

	if (!InstanceData.bMoveTowardTarget)
	{
		return EStateTreeRunStatus::Running;
	}

	if (IsTargetInRange(OwnerPawn, Target, InstanceData.AttackRange))
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Running;
	}

	InstanceData.TimeUntilNextMoveRequest -= DeltaTime;
	if (InstanceData.TimeUntilNextMoveRequest <= 0.0f)
	{
		RequestMove(*Controller, *Target, InstanceData.MoveAcceptableRadius);
		InstanceData.TimeUntilNextMoveRequest = FMath::Max(0.05f, InstanceData.RepathInterval);
	}

	return EStateTreeRunStatus::Running;
}

void FRpgStateTreeFindHostileTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIController* Controller = ResolveAIController(Context))
	{
		Controller->StopMovement();
		if (APawn* OwnerPawn = Controller->GetPawn())
		{
			SetAttackDriverTarget(OwnerPawn, nullptr);
		}
	}

	InstanceData.TargetActor = nullptr;
	InstanceData.TimeUntilNextMoveRequest = 0.0f;
}

AAIController* FRpgStateTreeFindHostileTargetTask::ResolveAIController(const FStateTreeExecutionContext& Context)
{
	return Cast<AAIController>(Context.GetOwner());
}

bool FRpgStateTreeFindHostileTargetTask::IsAlive(const AActor* Actor)
{
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(Actor);
	return !HealthComponent || !HealthComponent->IsDeadOrDying();
}

bool FRpgStateTreeFindHostileTargetTask::IsValidHostileTarget(const AActor* Owner, const AActor* Candidate)
{
	if (!Owner || !Candidate || Owner == Candidate || !IsAlive(Candidate))
	{
		return false;
	}

	const UWorld* World = Owner->GetWorld();
	const URpgFactionSubsystem* FactionSubsystem = World ? World->GetSubsystem<URpgFactionSubsystem>() : nullptr;
	return FactionSubsystem && FactionSubsystem->IsHostile(Owner, Candidate);
}

AActor* FRpgStateTreeFindHostileTargetTask::FindNearestHostileTarget(const AActor* Owner, float SearchRadius)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!Owner || !World)
	{
		return nullptr;
	}

	const float Radius = FMath::Max(0.0f, SearchRadius);
	const float RadiusSq = FMath::Square(Radius);
	const FVector OwnerLocation = Owner->GetActorLocation();

	AActor* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || !IsValidHostileTarget(Owner, Candidate))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(OwnerLocation, Candidate->GetActorLocation());
		if (DistanceSq > RadiusSq || DistanceSq >= BestDistanceSq)
		{
			continue;
		}

		BestTarget = Candidate;
		BestDistanceSq = DistanceSq;
	}

	return BestTarget;
}

void FRpgStateTreeFindHostileTargetTask::SetAttackDriverTarget(AActor* Owner, AActor* Target)
{
	URpgAIAttackDriverComponent* AttackDriver = Owner ? Owner->FindComponentByClass<URpgAIAttackDriverComponent>() : nullptr;
	if (!AttackDriver)
	{
		return;
	}

	if (Target)
	{
		AttackDriver->SetCombatTarget(Target);
	}
	else
	{
		AttackDriver->ClearCombatTarget();
	}
}

bool FRpgStateTreeFindHostileTargetTask::IsTargetInRange(const AActor* Owner, const AActor* Target, float Range)
{
	if (!Owner || !Target)
	{
		return false;
	}

	const float RangeSq = FMath::Square(FMath::Max(0.0f, Range));
	return FVector::DistSquared2D(Owner->GetActorLocation(), Target->GetActorLocation()) <= RangeSq;
}

void FRpgStateTreeFindHostileTargetTask::RequestMove(AAIController& Controller, AActor& Target, float AcceptableRadius)
{
	Controller.MoveToActor(&Target, FMath::Max(0.0f, AcceptableRadius), true, true, true, nullptr, true);
}

#if WITH_EDITOR
FText FRpgStateTreeFindHostileTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 0;
	const FText Radius = FText::AsNumber(InstanceData->SearchRadius, &Options);
	return FText::Format(LOCTEXT("FindHostileTargetDescription", "Find Hostile Target within {0}"), Radius);
}
#endif

#undef LOCTEXT_NAMESPACE
