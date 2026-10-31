#include "RpgGameplayAbility_BasicWeaponAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/AnimNotifyState_RpgComboWindow.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Combat/RpgCombatDeveloperSettings.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "TimerManager.h"

namespace
{
	bool ShouldLogWeaponAttackLifecycle()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings && Settings->bLogWeaponAttackLifecycle;
	}
}

URpgGameplayAbility_BasicWeaponAttack::URpgGameplayAbility_BasicWeaponAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;
}

#if WITH_DEV_AUTOMATION_TESTS
bool URpgGameplayAbility_BasicWeaponAttack::HasPendingAttackTimersForTests() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FTimerManager& TimerManager = World->GetTimerManager();
	return SwingTrace.HasPendingTimers() ||
		TimerManager.TimerExists(ComboWindowOpenTimerHandle) ||
		TimerManager.TimerExists(ComboWindowCloseTimerHandle) ||
		TimerManager.TimerExists(RecoveryCancelTimerHandle);
}

bool URpgGameplayAbility_BasicWeaponAttack::HasResidualAttackRuntimeStateForTests() const
{
	return HasPendingAttackTimersForTests() ||
		ActiveWeaponInstance != nullptr ||
		ActiveAttackDefinition.Montage != nullptr ||
		!AppliedStepOwnerTags.IsEmpty() ||
		bWaitingForMontage ||
		bFinishingAttack ||
		SwingTrace.HasResidualState() ||
		!HitActorsThisWindow.IsEmpty() ||
		bRecordingSwing ||
		!SwingRecord.Traces.IsEmpty() ||
		ActiveMontageTask != nullptr ||
		ActiveAttackTag.IsValid() ||
		ComboStepIndex != 0 ||
		bComboWindowOpen ||
		bComboInputBuffered ||
		bComboRequestPending ||
		bRecoveryCancelable ||
		ComboInputPressedHandle.IsValid() ||
		ComboRequestHandle.IsValid();
}
#endif

void URpgGameplayAbility_BasicWeaponAttack::LogAbilitySystemActivationFailure(
	const FGameplayAbilitySpecHandle Handle,
	const AActor* AvatarActor,
	const FGameplayTagContainer& FailedReason,
	const FString& PredictionKey) const
{
	if (!ShouldLogWeaponAttackLifecycle())
	{
		return;
	}

	const UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	UE_LOG(
		LogRpgWeapons,
		Log,
		TEXT("WeaponAttackLifecycle Stage=ActivationRejected.Server Ability=%s Spec=%s Avatar=%s NetMode=%d LocalRole=%d RemoteRole=%d WorldTime=%.3f Montage=None Section=None Position=-1.000 PredictionKey=%s Detail=FailureTags=%s"),
		*GetNameSafe(this),
		*Handle.ToString(),
		*GetNameSafe(AvatarActor),
		World ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
		AvatarActor ? static_cast<int32>(AvatarActor->GetLocalRole()) : INDEX_NONE,
		AvatarActor ? static_cast<int32>(AvatarActor->GetRemoteRole()) : INDEX_NONE,
		World ? World->GetTimeSeconds() : -1.0,
		*PredictionKey,
		*FailedReason.ToStringSimple());
}

bool URpgGameplayAbility_BasicWeaponAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const URpgWeaponInstance* WeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo));
	if (!WeaponInstance)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RpgGameplayTags::Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	const FGameplayTag InputTag = GetInputTagFromSpec(Handle, ActorInfo);
	if (!IsEquipmentActiveForInput(WeaponInstance, InputTag))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RpgGameplayTags::Ability_ActivateFail_TagsBlocked);
		}
		return false;
	}

	if (!WeaponInstance->FindAttackDefinition(ResolveAttackDefinitionTag(Handle, ActorInfo)))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RpgGameplayTags::Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	return true;
}

void URpgGameplayAbility_BasicWeaponAttack::NativeOnAbilityFailedToActivate(
	const FGameplayTagContainer& FailedReason) const
{
	Super::NativeOnAbilityFailedToActivate(FailedReason);
	LogAttackLifecycleLazy(
		TEXT("ActivationRejected"),
		[&FailedReason]()
		{
			return FString::Printf(TEXT("FailureTags=%s"), *FailedReason.ToStringSimple());
		});
}

