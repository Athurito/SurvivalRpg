#include "RpgAbilityTask_WaitMoverGetUp.h"

#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Core/Character/RpgMoverRagdollComponent.h"

URpgAbilityTask_WaitMoverGetUp* URpgAbilityTask_WaitMoverGetUp::WaitRagdollGetUpSelection(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<URpgAbilityTask_WaitMoverGetUp>(OwningAbility);
}

void URpgAbilityTask_WaitMoverGetUp::Activate()
{
	AActor* Avatar = GetAvatarActor();
	RagdollComponent = Avatar ? Avatar->FindComponentByClass<URpgMoverRagdollComponent>() : nullptr;
	if (!RagdollComponent.IsValid())
	{
		if (ShouldBroadcastAbilityTaskDelegates()) { OnRejected.Broadcast(); }
		EndTask();
		return;
	}
	StateChangedHandle = RagdollComponent->OnStateChanged.AddUObject(this, &ThisClass::CheckSelection);
	CheckSelection();
}

void URpgAbilityTask_WaitMoverGetUp::CheckSelection()
{
	URpgMoverRagdollComponent* Component = RagdollComponent.Get();
	if (!Component || !Ability) { return; }
	const FRpgMoverRagdollState State = Component->GetRagdollState();
	if (!State.MatchesAbility(Ability)) { return; }
	if (State.Phase == ERpgMoverRagdollPhase::Inactive)
	{
		if (ShouldBroadcastAbilityTaskDelegates()) { OnRejected.Broadcast(); }
		EndTask();
	}
	else if (Component->CanConsumeSelection(Ability))
	{
		// Retire the listener before Blueprint starts a montage or synchronously ends/cancels the ability.
		Component->OnStateChanged.Remove(StateChangedHandle);
		StateChangedHandle.Reset();
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnSelected.Broadcast(State.GetUpMontage, State.GetUpStartTime, State.GetUpPlayRate);
		}
		EndTask();
	}
}

void URpgAbilityTask_WaitMoverGetUp::OnDestroy(bool bInOwnerFinished)
{
	if (URpgMoverRagdollComponent* Component = RagdollComponent.Get()) { Component->OnStateChanged.Remove(StateChangedHandle); }
	StateChangedHandle.Reset();
	Super::OnDestroy(bInOwnerFinished);
}
