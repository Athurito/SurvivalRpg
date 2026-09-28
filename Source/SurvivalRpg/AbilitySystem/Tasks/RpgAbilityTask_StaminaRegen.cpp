#include "RpgAbilityTask_StaminaRegen.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgStaminaRegenTask
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

	bool IsRegenerationTuningEffect(const UGameplayEffect* Effect, float Level)
	{
		if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| Effect->Modifiers.Num() != 1 || !Effect->Executions.IsEmpty()
			|| Effect->GetStackingType() != EGameplayEffectStackingType::None
			|| Effect->Period.GetValueAtLevel(Level) != 0.f)
		{
			return false;
		}
		const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
		return Modifier.Attribute == URpgStaminaSet::GetStaminaRegenAttribute()
			&& Modifier.ModifierOp == EGameplayModOp::Additive;
	}
}

URpgAbilityTask_StaminaRegen* URpgAbilityTask_StaminaRegen::RegenerateStamina(UGameplayAbility* OwningAbility,
	TSubclassOf<UGameplayEffect> StaminaChangeEffect, FGameplayTag StaminaDeltaTag, float DelaySeconds,
	TSubclassOf<UGameplayEffect> RegenerationTuningEffect)
{
	URpgAbilityTask_StaminaRegen* Task = NewAbilityTask<URpgAbilityTask_StaminaRegen>(OwningAbility);
	Task->ChangeEffect = StaminaChangeEffect;
	Task->TuningEffect = RegenerationTuningEffect;
	Task->DeltaTag = StaminaDeltaTag;
	Task->RecoveryDelay = DelaySeconds;
	return Task;
}

void URpgAbilityTask_StaminaRegen::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(AbilitySystemComponent.Get());
	AActor* Avatar = GetAvatarActor();
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ASC || !Avatar || !Avatar->HasAuthority() || !Avatar->GetWorld()
		|| !FMath::IsFinite(RecoveryDelay) || RecoveryDelay < 0.f
		|| !RpgStaminaRegenTask::IsStaminaEffect(ChangeEffect.GetDefaultObject(), DeltaTag)
		|| (TuningEffect && !RpgStaminaRegenTask::IsRegenerationTuningEffect(TuningEffect.GetDefaultObject(), Ability->GetAbilityLevel())))
	{
		StopRegeneration();
		return;
	}
	CapturedASC = ASC;
	CapturedAvatar = Avatar;
	CapturedStamina = ASC->GetSet<URpgStaminaSet>();
	CapturedWorld = Avatar->GetWorld();
	CapturedSpec = Ability->GetCurrentAbilitySpecHandle();
	CapturedActivationKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	if (!MatchesActivation()) { StopRegeneration(); return; }
	if (!CapturedStamina.IsValid())
	{
		// OnSpawn can run inside a synchronous GameFeature grant before its attributes are added.
		// Allow that batch to finish once; never adopt a later replacement of an already captured set.
		RegenTimer = CapturedWorld->GetTimerManager().SetTimerForNextTick(this, &ThisClass::InitializeRegeneration);
		return;
	}
	InitializeRegeneration();
}