void URpgGameplayAbility_BasicWeaponAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	ActiveWeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo));
	const FGameplayTag EntryAttackTag = ResolveAttackDefinitionTag(Handle, ActorInfo);
	const FRpgWeaponAttackDefinition* AttackDefinition = ActiveWeaponInstance ? ActiveWeaponInstance->FindAttackDefinition(EntryAttackTag) : nullptr;
	if (!ActiveWeaponInstance || !AttackDefinition)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RolledDamage = -1.0f;
	RolledStaggerDamage = -1.0f;
	EntryAuthoredDamage = AttackDefinition->Damage;
	EntryAuthoredStaggerDamage = AttackDefinition->StaggerDamage;
	if (const URpgInventoryItemInstance* AssociatedItem = GetAssociatedItem();
		AssociatedItem && AssociatedItem->HasGeneratedItemization())
	{
		const FRpgItemizationState& Itemization = AssociatedItem->GetItemizationStateRef();
		auto HasRolledStat = [&Itemization](const FGameplayTag& StatTag)
		{
			return Itemization.BaseStats.ContainsByPredicate(
				[&StatTag](const FRpgRolledItemStat& Stat) { return Stat.StatTag == StatTag; }) ||
				Itemization.Affixes.ContainsByPredicate(
					[&StatTag](const FRpgRolledItemAffix& Affix) { return Affix.StatTag == StatTag; });
		};

		if (HasRolledStat(RpgItemizationGameplayTags::Item_Stat_WeaponDamage))
		{
			RolledDamage = FMath::Max(
				0.0f,
				Itemization.GetTotalValueForStat(RpgItemizationGameplayTags::Item_Stat_WeaponDamage));
		}
		if (HasRolledStat(RpgItemizationGameplayTags::Item_Stat_WeaponStagger))
		{
			RolledStaggerDamage = FMath::Max(
				0.0f,
				Itemization.GetTotalValueForStat(RpgItemizationGameplayTags::Item_Stat_WeaponStagger));
		}
	}
	bWaitingForMontage = false;
	bFinishingAttack = false;
	SwingTrace.Reset();
	AuthorityDamageHitsThisActivation = 0;
	AuthorityDuplicateHitsSkippedThisActivation = 0;
	HitActorsThisWindow.Reset();
	ActiveAttackTag = EntryAttackTag;
	ComboStepIndex = 0;
	bComboWindowOpen = false;
	bComboWindowClosed = false;
	bComboInputBuffered = false;
	bComboRequestPending = false;
	bRecoveryCancelable = false;

	float AttackWindowStartTime = 0.0f;
	float AttackWindowEndTime = 0.0f;
	if (!PrepareAttackStep(*AttackDefinition, ActiveAttackDefinition, AttackWindowStartTime, AttackWindowEndTime))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveAttackWindowStartTime = AttackWindowStartTime;
	ActiveAttackWindowEndTime = AttackWindowEndTime;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActiveAttackDefinition.CameraMode)
	{
		SetCameraMode(ActiveAttackDefinition.CameraMode);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// Aim is chosen at the start: the windup may still steer toward the camera, the swing and recovery hold it.
	BeginFacingCommitment(WindupSteeringRate);
	ApplyStepOwnerTags();
	LogAttackLifecycleLazy(
		ActorInfo->IsNetAuthority()
			? TEXT("ActivationAccepted.Server")
			: TEXT("ActivationPredicted.Client"),
		[this, AttackWindowStartTime, AttackWindowEndTime]()
		{
			return FString::Printf(
				TEXT("AuthoredWindow=%.3f..%.3f PlayRate=%.3f"),
				AttackWindowStartTime,
				AttackWindowEndTime,
				ActiveAttackDefinition.MontagePlayRate);
		});

	UAbilityTask_WaitGameplayEvent* WindowStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_Start,
		nullptr,
		false,
		true);
	WindowStartTask->EventReceived.AddDynamic(this, &ThisClass::OnAttackWindowStarted);
	WindowStartTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_End,
		nullptr,
		false,
		true);
	WindowEndTask->EventReceived.AddDynamic(this, &ThisClass::OnAttackWindowEnded);
	WindowEndTask->ReadyForActivation();

	// Presses while active reach the owner as InputPressed; a remote owner's chain steps reach the server as GameCustom1.
	if (UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get())
	{
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		if (ActorInfo->IsLocallyControlled())
		{
			ComboInputPressedHandle = AbilitySystem->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::InputPressed, Handle, ActivationKey).AddUObject(this, &ThisClass::OnComboInputPressed);
		}
		else if (ActorInfo->IsNetAuthority())
		{
			ComboRequestHandle = AbilitySystem->AbilityReplicatedEventDelegate(
				EAbilityGenericReplicatedEvent::GameCustom1, Handle, ActivationKey).AddUObject(this, &ThisClass::OnComboStepRequested);
		}
	}

	if (!StartAttackMontage())
	{
		FinishAttack(true);
	}
}

bool URpgGameplayAbility_BasicWeaponAttack::PrepareAttackStep(
	const FRpgWeaponAttackDefinition& Source,
	FRpgWeaponAttackDefinition& OutStep,
	float& OutWindowStartTime,
	float& OutWindowEndTime)
{
	OutStep = Source;
	// An item roll replaces the entry attack's damage; later combo steps keep their authored ratio to the entry.
	if (RolledDamage >= 0.0f)
	{
		OutStep.Damage = EntryAuthoredDamage > UE_KINDA_SMALL_NUMBER
			? Source.Damage * RolledDamage / EntryAuthoredDamage
			: RolledDamage;
	}
	if (RolledStaggerDamage >= 0.0f)
	{
		OutStep.StaggerDamage = EntryAuthoredStaggerDamage > UE_KINDA_SMALL_NUMBER
			? Source.StaggerDamage * RolledStaggerDamage / EntryAuthoredStaggerDamage
			: RolledStaggerDamage;
	}
	if (!OutStep.CanApplyDamage())
	{
		return false;
	}

	if (!OutStep.Montage || !OutStep.HasValidTraceData())
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] cannot run: Montage=%s TraceMode=%d TracePoints=%d TraceRadius=%.2f TraceSampleInterval=%.3f."),
			*GetNameSafe(this),
			*GetNameSafe(OutStep.Montage),
			static_cast<int32>(OutStep.TraceMode),
			OutStep.TracePointSockets.Num(),
			OutStep.TraceRadius,
			OutStep.TraceSampleInterval);
		return false;
	}
	if (!FMath::IsFinite(OutStep.MontagePlayRate) ||
		OutStep.MontagePlayRate <= UE_SMALL_NUMBER)
	{
		UE_LOG(
			LogRpgWeapons,
			Error,
			TEXT("BasicWeaponAttack[%s] cannot run montage [%s] with invalid play rate %.6f."),
			*GetNameSafe(this),
			*GetNameSafe(OutStep.Montage),
			OutStep.MontagePlayRate);
		LogAttackLifecycleLazy(
			TEXT("ActivationRejected.InvalidPlayRate"),
			[&OutStep]()
			{
				return FString::Printf(
					TEXT("PlayRate=%.6f"),
					OutStep.MontagePlayRate);
			});
		return false;
	}
	const AActor* AttackAvatar = GetAvatarActorFromActorInfo();
	if (AttackAvatar && !FMath::IsNearlyEqual(
			AttackAvatar->CustomTimeDilation,
			1.0f,
			KINDA_SMALL_NUMBER))
	{
		const FString FailureReason = FString::Printf(
			TEXT("Avatar CustomTimeDilation %.6f is unsupported by the one-shot authority schedule."),
			AttackAvatar->CustomTimeDilation);
		UE_LOG(LogRpgWeapons, Error, TEXT("BasicWeaponAttack[%s] cannot run: %s"),
			*GetNameSafe(this), *FailureReason);
		LogAttackLifecycle(TEXT("ActivationRejected.CustomTimeDilation"), FailureReason);
		return false;
	}

	FString AttackWindowFailureReason;
	float ConfiguredTaskPlayRate = OutStep.MontagePlayRate;
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(ConfiguredTaskPlayRate);
	const float ConfiguredEffectivePlayRate = ConfiguredTaskPlayRate *
		OutStep.Montage->RateScale;
	if (!FRpgSwingTraceWindow::ResolveWindowTiming(
			OutStep.Montage,
			ConfiguredEffectivePlayRate,
			OutWindowStartTime,
			OutWindowEndTime,
			AttackWindowFailureReason))
	{
		UE_LOG(
			LogRpgWeapons,
			Error,
			TEXT("BasicWeaponAttack[%s] cannot run montage [%s]: %s"),
			*GetNameSafe(this),
			*GetNameSafe(OutStep.Montage),
			*AttackWindowFailureReason);
		LogAttackLifecycle(TEXT("ActivationRejected.InvalidAttackWindow"), AttackWindowFailureReason);
		return false;
	}
	return true;
}

