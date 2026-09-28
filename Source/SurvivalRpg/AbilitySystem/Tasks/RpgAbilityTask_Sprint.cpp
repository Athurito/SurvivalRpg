#include "RpgAbilityTask_Sprint.h"

#include "Abilities/GameplayAbility.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgSprintTask
{
	bool IsStaminaEffect(const UGameplayEffect* Effect, FGameplayTag DeltaTag)
	{
		if (!Effect || !DeltaTag.IsValid() || Effect->DurationPolicy != EGameplayEffectDurationType::Instant
			|| Effect->Modifiers.Num() != 1 || !Effect->Executions.IsEmpty())
		{
			return false;
		}
		const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
		return Modifier.Attribute == URpgStaminaSet::GetStaminaAttribute()
			&& Modifier.ModifierOp == EGameplayModOp::Additive
			&& Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
			&& Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag == DeltaTag;
	}
}

URpgAbilityTask_Sprint* URpgAbilityTask_Sprint::StartSprint(UGameplayAbility* OwningAbility,
	TSubclassOf<UGameplayEffect> StaminaChangeEffect, FGameplayTag StaminaDeltaTag,
	float StaminaPerSecond, float MinimumStaminaToStart)
{
	URpgAbilityTask_Sprint* Task = NewAbilityTask<URpgAbilityTask_Sprint>(OwningAbility);
	Task->ChangeEffect = StaminaChangeEffect;
	Task->DeltaTag = StaminaDeltaTag;
	Task->DrainRate = StaminaPerSecond;
	Task->StartThreshold = MinimumStaminaToStart;
	return Task;
}

void URpgAbilityTask_Sprint::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(AbilitySystemComponent.Get());
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	URpgCharacterMovementComponent* Movement = Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!ActorInfo || !ASC || !Character || !Movement || !Movement->bEnableGASSprint
		|| (!ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
		|| !FMath::IsFinite(DrainRate) || DrainRate < 0.f || !FMath::IsFinite(StartThreshold) || StartThreshold < 0.f
		|| !FMath::IsFinite(Movement->SprintSpeed) || Movement->SprintSpeed <= 0.f
		|| !RpgSprintTask::IsStaminaEffect(ChangeEffect.GetDefaultObject(), DeltaTag))
	{
		StopSprint(false);
		return;
	}
	CapturedASC = ASC;
	CapturedCharacter = Character;
	CapturedMovement = Movement;
	CapturedStamina = ASC->GetSet<URpgStaminaSet>();
	CapturedSpec = Ability->GetCurrentAbilitySpecHandle();
	CapturedActivationKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	bAuthority = ActorInfo->IsNetAuthority();
	if (!HasLiveBinding()) { StopSprint(false); return; }
	const float Stamina = CapturedStamina->GetStamina();
	if (!FMath::IsFinite(Stamina)) { StopSprint(false); return; }
	if (Stamina <= 0.f || Stamina < StartThreshold) { StopSprint(true); return; }

	MovementLease = ASC->BeginSprintMovement(Ability, Movement->SprintSpeed);
	if (!MovementLease) { StopSprint(false); return; }
	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(URpgStaminaSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	Character->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleMovementUpdated);
}

bool URpgAbilityTask_Sprint::MatchesActivation() const
{
	const URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const ACharacter* Character = CapturedCharacter.Get();
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ASC || !Character
		|| !Ability || !Ability->IsActive() || !ActorInfo || !CapturedSpec.IsValid()
		|| ActorInfo->AbilitySystemComponent.Get() != ASC || ActorInfo->AvatarActor.Get() != Character
		|| ASC->GetAvatarActor() != Character
		|| Ability->GetCurrentAbilitySpecHandle() != CapturedSpec
		|| Ability->GetCurrentActivationInfo().GetActivationPredictionKey() != CapturedActivationKey)
	{
		return false;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(CapturedSpec);
	return Spec && Spec->IsActive();
}

bool URpgAbilityTask_Sprint::HasLiveBinding() const
{
	if (bStopped || !MatchesActivation() || !CapturedMovement.IsValid() || !CapturedStamina.IsValid()
		|| CapturedASC->GetSet<URpgStaminaSet>() != CapturedStamina.Get()
		|| CapturedCharacter->GetCharacterMovement() != CapturedMovement.Get()
		|| CapturedASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead))
	{
		return false;
	}
	const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(CapturedCharacter.Get());
	return !Health || !Health->IsDeadOrDying();
}