void URpgAbilityTask_StaminaRegen::InitializeRegeneration()
{
	if (bStopped || !MatchesActivation()) { StopRegeneration(); return; }
	if (!CapturedStamina.IsValid()) { CapturedStamina = CapturedASC->GetSet<URpgStaminaSet>(); }
	if (!HasLiveBinding()) { StopRegeneration(); return; }
	if (TuningEffect)
	{
		URpgAbilitySystemComponent* ASC = CapturedASC.Get();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(TuningEffect, Ability->GetAbilityLevel(), ASC->MakeEffectContext());
		if (!Spec.IsValid()) { StopRegeneration(); return; }
		const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		// Attribute/effect callbacks can synchronously end this ability or replace its avatar/set.
		// The returned handle is not available to OnDestroy during that callback, so retire it here.
		if (!HasLiveBinding())
		{
			if (AppliedHandle.IsValid()) { ASC->RemoveActiveGameplayEffect(AppliedHandle); }
			StopRegeneration();
			return;
		}
		TuningEffectHandle = AppliedHandle;
		if (!AppliedHandle.IsValid() || !AppliedHandle.WasSuccessfullyApplied()
			|| !ASC->GetActiveGameplayEffect(AppliedHandle)
			|| !FMath::IsFinite(CapturedStamina->GetStaminaRegen()) || CapturedStamina->GetStaminaRegen() < 0.f)
		{
			StopRegeneration();
			return;
		}
	}
	LastUpdateTime = CapturedWorld->GetTimeSeconds();
	RegenAllowedAt = LastUpdateTime + RecoveryDelay;
	StaminaChangedHandle = CapturedASC->GetGameplayAttributeValueChangeDelegate(URpgStaminaSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	CapturedWorld->GetTimerManager().SetTimer(RegenTimer, this, &ThisClass::Regenerate, 0.1f, true);
}

bool URpgAbilityTask_StaminaRegen::MatchesActivation() const
{
	const URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const AActor* Avatar = CapturedAvatar.Get();
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ASC || !Avatar || !Avatar->HasAuthority() || !CapturedWorld.IsValid()
		|| !Ability || !Ability->IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority() || !CapturedSpec.IsValid()
		|| ActorInfo->AbilitySystemComponent.Get() != ASC || ActorInfo->AvatarActor.Get() != Avatar
		|| ASC->GetAvatarActor() != Avatar || Avatar->GetWorld() != CapturedWorld.Get()
		|| Ability->GetCurrentAbilitySpecHandle() != CapturedSpec
		|| Ability->GetCurrentActivationInfo().GetActivationPredictionKey() != CapturedActivationKey)
	{
		return false;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(CapturedSpec);
	return Spec && Spec->IsActive();
}

bool URpgAbilityTask_StaminaRegen::HasLiveBinding() const
{
	return !bStopped && MatchesActivation() && CapturedStamina.IsValid()
		&& CapturedASC->GetSet<URpgStaminaSet>() == CapturedStamina.Get();
}

void URpgAbilityTask_StaminaRegen::HandleStaminaChanged(const FOnAttributeChangeData& Change)
{
	if (!HasLiveBinding()) { StopRegeneration(); return; }
	if (!FMath::IsFinite(Change.NewValue) || !FMath::IsFinite(Change.OldValue)) { StopRegeneration(); return; }
	if (Change.NewValue < Change.OldValue)
	{
		// This includes damage/block costs, other abilities and reductions of the maximum; no source whitelist.
		RegenAllowedAt = CapturedWorld->GetTimeSeconds() + RecoveryDelay;
	}
}

void URpgAbilityTask_StaminaRegen::Regenerate()
{
	if (!HasLiveBinding()) { StopRegeneration(); return; }
	const double Now = CapturedWorld->GetTimeSeconds();
	const double EligibleSeconds = FMath::Max(0., Now - FMath::Max(LastUpdateTime, RegenAllowedAt));
	LastUpdateTime = Now;
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(CapturedAvatar.Get());
	if (ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead) || (Health && Health->IsDeadOrDying())) { return; }
	const URpgStaminaSet* StaminaSet = CapturedStamina.Get();
	const float Stamina = StaminaSet->GetStamina();
	const float Maximum = StaminaSet->GetMaxStamina();
	const float Rate = StaminaSet->GetStaminaRegen();
	if (!FMath::IsFinite(Stamina) || !FMath::IsFinite(Maximum) || !FMath::IsFinite(Rate)
		|| !FMath::IsFinite(EligibleSeconds) || Rate < 0.f)
	{
		StopRegeneration();
		return;
	}
	if (EligibleSeconds <= 0. || Rate <= 0.f || Stamina >= Maximum) { return; }
	const double Amount = FMath::Min(static_cast<double>(Maximum - Stamina), Rate * EligibleSeconds);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ChangeEffect, Ability->GetAbilityLevel(), ASC->MakeEffectContext());
	if (!Spec.IsValid()) { StopRegeneration(); return; }
	Spec.Data->SetSetByCallerMagnitude(DeltaTag, static_cast<float>(Amount));
	const bool bApplied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	if (!bStopped && !bApplied) { StopRegeneration(); }
}

void URpgAbilityTask_StaminaRegen::StopRegeneration()
{
	if (bStopped) { return; }
	const bool bNotify = !CapturedSpec.IsValid() || MatchesActivation();
	const TWeakObjectPtr<URpgAbilitySystemComponent> NotifyASC = CapturedASC;
	const TWeakObjectPtr<AActor> NotifyAvatar = CapturedAvatar;
	bStopped = true;
	ReleaseResources();
	// Removing our persistent modifier can reenter GAS. Never broadcast an old task into a new activation.
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	const bool bSameBinding = !CapturedSpec.IsValid() || (NotifyASC.IsValid() && NotifyAvatar.IsValid() && ActorInfo
		&& ActorInfo->AbilitySystemComponent.Get() == NotifyASC.Get() && ActorInfo->AvatarActor.Get() == NotifyAvatar.Get()
		&& NotifyASC->GetAvatarActor() == NotifyAvatar.Get() && Ability->GetCurrentAbilitySpecHandle() == CapturedSpec
		&& Ability->GetCurrentActivationInfo().GetActivationPredictionKey() == CapturedActivationKey);
	if (bNotify && bSameBinding && !IsFinished() && ShouldBroadcastAbilityTaskDelegates()) { OnStopped.Broadcast(); }
	EndTask();
}

void URpgAbilityTask_StaminaRegen::ReleaseResources()
{
	if (UWorld* World = CapturedWorld.Get()) { World->GetTimerManager().ClearTimer(RegenTimer); }
	URpgAbilitySystemComponent* ASC = CapturedASC.Get();
	const FActiveGameplayEffectHandle HandleToRemove = TuningEffectHandle;
	TuningEffectHandle = FActiveGameplayEffectHandle{};
	if (ASC)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(URpgStaminaSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
	}
	StaminaChangedHandle.Reset();
	CapturedStamina.Reset();
	CapturedAvatar.Reset();
	CapturedASC.Reset();
	CapturedWorld.Reset();
	// Clear ownership before removal can broadcast callbacks. Never remove by class or restore a numeric snapshot.
	if (ASC && HandleToRemove.IsValid()) { ASC->RemoveActiveGameplayEffect(HandleToRemove); }
}

void URpgAbilityTask_StaminaRegen::OnDestroy(bool bInOwnerFinished)
{
	bStopped = true;
	ReleaseResources();
	Super::OnDestroy(bInOwnerFinished);
}