bool URpgGameplayAbility_BasicWeaponAttack::StartAttackMontage()
{
	bWaitingForMontage = true;
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ActiveAttackDefinition.Montage,
		ActiveAttackDefinition.MontagePlayRate);

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	ActiveMontageTask = MontageTask;
	MontageTask->ReadyForActivation();

	if (!IsActive())
	{
		// The montage could not start, and its task already finished the attack.
		return true;
	}
	if (!SwingTrace.Begin(
			*this,
			ActiveWeaponInstance,
			ActiveAttackDefinition.Montage,
			ActiveAttackDefinition.MakeSwingTraceParams(),
			MakeSwingTraceSink()))
	{
		LogAttackLifecycle(
			TEXT("ActivationAborted.AuthoritySchedule"),
			TEXT("The server could not establish an authoritative attack window."));
		return false;
	}
	ScheduleComboTimers();
	LogAttackLifecycle(TEXT("MontageStarted"));
	return true;
}

void URpgGameplayAbility_BasicWeaponAttack::DetachAttackMontageTask()
{
	if (UAbilityTask_PlayMontageAndWait* MontageTask = ActiveMontageTask.Get())
	{
		MontageTask->OnCompleted.RemoveAll(this);
		MontageTask->OnBlendOut.RemoveAll(this);
		MontageTask->OnInterrupted.RemoveAll(this);
		MontageTask->OnCancelled.RemoveAll(this);
		// Ending the task alone keeps its montage playing until the next step's montage replaces it.
		MontageTask->EndTask();
	}
	ActiveMontageTask = nullptr;
	bWaitingForMontage = false;
}

