#include "RpgStateTreeCombatConditions.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "SurvivalRpg/Combat/RpgAIAttackDriverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgStateTreeCombatConditions)

namespace
{
	APawn* ResolveConditionPawn(const FStateTreeExecutionContext& Context)
	{
		const AAIController* Controller = Cast<AAIController>(Context.GetOwner());
		return Controller ? Controller->GetPawn() : nullptr;
	}
}

bool FRpgStateTreeCombatTargetDistanceCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const APawn* Pawn = ResolveConditionPawn(Context);
	const URpgAIAttackDriverComponent* Driver = Pawn ? Pawn->FindComponentByClass<URpgAIAttackDriverComponent>() : nullptr;
	const AActor* Target = Driver ? Driver->GetCombatTarget() : nullptr;

	bool bPasses = false;
	if (Target)
	{
		const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
		bPasses = Distance >= InstanceData.MinDistance &&
			(InstanceData.MaxDistance < 0.0f || Distance <= InstanceData.MaxDistance);
	}
	return bPasses != bInvert;
}

bool FRpgStateTreeOwnerHasTagsCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const UAbilitySystemComponent* AbilitySystem =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ResolveConditionPawn(Context));
	const bool bPasses = AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(InstanceData.Tags);
	return bPasses != bInvert;
}