void URpgAbilityTask_Sprint::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	(void)OldVelocity;
	if (!HasLiveBinding()) { StopSprint(false); return; }
	ACharacter* Character = CapturedCharacter.Get();
	URpgCharacterMovementComponent* Movement = CapturedMovement.Get();
	// OnCharacterMovementUpdated runs inside the effective CMC simulation scope. Owner replay
	// can revisit the same historical sprint, but only authoritative movement may spend stamina.
	if (!bAuthority || !Character->HasAuthority() || Character->bClientUpdating || !Movement->IsSprinting()
		|| !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || DrainRate <= 0.f
		|| FVector::DistSquared2D(Character->GetActorLocation(), OldLocation) <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float Stamina = CapturedStamina->GetStamina();
	const float RequestedCost = DrainRate * DeltaSeconds;
	if (!FMath::IsFinite(Stamina) || !FMath::IsFinite(RequestedCost)) { StopSprint(false); return; }
	if (Stamina <= 0.f) { StopSprint(true); return; }
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ChangeEffect, Ability->GetAbilityLevel(), ASC->MakeEffectContext());
	if (!Spec.IsValid()) { StopSprint(false); return; }
	Spec.Data->SetSetByCallerMagnitude(DeltaTag, -FMath::Min(Stamina, RequestedCost));
	// Instant effects return a non-active sentinel handle; IsValid() would incorrectly reject success.
	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	// Application may synchronously exhaust stamina and end the owning Blueprint ability.
	if (!bStopped && !bApplied) { StopSprint(false); }
}

void URpgAbilityTask_Sprint::HandleStaminaChanged(const FOnAttributeChangeData& Change)
{
	if (!HasLiveBinding()) { StopSprint(false); return; }
	if (!FMath::IsFinite(Change.NewValue)) { StopSprint(false); }
	else if (Change.NewValue <= 0.f) { StopSprint(true); }
}

void URpgAbilityTask_Sprint::StopSprint(bool bExhausted)
{
	if (bStopped) { return; }
	// Capture the validity before cleanup; never consume input belonging to a replacement activation/avatar.
	const bool bSuppressInput = bExhausted && HasLiveBinding();
	const bool bNotify = !CapturedSpec.IsValid() || MatchesActivation();
	bStopped = true;
	if (bSuppressInput)
	{
		CapturedASC->SuppressAbilityInputUntilRelease(CapturedSpec);
	}
	ReleaseResources();
	if (bNotify && ShouldBroadcastAbilityTaskDelegates()) { OnStopped.Broadcast(); }
	EndTask();
}

void URpgAbilityTask_Sprint::ReleaseResources()
{
	if (ACharacter* Character = CapturedCharacter.Get())
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	const uint32 LeaseToRelease = MovementLease;
	MovementLease = 0;
	if (URpgAbilitySystemComponent* ASC = CapturedASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(URpgStaminaSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		ASC->EndSprintMovement(Ability, LeaseToRelease);
	}
	StaminaChangedHandle.Reset();
	CapturedCharacter.Reset();
	CapturedMovement.Reset();
	CapturedStamina.Reset();
	CapturedASC.Reset();
}

void URpgAbilityTask_Sprint::OnDestroy(bool bInOwnerFinished)
{
	// A server end can arrive before exhausted Stamina replicates. Do not let the same held
	// input predict a replacement sprint from that older resource value. EndAbility already
	// cleared Ability::bIsActive before TaskOwnerEnded, so match identity without an active gate.
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(CapturedSpec) : nullptr;
	if (bInOwnerFinished && !bStopped && ASC && CapturedCharacter.IsValid() && ActorInfo && Spec && Spec->InputPressed
		&& ActorInfo->IsLocallyControlled() && ActorInfo->AbilitySystemComponent.Get() == ASC
		&& ActorInfo->AvatarActor.Get() == CapturedCharacter.Get() && ASC->GetAvatarActor() == CapturedCharacter.Get()
		&& Ability->GetCurrentAbilitySpecHandle() == CapturedSpec
		&& Ability->GetCurrentActivationInfo().GetActivationPredictionKey() == CapturedActivationKey)
	{
		ASC->SuppressAbilityInputUntilRelease(CapturedSpec);
	}
	bStopped = true;
	ReleaseResources();
	Super::OnDestroy(bInOwnerFinished);
}