void URpgGameplayAbility_BasicWeaponAttack::ScheduleComboTimers()
{
	ClearComboTimers();
	UWorld* World = GetWorld();
	const UAnimMontage* Montage = ActiveAttackDefinition.Montage;
	if (!World || !Montage || !CurrentActorInfo)
	{
		return;
	}

	float PlayRate = ActiveAttackDefinition.MontagePlayRate;
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(PlayRate);
	PlayRate *= Montage->RateScale;
	float Position = 0.0f;
	if (UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance();
		AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		PlayRate = AnimInstance->Montage_GetEffectivePlayRate(Montage);
		Position = AnimInstance->Montage_GetPosition(Montage);
	}
	if (!FMath::IsFinite(PlayRate) || PlayRate <= UE_SMALL_NUMBER)
	{
		return;
	}

	// A remote owner acts at its own window edge; its request reaches a server montage that started later by about the
	// same latency, so only jitter separates the two clocks.
	const bool bRemoteOwner = CurrentActorInfo->IsNetAuthority() && !CurrentActorInfo->IsLocallyControlled();
	const float Lead = bRemoteOwner ? ComboServerLeadTolerance : 0.0f;
	auto Schedule = [this, World, Position, PlayRate](FTimerHandle& TimerHandle, void (ThisClass::*Callback)(), float MontageTime)
	{
		// Never call back synchronously: a chain started here would replace the step that is still being scheduled.
		const float Delay = FMath::Max((MontageTime - Position) / PlayRate, UE_KINDA_SMALL_NUMBER);
		World->GetTimerManager().SetTimer(TimerHandle, this, Callback, Delay, false);
	};

	Schedule(RecoveryCancelTimerHandle, &ThisClass::OnRecoveryCancelOpened, ActiveAttackWindowEndTime - Lead);
	if (!ActiveAttackDefinition.ComboNextAttack.IsValid())
	{
		return;
	}
	float ComboWindowStart = 0.0f;
	float ComboWindowEnd = 0.0f;
	FString Failure;
	if (!UAnimNotifyState_RpgComboWindow::FindComboWindow(Montage, ComboWindowStart, ComboWindowEnd, Failure))
	{
		LogAttackLifecycle(TEXT("ComboWindowMissing"), Failure);
		return;
	}
	Schedule(ComboWindowOpenTimerHandle, &ThisClass::OnComboWindowOpened, ComboWindowStart - Lead);
	// The server keeps accepting a remote owner's step until this step ends; the owner sent it inside its window.
	if (!bRemoteOwner)
	{
		Schedule(ComboWindowCloseTimerHandle, &ThisClass::OnComboWindowClosed, ComboWindowEnd);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::ClearComboTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
		World->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryCancelTimerHandle);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OnComboWindowOpened()
{
	if (!IsActive() || bFinishingAttack || !CurrentActorInfo)
	{
		return;
	}
	bComboWindowOpen = true;
	LogAttackLifecycle(TEXT("ComboWindowOpened"));
	if (CurrentActorInfo->IsLocallyControlled())
	{
		if (bComboInputBuffered)
		{
			ChainComboFromOwner();
		}
	}
	else if (bComboRequestPending)
	{
		bComboRequestPending = false;
		FScopedPredictionWindow ScopedPrediction(GetAbilitySystemComponentFromActorInfo(), PendingComboPredictionKey);
		StartComboStep(ActiveAttackDefinition.ComboNextAttack);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OnComboWindowClosed()
{
	bComboWindowOpen = false;
	bComboWindowClosed = true;
	bComboInputBuffered = false;
	LogAttackLifecycle(TEXT("ComboWindowClosed"));
}

void URpgGameplayAbility_BasicWeaponAttack::OnRecoveryCancelOpened()
{
	if (!IsActive() || bFinishingAttack || bRecoveryCancelable)
	{
		return;
	}
	// Lyra's activation groups then let dodge, block or another exclusive ability replace the recovery.
	if (CanChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Replaceable)
		&& ChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Replaceable))
	{
		bRecoveryCancelable = true;
		LogAttackLifecycle(TEXT("RecoveryCancelable"));
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OnComboInputPressed()
{
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey());
	}
	if (!IsActive() || bFinishingAttack || !ActiveAttackDefinition.ComboNextAttack.IsValid() || bComboWindowClosed)
	{
		return;
	}
	if (bComboWindowOpen)
	{
		ChainComboFromOwner();
	}
	else
	{
		bComboInputBuffered = true;
		LogAttackLifecycle(TEXT("ComboInputBuffered"));
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OnComboStepRequested()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (AbilitySystem)
	{
		AbilitySystem->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::GameCustom1, CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey());
	}
	if (!IsActive() || bFinishingAttack || !AbilitySystem)
	{
		return;
	}
	if (bComboRequestPending)
	{
		// Requests arrive in order; play the earlier step before judging this one.
		bComboRequestPending = false;
		FScopedPredictionWindow ScopedPrediction(AbilitySystem, PendingComboPredictionKey);
		if (!StartComboStep(ActiveAttackDefinition.ComboNextAttack))
		{
			return;
		}
	}
	if (!ActiveAttackDefinition.ComboNextAttack.IsValid())
	{
		LogAttackLifecycle(TEXT("ComboRequestRejected"), TEXT("The running step has no next attack."));
		return;
	}
	if (bComboWindowOpen)
	{
		// Still inside the request's prediction window: the montage replicates under the owner's key.
		StartComboStep(ActiveAttackDefinition.ComboNextAttack);
		return;
	}
	bComboRequestPending = true;
	PendingComboPredictionKey = AbilitySystem->ScopedPredictionKey;
	LogAttackLifecycle(TEXT("ComboRequestDeferred"));
}

void URpgGameplayAbility_BasicWeaponAttack::ChainComboFromOwner()
{
	bComboInputBuffered = false;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	const FGameplayTag NextAttackTag = ActiveAttackDefinition.ComboNextAttack;
	const FRpgWeaponAttackDefinition* Source = ActiveWeaponInstance ? ActiveWeaponInstance->FindAttackDefinition(NextAttackTag) : nullptr;
	FRpgWeaponAttackDefinition CheckedStep;
	float WindowStart = 0.0f;
	float WindowEnd = 0.0f;
	// Check before telling the server, so the owner never requests a step it cannot play itself.
	if (!AbilitySystem || !CurrentActorInfo || !Source || !PrepareAttackStep(*Source, CheckedStep, WindowStart, WindowEnd))
	{
		LogAttackLifecycle(TEXT("ComboStepRejected"), NextAttackTag.ToString());
		return;
	}
	if (CurrentActorInfo->IsNetAuthority())
	{
		StartComboStep(NextAttackTag);
		return;
	}
	// The new key makes the montage below a prediction; the server plays its step under the same key.
	FScopedPredictionWindow ScopedPrediction(AbilitySystem, true);
	AbilitySystem->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GameCustom1, CurrentSpecHandle,
		CurrentActivationInfo.GetActivationPredictionKey(), AbilitySystem->ScopedPredictionKey);
	StartComboStep(NextAttackTag);
}

bool URpgGameplayAbility_BasicWeaponAttack::StartComboStep(FGameplayTag NextAttackTag)
{
	const FRpgWeaponAttackDefinition* Source = ActiveWeaponInstance ? ActiveWeaponInstance->FindAttackDefinition(NextAttackTag) : nullptr;
	FRpgWeaponAttackDefinition NextStep;
	float WindowStart = 0.0f;
	float WindowEnd = 0.0f;
	if (!Source || !PrepareAttackStep(*Source, NextStep, WindowStart, WindowEnd))
	{
		LogAttackLifecycle(TEXT("ComboStepRejected"), NextAttackTag.ToString());
		return false;
	}

	DetachAttackMontageTask();
	// The owner chains after this step's damage window; a lagging server cuts at most its lead tolerance from it.
	SwingTrace.StopTracing(false);
	ClearComboTimers();
	if (bRecoveryCancelable)
	{
		ChangeActivationGroup(ERpgAbilityActivationGroup::Exclusive_Blocking);
		bRecoveryCancelable = false;
	}
	bComboWindowOpen = false;
	bComboWindowClosed = false;
	bComboInputBuffered = false;
	bComboRequestPending = false;
	HitActorsThisWindow.Reset();

	ActiveAttackDefinition = MoveTemp(NextStep);
	ActiveAttackTag = NextAttackTag;
	ActiveAttackWindowStartTime = WindowStart;
	ActiveAttackWindowEndTime = WindowEnd;
	++ComboStepIndex;
	ApplyStepOwnerTags();
	if (ActiveAttackDefinition.CameraMode)
	{
		SetCameraMode(ActiveAttackDefinition.CameraMode);
	}
	else
	{
		ClearCameraMode();
	}
	// Every step may re-aim during its windup, like the first attack.
	BeginFacingCommitment(WindupSteeringRate);
	LogAttackLifecycleLazy(
		TEXT("ComboStepStarted"),
		[this, WindowStart, WindowEnd]()
		{
			return FString::Printf(TEXT("Step=%d Attack=%s AuthoredWindow=%.3f..%.3f"),
				ComboStepIndex, *ActiveAttackTag.ToString(), WindowStart, WindowEnd);
		});

	if (!StartAttackMontage())
	{
		FinishAttack(true);
		return false;
	}
	return true;
}

void URpgGameplayAbility_BasicWeaponAttack::ApplyStepOwnerTags()
{
	RemoveStepOwnerTags();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() ||
		ActiveAttackDefinition.OwnerTagsDuringAttack.IsEmpty())
	{
		return;
	}
	// Only server rules read these tags, so they stay unreplicated loose tags with their own count.
	AppliedStepOwnerTags = ActiveAttackDefinition.OwnerTagsDuringAttack;
	AbilitySystem->AddLooseGameplayTags(AppliedStepOwnerTags);
}

void URpgGameplayAbility_BasicWeaponAttack::RemoveStepOwnerTags()
{
	if (AppliedStepOwnerTags.IsEmpty())
	{
		return;
	}
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo())
	{
		AbilitySystem->RemoveLooseGameplayTags(AppliedStepOwnerTags);
	}
	AppliedStepOwnerTags.Reset();
}

void URpgGameplayAbility_BasicWeaponAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	SwingTrace.StopTracing(false);
	ClearComboTimers();
	RemoveStepOwnerTags();
	if (UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		if (ComboInputPressedHandle.IsValid())
		{
			AbilitySystem->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputPressed, Handle, ActivationKey)
				.Remove(ComboInputPressedHandle);
		}
		if (ComboRequestHandle.IsValid())
		{
			AbilitySystem->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::GameCustom1, Handle, ActivationKey)
				.Remove(ComboRequestHandle);
		}
	}
	ComboInputPressedHandle.Reset();
	ComboRequestHandle.Reset();
	const bool bHasPendingTimers = SwingTrace.HasPendingTimers();
	LogAttackLifecycleLazy(
		TEXT("AbilityEnded"),
		[this, bWasCancelled, bHasPendingTimers]()
		{
			return FString::Printf(
				TEXT("Cancelled=%d NotifyStart=%d NotifyEnd=%d ScheduledOpen=%d ScheduledClose=%d TraceSamples=%d TracePointFailures=%d DamageHits=%d DuplicateHitsSkipped=%d PendingTimers=%d"),
				bWasCancelled,
				SwingTrace.HasReceivedStartNotify(),
				SwingTrace.HasReceivedEndNotify(),
				SwingTrace.WasOpenedBySchedule(),
				SwingTrace.WasClosedBySchedule(),
				SwingTrace.GetAuthorityTraceSampleCount(),
				SwingTrace.GetAuthorityTracePointFailureCount(),
				AuthorityDamageHitsThisActivation,
				AuthorityDuplicateHitsSkippedThisActivation,
				bHasPendingTimers);
		});

	ActiveWeaponInstance = nullptr;
	ActiveMontageTask = nullptr;
	ActiveAttackDefinition = FRpgWeaponAttackDefinition();
	ActiveAttackTag = FGameplayTag();
	ActiveAttackWindowStartTime = 0.0f;
	ActiveAttackWindowEndTime = 0.0f;
	ComboStepIndex = 0;
	bComboWindowOpen = false;
	bComboWindowClosed = false;
	bComboInputBuffered = false;
	bComboRequestPending = false;
	// The activation group leaves the ASC with the ability, whichever group it holds now.
	bRecoveryCancelable = false;
	PendingComboPredictionKey = FPredictionKey();
	bWaitingForMontage = false;
	bFinishingAttack = false;
	SwingTrace.Reset();
	AuthorityDamageHitsThisActivation = 0;
	AuthorityDuplicateHitsSkippedThisActivation = 0;
	HitActorsThisWindow.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// The ASC has released the group this activation ended in; the reused instance starts blocking again.
	ActivationGroup = GetClass()->GetDefaultObject<ThisClass>()->GetActivationGroup();
}

void URpgGameplayAbility_BasicWeaponAttack::OnAttackWindowStarted(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	SwingTrace.HandleStartNotify();
}

void URpgGameplayAbility_BasicWeaponAttack::OnAttackWindowEnded(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	SwingTrace.HandleEndNotify();
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageCompleted()
{
	HandleMontageEnded(TEXT("MontageCompleted"), false);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageBlendOut()
{
	HandleMontageEnded(TEXT("MontageBlendOut"), false);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageInterrupted()
{
	HandleMontageEnded(TEXT("MontageInterrupted"), true);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageCancelled()
{
	HandleMontageEnded(TEXT("MontageCancelled"), true);
}

void URpgGameplayAbility_BasicWeaponAttack::HandleMontageEnded(
	const TCHAR* Stage,
	const bool bWasCancelled)
{
	bWaitingForMontage = false;

	if (SwingTrace.IsWindowOpen())
	{
		SwingTrace.StopTracing(!bWasCancelled);
	}
	else if (!bWasCancelled && !SwingTrace.HasReceivedStartNotify())
	{
		if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
		{
			if (SwingTrace.WasOpenedBySchedule())
			{
				LogAttackLifecycle(
					TEXT("MontageEnded.NotifyStartMissing"),
					TEXT("Authority schedule preserved the damage window."));
			}
			else
			{
				UE_LOG(
					LogRpgWeapons,
					Warning,
					TEXT("BasicWeaponAttack[%s] authority montage [%s] finished without an AttackWindowStart notify or authority schedule; no damage window opened."),
					*GetNameSafe(this),
					*GetNameSafe(ActiveAttackDefinition.Montage));
			}
		}
		else
		{
			LogAttackLifecycle(
				TEXT("MontageEnded.ClientNotifyStartMissing"),
				TEXT("The local cosmetic window remained closed; authority owns damage and traces."));
		}
	}

	LogAttackLifecycle(Stage);
	FinishAttack(bWasCancelled);
}

#if WITH_DEV_AUTOMATION_TESTS
bool URpgGameplayAbility_BasicWeaponAttack::IsAttackWindowEndBeforeAutoBlendOutForTests(
	const float MontageLength,
	const float WindowEndTime,
	const float EffectivePlayRate,
	const float AuthoredBlendOutTriggerTime)
{
	float RemainingPlayTime = 0.0f;
	float BlendOutTriggerSeconds = 0.0f;
	return FRpgSwingTraceWindow::IsWindowEndBeforeAutoBlendOut(
		MontageLength,
		WindowEndTime,
		EffectivePlayRate,
		AuthoredBlendOutTriggerTime,
		RemainingPlayTime,
		BlendOutTriggerSeconds);
}
#endif

void URpgGameplayAbility_BasicWeaponAttack::LogAttackLifecycle(
	const TCHAR* Stage,
	const FString& Detail) const
{
	if (!ShouldLogWeaponAttackLifecycle())
	{
		return;
	}

	WriteAttackLifecycle(Stage, Detail);
}

void URpgGameplayAbility_BasicWeaponAttack::LogAttackLifecycleLazy(
	const TCHAR* Stage,
	TFunctionRef<FString()> DetailBuilder) const
{
	if (!ShouldLogWeaponAttackLifecycle())
	{
		return;
	}

	WriteAttackLifecycle(Stage, DetailBuilder());
}

void URpgGameplayAbility_BasicWeaponAttack::WriteAttackLifecycle(
	const TCHAR* Stage,
	const FString& Detail) const
{
	const AActor* AvatarActor = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = AvatarActor ? AvatarActor->GetWorld() : GetWorld();
	const UAnimMontage* Montage = ActiveAttackDefinition.Montage;
	const UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	const float MontagePosition = AnimInstance && Montage
		? AnimInstance->Montage_GetPosition(Montage)
		: -1.0f;
	const FName MontageSection = AnimInstance && Montage
		? AnimInstance->Montage_GetCurrentSection(Montage)
		: NAME_None;
	const FString PredictionKey = CurrentActivationInfo.GetActivationPredictionKey().ToString();

	UE_LOG(
		LogRpgWeapons,
		Log,
		TEXT("WeaponAttackLifecycle Stage=%s Ability=%s Spec=%s Avatar=%s NetMode=%d LocalRole=%d RemoteRole=%d WorldTime=%.3f Montage=%s Section=%s Position=%.3f PredictionKey=%s Detail=%s"),
		Stage ? Stage : TEXT("Unknown"),
		*GetNameSafe(this),
		*CurrentSpecHandle.ToString(),
		*GetNameSafe(AvatarActor),
		World ? static_cast<int32>(World->GetNetMode()) : INDEX_NONE,
		AvatarActor ? static_cast<int32>(AvatarActor->GetLocalRole()) : INDEX_NONE,
		AvatarActor ? static_cast<int32>(AvatarActor->GetRemoteRole()) : INDEX_NONE,
		World ? World->GetTimeSeconds() : -1.0,
		*GetNameSafe(Montage),
		*MontageSection.ToString(),
		MontagePosition,
		*PredictionKey,
		Detail.IsEmpty() ? TEXT("-") : *Detail);
}

FGameplayTag URpgGameplayAbility_BasicWeaponAttack::ResolveAttackDefinitionTag(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (bRouteAttackDefinitionFromInputTag)
	{
		const FGameplayTag InputTag = GetInputTagFromSpec(Handle, ActorInfo);
		if (InputTag == RpgGameplayTags::InputTag_Weapon_Secondary)
		{
			return RpgGameplayTags::Weapon_Attack_Secondary;
		}
		if (InputTag == RpgGameplayTags::InputTag_Weapon_Primary)
		{
			return RpgGameplayTags::Weapon_Attack_Primary;
		}
	}

	return AttackDefinitionTag;
}

FRpgSwingTraceSink URpgGameplayAbility_BasicWeaponAttack::MakeSwingTraceSink()
{
	FRpgSwingTraceSink Sink;
	Sink.OnHits.BindUObject(this, &ThisClass::HandleSwingHits);
	Sink.OnTraceQuery.BindUObject(this, &ThisClass::RecordSwingTrace);
	Sink.OnWindowOpened.BindUObject(this, &ThisClass::HandleSwingWindowOpened);
	Sink.OnWindowClosed.BindUObject(this, &ThisClass::HandleSwingWindowClosed);
	Sink.OnLifecycle.BindUObject(this, &ThisClass::LogAttackLifecycleLazy);
	return Sink;
}

void URpgGameplayAbility_BasicWeaponAttack::HandleSwingWindowOpened(const ERpgSwingTraceRole Role)
{
	// Owner (notify) and authority (notify, montage poll or schedule) each hold their own facing from the swing until the
	// attack ends.
	HoldFacingCommitment();
	HitActorsThisWindow.Reset();
	BeginSwingRecord(Role == ERpgSwingTraceRole::Authority);
}

void URpgGameplayAbility_BasicWeaponAttack::HandleSwingWindowClosed(ERpgSwingTraceRole Role)
{
	FinishSwingRecord();
	LogAttackLifecycleLazy(
		TEXT("AttackWindowClosed"),
		[this]()
		{
			return FString::Printf(
				TEXT("TraceSamples=%d MaxTraceSubsteps=%d TracePointFailures=%d DamageHits=%d DuplicateHitsSkipped=%d"),
				SwingTrace.GetAuthorityTraceSampleCount(),
				SwingTrace.GetAuthorityMaxTraceSubstepCount(),
				SwingTrace.GetAuthorityTracePointFailureCount(),
				AuthorityDamageHitsThisActivation,
				AuthorityDuplicateHitsSkippedThisActivation);
		});
}

void URpgGameplayAbility_BasicWeaponAttack::BeginSwingRecord(const bool bAuthority)
{
	const UWorld* World = GetWorld();
	bRecordingSwing = bAuthority && World && ActiveWeaponInstance && ActiveWeaponInstance->WantsSwingRecords();
	SwingRecord = FRpgWeaponSwingRecord();
	if (bRecordingSwing)
	{
		SwingRecord.WindowStartTime = World->GetTimeSeconds();
		DamageHitsBeforeSwing = AuthorityDamageHitsThisActivation;
	}
}

void URpgGameplayAbility_BasicWeaponAttack::RecordSwingTrace(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const ERpgRecordedTraceShape Shape,
	const FVector& ShapeExtent)
{
	const UWorld* World = GetWorld();
	if (!bRecordingSwing || !World)
	{
		return;
	}

	FRpgRecordedWeaponTrace& Trace = SwingRecord.Traces.AddDefaulted_GetRef();
	Trace.TimeOffset = World->GetTimeSeconds() - SwingRecord.WindowStartTime;
	Trace.Start = TraceStart;
	Trace.End = TraceEnd;
	Trace.Shape = Shape;
	Trace.ShapeExtent = ShapeExtent;
}

void URpgGameplayAbility_BasicWeaponAttack::FinishSwingRecord()
{
	if (!bRecordingSwing)
	{
		return;
	}

	bRecordingSwing = false;
	const UWorld* World = GetWorld();
	SwingRecord.WindowEndTime = World ? World->GetTimeSeconds() : SwingRecord.WindowStartTime;
	if (ActiveWeaponInstance)
	{
		// A swing counts as landed when it dealt health damage to at least one target.
		ActiveWeaponInstance->HandleAttackSwingResolved(SwingRecord, AuthorityDamageHitsThisActivation > DamageHitsBeforeSwing);
	}
	SwingRecord = FRpgWeaponSwingRecord();
}

void URpgGameplayAbility_BasicWeaponAttack::HandleSwingHits(const TArray<FHitResult>& HitResults, const ERpgSwingTraceRole Role)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const bool bAuthority = Role == ERpgSwingTraceRole::Authority;
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		const TObjectKey<AActor> TargetKey(TargetActor);
		if (!TargetActor || TargetActor == AvatarActor)
		{
			continue;
		}
		if (HitActorsThisWindow.Contains(TargetKey))
		{
			++AuthorityDuplicateHitsSkippedThisActivation;
			if (AuthorityDuplicateHitsSkippedThisActivation == 1 && bAuthority)
			{
				LogAttackLifecycleLazy(
					TEXT("AuthorityDuplicateHitSkipped"),
					[TargetActor]()
					{
						return FString::Printf(TEXT("Target=%s"), *GetNameSafe(TargetActor));
					});
			}
			continue;
		}

		HitActorsThisWindow.Add(TargetKey);
		if (bAuthority)
		{
			ApplyDamageToHitActor(TargetActor, HitResult);
		}
		else
		{
			PredictImpactFeedback(TargetActor, HitResult);
		}
	}
}

void URpgGameplayAbility_BasicWeaponAttack::EvaluateConditionalModifiers(
	const UAbilitySystemComponent* TargetASC,
	float& Damage,
	float& StaggerDamage) const
{
	if (!TargetASC || ActiveAttackDefinition.ConditionalModifiers.IsEmpty())
	{
		return;
	}

	FGameplayTagContainer TargetTags;
	TargetASC->GetOwnedGameplayTags(TargetTags);

	for (const FRpgConditionalAttackModifier& Modifier : ActiveAttackDefinition.ConditionalModifiers)
	{
		if (Modifier.MatchesTargetTags(TargetTags))
		{
			Damage *= FMath::Max(0.0f, Modifier.DamageMultiplier);
			StaggerDamage *= FMath::Max(0.0f, Modifier.StaggerDamageMultiplier);
		}
	}
}

FGameplayEffectSpecHandle URpgGameplayAbility_BasicWeaponAttack::MakeWeaponDamageEffectSpec(
	const FHitResult& HitResult,
	const UAbilitySystemComponent* TargetASC) const
{
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(ActiveAttackDefinition.DamageEffect, GetAbilityLevel());
	if (FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get())
	{
		float Damage = GetTunedValue(RpgGameplayTags::Ability_Tuning_Attack_Damage, ActiveAttackDefinition.Damage);
		float StaggerDamage = ActiveAttackDefinition.StaggerDamage;
		EvaluateConditionalModifiers(TargetASC, Damage, StaggerDamage);

		DamageSpec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, FMath::Max(0.0f, Damage));
		DamageSpec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, FMath::Max(0.0f, StaggerDamage));
		DamageSpec->AppendDynamicAssetTags(ActiveAttackDefinition.DamageTypeTags);
		if (ActiveAttackDefinition.HitCueTag.IsValid())
		{
			// The damage execution resolves hit, block or perfect block and plays the matching impact cue.
			DamageSpec->AddDynamicAssetTag(ActiveAttackDefinition.HitCueTag);
		}
		DamageSpec->GetContext().AddHitResult(HitResult, true);
	}

	return DamageSpecHandle;
}

void URpgGameplayAbility_BasicWeaponAttack::ApplyDamageToHitActor(AActor* TargetActor, const FHitResult& HitResult)
{
	if (!TargetActor)
	{
		LogAttackLifecycle(TEXT("AuthorityDamageRejected"), TEXT("Reason=MissingTarget"));
		return;
	}

	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	if (!HealthComponent || HealthComponent->IsDeadOrDying())
	{
		LogAttackLifecycleLazy(
			TEXT("AuthorityDamageRejected"),
			[TargetActor, HealthComponent]()
			{
				return FString::Printf(
					TEXT("Target=%s Reason=%s"),
					*GetNameSafe(TargetActor),
					HealthComponent ? TEXT("DeadOrDying") : TEXT("MissingHealthComponent"));
			});
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		LogAttackLifecycleLazy(
			TEXT("AuthorityDamageRejected"),
			[TargetActor]()
			{
				return FString::Printf(
					TEXT("Target=%s Reason=MissingASC"),
					*GetNameSafe(TargetActor));
			});
		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle = MakeWeaponDamageEffectSpec(HitResult, TargetASC);
	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	if (!DamageSpec)
	{
		LogAttackLifecycleLazy(
			TEXT("AuthorityDamageRejected"),
			[TargetActor]()
			{
				return FString::Printf(
					TEXT("Target=%s Reason=InvalidDamageSpec"),
					*GetNameSafe(TargetActor));
			});
		return;
	}

	if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
	{
		const float HealthBefore = HealthComponent->GetHealth();
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
		const float HealthAfter = HealthComponent->GetHealth();
		if (HealthAfter < HealthBefore)
		{
			++AuthorityDamageHitsThisActivation;
#if WITH_DEV_AUTOMATION_TESTS
			++AuthorityDamageHitCountForTests;
#endif
			LogAttackLifecycleLazy(
				TEXT("AuthorityDamageApplied"),
				[TargetActor, HealthBefore, HealthAfter]()
				{
					return FString::Printf(
						TEXT("Target=%s HealthBefore=%.3f HealthAfter=%.3f"),
						*GetNameSafe(TargetActor),
						HealthBefore,
						HealthAfter);
				});
			SendHitReactionEvent(TargetActor, HitResult, DamageSpec);
		}
		else
		{
			LogAttackLifecycleLazy(
				TEXT("AuthorityDamageRejected"),
				[TargetActor, HealthBefore, HealthAfter]()
				{
					return FString::Printf(
						TEXT("Target=%s Reason=NoHealthDrop HealthBefore=%.3f HealthAfter=%.3f"),
						*GetNameSafe(TargetActor),
						HealthBefore,
						HealthAfter);
				});
		}
	}
	else
	{
		LogAttackLifecycleLazy(
			TEXT("AuthorityDamageRejected"),
			[TargetActor]()
			{
				return FString::Printf(
					TEXT("Target=%s Reason=MissingSourceASC"),
					*GetNameSafe(TargetActor));
			});
	}
}

void URpgGameplayAbility_BasicWeaponAttack::PredictImpactFeedback(AActor* TargetActor, const FHitResult& HitResult)
{
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!HealthComponent || HealthComponent->IsDeadOrDying() || !TargetASC || !AvatarActor)
	{
		return;
	}

	// The spec the server will apply carries the attack's hit cue, damage type and instigators.
	const FGameplayEffectSpecHandle DamageSpecHandle = MakeWeaponDamageEffectSpec(HitResult, TargetASC);
	const FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	const FGameplayTag HitCueTag = DamageSpec ? RpgCombatImpact::FindHitCueTag(*DamageSpec) : FGameplayTag();
	if (!HitCueTag.IsValid() || !RpgCombatImpact::CanDamageTarget(DamageSpec->GetEffectContext(), AvatarActor, TargetActor))
	{
		return;
	}

	ERpgCombatImpactOutcome Outcome = RpgCombatImpact::HasDamageType(*DamageSpec, RpgGameplayTags::Damage_Type_Melee)
		? RpgCombatImpact::ResolveMeleeOutcome(TargetASC, TargetActor, AvatarActor)
		: ERpgCombatImpactOutcome::Hit;
	if (!RpgCombatImpact::IsPredictedByOwner(Outcome))
	{
		// The server's perfect-block cue reaches the attacker unpredicted; until then the shield takes a normal block.
		Outcome = ERpgCombatImpactOutcome::Block;
	}

	const float Damage = DamageSpec->GetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, false, 0.0f);
	const float StaggerDamage = DamageSpec->GetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, false, 0.0f);
	const FGameplayTag CueTag = RpgCombatImpact::GetOutcomeCueTag(Outcome, HitCueTag);
	RpgCombatImpact::PlayPredictedImpact(
		TargetActor,
		CueTag,
		RpgCombatImpact::MakeCueParameters(DamageSpec->GetEffectContext(), AvatarActor, TargetASC, &HitResult, Damage, StaggerDamage));

#if WITH_DEV_AUTOMATION_TESTS
	++OwnerPredictedImpactCountForTests;
#endif
	LogAttackLifecycleLazy(
		TEXT("OwnerImpactPredicted"),
		[TargetActor, CueTag]()
		{
			return FString::Printf(TEXT("Target=%s Cue=%s"), *GetNameSafe(TargetActor), *CueTag.ToString());
		});
}

void URpgGameplayAbility_BasicWeaponAttack::SendHitReactionEvent(AActor* TargetActor, const FHitResult& HitResult, const FGameplayEffectSpec* DamageSpec) const
{
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	if (!TargetActor || !HealthComponent || HealthComponent->IsDeadOrDying())
	{
		return;
	}

	const FGameplayTag EventTag = ActiveAttackDefinition.HitReactionEventTag.IsValid()
		? ActiveAttackDefinition.HitReactionEventTag
		: RpgGameplayTags::GameplayEvent_HitReaction;

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetAvatarActorFromActorInfo();
	Payload.Target = TargetActor;
	Payload.EventMagnitude = ActiveAttackDefinition.Damage;
	if (DamageSpec)
	{
		Payload.ContextHandle = DamageSpec->GetContext();
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, Payload);
}

void URpgGameplayAbility_BasicWeaponAttack::FinishAttack(bool bWasCancelled)
{
	if (bFinishingAttack || !IsActive())
	{
		return;
	}

	bFinishingAttack = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
