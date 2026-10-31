#include "RpgSwingTraceWindow.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"
#include "SurvivalRpg/Combat/RpgCombatDeveloperSettings.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "TimerManager.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	float GetSwingTraceDebugDuration()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings ? FMath::Max(0.0f, Settings->WeaponAttackTraceDebugDuration) : 0.0f;
	}

	bool ShouldDrawSwingTraces()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings && Settings->bDrawWeaponAttackTraces;
	}

	FColor GetSwingTraceColor(const TArray<FHitResult>& HitResults)
	{
		return HitResults.IsEmpty() ? FColor::Green : FColor::Red;
	}

	void DrawSwingTraceLine(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawSwingTraces())
		{
			return;
		}

		const float Duration = GetSwingTraceDebugDuration();
		const FColor TraceColor = GetSwingTraceColor(HitResults);

		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.5f);
		DrawDebugPoint(World, TraceStart, 8.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 8.0f, FColor::Cyan, false, Duration);

		for (const FHitResult& HitResult : HitResults)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 18.0f, FColor::Yellow, false, Duration);
		}
	}

	void DrawSwingTraceSweep(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FRpgSwingTraceParams& Params,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawSwingTraces())
		{
			return;
		}

		const float Duration = GetSwingTraceDebugDuration();
		const FVector TraceDelta = TraceEnd - TraceStart;
		const FColor TraceColor = GetSwingTraceColor(HitResults);

		DrawDebugPoint(World, TraceStart, 12.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 12.0f, FColor::Cyan, false, Duration);
		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.0f);

		switch (Params.TraceMode)
		{
		case ERpgWeaponAttackTraceMode::SphereSweep:
		{
			const float Radius = FMath::Max(1.0f, Params.TraceRadius);
			const FVector TraceCenter = (TraceStart + TraceEnd) * 0.5f;
			const FQuat TraceRotation = TraceDelta.IsNearlyZero()
				? FQuat::Identity
				: FRotationMatrix::MakeFromZ(TraceDelta).ToQuat();
			DrawDebugCapsule(World, TraceCenter, Radius + (TraceDelta.Size() * 0.5f), Radius, TraceRotation, TraceColor, false, Duration, 0, 1.5f);
			break;
		}
		case ERpgWeaponAttackTraceMode::CapsuleSweep:
		{
			const float Radius = FMath::Max(1.0f, Params.TraceRadius);
			const float HalfHeight = FMath::Max(Radius, Params.TraceCapsuleHalfHeight);
			DrawDebugCapsule(World, TraceStart, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			DrawDebugCapsule(World, TraceEnd, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			break;
		}
		case ERpgWeaponAttackTraceMode::BoxSweep:
		{
			const FVector Extent(
				FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.X)),
				FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.Y)),
				FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.Z)));
			DrawDebugBox(World, TraceStart, Extent, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			DrawDebugBox(World, TraceEnd, Extent, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			break;
		}
		default:
			break;
		}

		for (const FHitResult& HitResult : HitResults)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 18.0f, FColor::Yellow, false, Duration);
		}
	}
}
#endif

bool FRpgSwingTraceParams::HasValidTraceData() const
{
	if (TracePointSockets.Num() < 2 || TraceSampleInterval <= 0.0f)
	{
		return false;
	}

	switch (TraceMode)
	{
	case ERpgWeaponAttackTraceMode::LineTrace:
		return !bTraceBetweenSockets || TraceInterpolationDistance > 0.0f;
	case ERpgWeaponAttackTraceMode::SphereSweep:
		return TraceRadius > 0.0f;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		return TraceRadius > 0.0f && TraceCapsuleHalfHeight > 0.0f;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		return TraceBoxExtent.GetAbsMin() > 0.0f;
	default:
		return false;
	}
}

void FRpgSwingTraceParams::ExtendTip(TArray<FTransform>& TracePointTransforms) const
{
	if (!FMath::IsFinite(TipExtension) || TipExtension <= UE_KINDA_SMALL_NUMBER || TracePointTransforms.Num() < 2)
	{
		return;
	}

	FTransform& Tip = TracePointTransforms.Last();
	const FVector Axis = (Tip.GetLocation() - TracePointTransforms.Last(1).GetLocation()).GetSafeNormal();
	if (!Axis.IsZero())
	{
		Tip.SetLocation(Tip.GetLocation() + Axis * TipExtension);
	}
}

bool FRpgSwingTraceWindow::IsWindowEndBeforeAutoBlendOut(
	const float MontageLength,
	const float WindowEndTime,
	const float EffectivePlayRate,
	const float AuthoredBlendOutTriggerTime,
	float& OutRemainingPlayTime,
	float& OutBlendOutTriggerSeconds)
{
	OutRemainingPlayTime = -1.0f;
	OutBlendOutTriggerSeconds = -1.0f;

	if (!FMath::IsFinite(MontageLength) || MontageLength <= 0.0f ||
		!FMath::IsFinite(WindowEndTime) || WindowEndTime < 0.0f || WindowEndTime > MontageLength ||
		!FMath::IsFinite(EffectivePlayRate) || EffectivePlayRate <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(AuthoredBlendOutTriggerTime) || AuthoredBlendOutTriggerTime < 0.0f)
	{
		return false;
	}

	OutBlendOutTriggerSeconds = FMath::Max(
		AuthoredBlendOutTriggerTime,
		UE_KINDA_SMALL_NUMBER);
	OutRemainingPlayTime = (MontageLength - WindowEndTime) / EffectivePlayRate;
	return FMath::IsFinite(OutRemainingPlayTime) &&
		OutRemainingPlayTime > OutBlendOutTriggerSeconds + UE_KINDA_SMALL_NUMBER;
}

bool FRpgSwingTraceWindow::ResolveWindowTiming(
	const UAnimMontage* Montage,
	const float EffectivePlayRate,
	float& OutStartTime,
	float& OutEndTime,
	FString& OutFailureReason)
{
	OutStartTime = 0.0f;
	OutEndTime = 0.0f;
	OutFailureReason.Reset();

	if (!Montage)
	{
		OutFailureReason = TEXT("No attack montage is configured.");
		return false;
	}
	if (!FMath::IsFinite(EffectivePlayRate) || EffectivePlayRate <= UE_SMALL_NUMBER)
	{
		OutFailureReason = FString::Printf(
			TEXT("The effective montage play rate must be finite and positive; found %.6f (MontageRateScale=%.6f)."),
			EffectivePlayRate,
			Montage->RateScale);
		return false;
	}
	if (Montage->TimeStretchCurve.IsValid())
	{
		OutFailureReason = TEXT("Attack montages with a time-stretch curve are unsupported by the one-shot authority schedule.");
		return false;
	}
	if (Montage->CompositeSections.Num() != 1 ||
		!Montage->CompositeSections[0].NextSectionName.IsNone() ||
		!FMath::IsNearlyZero(Montage->CompositeSections[0].GetTime(), KINDA_SMALL_NUMBER))
	{
		OutFailureReason = FString::Printf(
			TEXT("Attack montage must have exactly one section starting at zero with no section link or jump; found Sections=%d FirstStart=%.3f FirstNext=%s."),
			Montage->CompositeSections.Num(),
			Montage->CompositeSections.IsEmpty()
				? -1.0f
				: Montage->CompositeSections[0].GetTime(),
			Montage->CompositeSections.IsEmpty()
				? TEXT("None")
				: *Montage->CompositeSections[0].NextSectionName.ToString());
		return false;
	}

	const FAnimNotifyEvent* StartEvent = nullptr;
	const FAnimNotifyEvent* EndEvent = nullptr;
	int32 StartEventCount = 0;
	int32 EndEventCount = 0;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.Notify && NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponAttackWindowStart>())
		{
			++StartEventCount;
			StartEvent = &NotifyEvent;
		}
		else if (NotifyEvent.Notify && NotifyEvent.Notify->IsA<UAnimNotify_RpgWeaponAttackWindowEnd>())
		{
			++EndEventCount;
			EndEvent = &NotifyEvent;
		}
	}

	if (StartEventCount != 1 || EndEventCount != 1 || !StartEvent || !EndEvent)
	{
		OutFailureReason = FString::Printf(
			TEXT("Expected exactly one direct AttackWindowStart/AttackWindowEnd notify pair, found Start=%d End=%d."),
			StartEventCount,
			EndEventCount);
		return false;
	}

	auto HasReliableNotifyContract = [](const FAnimNotifyEvent& NotifyEvent)
	{
		return NotifyEvent.NotifyTriggerChance >= 1.0f - KINDA_SMALL_NUMBER &&
			NotifyEvent.bTriggerOnDedicatedServer &&
			NotifyEvent.NotifyFilterType == ENotifyFilterType::NoFiltering &&
			NotifyEvent.MontageTickType == EMontageNotifyTickType::Queued;
	};
	if (!HasReliableNotifyContract(*StartEvent) || !HasReliableNotifyContract(*EndEvent))
	{
		OutFailureReason = TEXT("Attack-window notifies must be queued with 100% trigger chance, NotifyFilterType=NoFiltering, and dedicated-server delivery enabled.");
		return false;
	}

	OutStartTime = StartEvent->GetTriggerTime();
	OutEndTime = EndEvent->GetTriggerTime();
	if (!FMath::IsFinite(OutStartTime) || !FMath::IsFinite(OutEndTime) ||
		OutStartTime < 0.0f || OutEndTime <= OutStartTime || OutEndTime > Montage->GetPlayLength())
	{
		OutFailureReason = FString::Printf(
			TEXT("Attack-window notify times are invalid or unordered: Start=%.3f End=%.3f MontageLength=%.3f."),
			OutStartTime,
			OutEndTime,
			Montage->GetPlayLength());
		return false;
	}

	if (Montage->bEnableAutoBlendOut)
	{
		const float AuthoredBlendOutTriggerTime = Montage->BlendOutTriggerTime >= 0.0f
			? Montage->BlendOutTriggerTime
			: Montage->BlendOut.GetBlendTime();
		float RemainingPlayTimeAtWindowEnd = 0.0f;
		float BlendOutTriggerSeconds = 0.0f;
		if (!IsWindowEndBeforeAutoBlendOut(
				Montage->GetPlayLength(),
				OutEndTime,
				EffectivePlayRate,
				AuthoredBlendOutTriggerTime,
				RemainingPlayTimeAtWindowEnd,
				BlendOutTriggerSeconds))
		{
			OutFailureReason = FString::Printf(
				TEXT("Attack-window end %.3f leaves %.3f seconds at effective play rate %.3f; it must precede the %.3f-second auto-blend-out trigger."),
				OutEndTime,
				RemainingPlayTimeAtWindowEnd,
				EffectivePlayRate,
				BlendOutTriggerSeconds);
			return false;
		}
	}

	return true;
}

bool FRpgSwingTraceWindow::Begin(
	UGameplayAbility& InAbility,
	URpgEquipmentInstance* InEquipment,
	UAnimMontage* InMontage,
	const FRpgSwingTraceParams& InParams,
	FRpgSwingTraceSink InSink)
{
	Ability = &InAbility;
	Equipment = InEquipment;
	Montage = InMontage;
	Params = InParams;
	Sink = MoveTemp(InSink);
	WindowStartTime = 0.0f;
	WindowEndTime = 0.0f;
	bReceivedStartNotify = false;
	bReceivedEndNotify = false;
	bOpenedBySchedule = false;
	bClosedBySchedule = false;
	PreviousSample.Reset();
	return ScheduleAuthorityWindow();
}

void FRpgSwingTraceWindow::Reset()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(TraceSampleTimerHandle);
		TimerManager.ClearTimer(AuthorityStartTimerHandle);
		TimerManager.ClearTimer(AuthorityEndTimerHandle);
		TimerManager.ClearTimer(AuthorityMontagePollTimerHandle);
	}
	// The ability and its sink stay: Reset may run inside a sink callback when a hit ends the ability.
	Equipment.Reset();
	Montage.Reset();
	Params = FRpgSwingTraceParams();
	WindowStartTime = 0.0f;
	WindowEndTime = 0.0f;
	bWindowOpen = false;
	bReceivedStartNotify = false;
	bReceivedEndNotify = false;
	bAuthorityScheduled = false;
	bOpenedBySchedule = false;
	bClosedBySchedule = false;
	AuthorityTraceSamples = 0;
	AuthorityTracePointFailures = 0;
	AuthorityMaxTraceSubsteps = 0;
	bLoggedTracePointFailure = false;
	PreviousSample.Reset();
}

bool FRpgSwingTraceWindow::HasPendingTimers() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FTimerManager& TimerManager = World->GetTimerManager();
	return TimerManager.TimerExists(TraceSampleTimerHandle) ||
		TimerManager.TimerExists(AuthorityStartTimerHandle) ||
		TimerManager.TimerExists(AuthorityEndTimerHandle) ||
		TimerManager.TimerExists(AuthorityMontagePollTimerHandle);
}

bool FRpgSwingTraceWindow::HasResidualState() const
{
	return bWindowOpen || bAuthorityScheduled || !PreviousSample.IsEmpty();
}

UWorld* FRpgSwingTraceWindow::GetWorld() const
{
	return Ability ? Ability->GetWorld() : nullptr;
}

const FGameplayAbilityActorInfo* FRpgSwingTraceWindow::GetActorInfo() const
{
	return Ability ? Ability->GetCurrentActorInfo() : nullptr;
}

bool FRpgSwingTraceWindow::IsAbilityActive() const
{
	return Ability && Ability->IsActive();
}

bool FRpgSwingTraceWindow::IsAuthority() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetActorInfo();
	return ActorInfo && ActorInfo->IsNetAuthority();
}

ERpgSwingTraceRole FRpgSwingTraceWindow::GetRole() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetActorInfo();
	if (!ActorInfo)
	{
		return ERpgSwingTraceRole::None;
	}
	if (ActorInfo->IsNetAuthority())
	{
		return ERpgSwingTraceRole::Authority;
	}
	return ActorInfo->IsLocallyControlled() ? ERpgSwingTraceRole::OwnerFeedback : ERpgSwingTraceRole::None;
}

void FRpgSwingTraceWindow::SetWindowTimer(
	FTimerHandle& Handle,
	void (FRpgSwingTraceWindow::*Callback)(),
	const float Rate,
	const bool bLoop,
	const float FirstDelay,
	const bool bMaxOncePerFrame)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = bLoop;
	TimerParameters.bMaxOncePerFrame = bMaxOncePerFrame;
	TimerParameters.FirstDelay = FirstDelay;
	World->GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateWeakLambda(Ability, [this, Callback]() { (this->*Callback)(); }),
		Rate,
		TimerParameters);
}

void FRpgSwingTraceWindow::LogLifecycle(const TCHAR* Stage, const FString& Detail) const
{
	Sink.OnLifecycle.ExecuteIfBound(Stage, [&Detail]() { return Detail; });
}

void FRpgSwingTraceWindow::LogLifecycleLazy(const TCHAR* Stage, TFunctionRef<FString()> DetailBuilder) const
{
	Sink.OnLifecycle.ExecuteIfBound(Stage, DetailBuilder);
}

bool FRpgSwingTraceWindow::ScheduleAuthorityWindow()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetActorInfo();
	if (!ActorInfo || !IsAbilityActive())
	{
		return false;
	}
	if (!ActorInfo->IsNetAuthority())
	{
		return true;
	}

	ClearAuthoritySchedule();
	auto RejectSchedule = [this](const FString& Reason)
	{
		UE_LOG(
			LogRpgWeapons,
			Error,
			TEXT("SwingTraceWindow[%s] aborted accepted authority activation: %s"),
			*GetNameSafe(Ability),
			*Reason);
		LogLifecycle(TEXT("AuthorityScheduleRejected"), Reason);
		return false;
	};

	UWorld* World = GetWorld();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	UAnimMontage* ActiveMontage = Montage.Get();
	if (!World || !AnimInstance || !ActiveMontage || Ability->GetCurrentMontage() != ActiveMontage || !AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		return RejectSchedule(TEXT("The accepted server activation did not start the configured montage."));
	}

	float StartTime = 0.0f;
	float EndTime = 0.0f;
	FString FailureReason;
	const float EffectivePlayRate = AnimInstance->Montage_GetEffectivePlayRate(ActiveMontage);
	if (!ResolveWindowTiming(
			ActiveMontage,
			EffectivePlayRate,
			StartTime,
			EndTime,
			FailureReason))
	{
		return RejectSchedule(FailureReason);
	}

	const float MontagePosition = AnimInstance->Montage_GetPosition(ActiveMontage);
	if (!FMath::IsFinite(EffectivePlayRate) || EffectivePlayRate <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(MontagePosition) || MontagePosition >= EndTime)
	{
		return RejectSchedule(
			FString::Printf(
				TEXT("EffectivePlayRate=%.3f MontagePosition=%.3f WindowEnd=%.3f"),
				EffectivePlayRate,
				MontagePosition,
				EndTime));
	}

	WindowStartTime = StartTime;
	WindowEndTime = EndTime;
	const float StartDelay = FMath::Max(0.0f, (StartTime - MontagePosition) / EffectivePlayRate);
	const float EndDelay = FMath::Max(0.0f, (EndTime - MontagePosition) / EffectivePlayRate);
	bAuthorityScheduled = true;

	SetWindowTimer(AuthorityEndTimerHandle, &FRpgSwingTraceWindow::OnAuthorityScheduleEnded, EndDelay);

	// Timers tick after the movement and animation updates, so the poll reads each frame's final montage position.
	SetWindowTimer(
		AuthorityMontagePollTimerHandle,
		&FRpgSwingTraceWindow::OnAuthorityMontagePoll,
		UE_KINDA_SMALL_NUMBER,
		true,
		-1.0f,
		true);

	if (StartDelay <= UE_SMALL_NUMBER)
	{
		OnAuthorityScheduleStarted();
	}
	else
	{
		SetWindowTimer(AuthorityStartTimerHandle, &FRpgSwingTraceWindow::OnAuthorityScheduleStarted, StartDelay);
	}

	LogLifecycleLazy(
		TEXT("AuthorityScheduleCreated"),
		[StartDelay, EndDelay, EffectivePlayRate]()
		{
			return FString::Printf(
				TEXT("StartDelay=%.3f EndDelay=%.3f EffectivePlayRate=%.3f"),
				StartDelay,
				EndDelay,
				EffectivePlayRate);
		});
	return true;
}

void FRpgSwingTraceWindow::DelayAuthoritySchedule(const float Seconds)
{
	UWorld* World = GetWorld();
	if (!bAuthorityScheduled || !World || !IsAuthority() || !FMath::IsFinite(Seconds) || Seconds <= 0.0f)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	auto Delay = [this, &TimerManager, Seconds](FTimerHandle& Handle, void (FRpgSwingTraceWindow::*Callback)())
	{
		if (TimerManager.IsTimerActive(Handle))
		{
			SetWindowTimer(Handle, Callback, FMath::Max(0.0f, TimerManager.GetTimerRemaining(Handle)) + Seconds);
		}
	};
	Delay(AuthorityStartTimerHandle, &FRpgSwingTraceWindow::OnAuthorityScheduleStarted);
	Delay(AuthorityEndTimerHandle, &FRpgSwingTraceWindow::OnAuthorityScheduleEnded);
	LogLifecycleLazy(TEXT("AuthorityScheduleDelayed"), [Seconds]() { return FString::Printf(TEXT("Delay=%.3f"), Seconds); });
}

void FRpgSwingTraceWindow::ClearAuthoritySchedule()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthorityStartTimerHandle);
		World->GetTimerManager().ClearTimer(AuthorityEndTimerHandle);
		World->GetTimerManager().ClearTimer(AuthorityMontagePollTimerHandle);
	}
	bAuthorityScheduled = false;
}

void FRpgSwingTraceWindow::HandleStartNotify()
{
	if (!IsAbilityActive())
	{
		return;
	}

	// The schedule, set during the activation frame, counts only from that frame's end and fires one or two frames after
	// the authored start; the montage and the owner's notify do not wait. So authority opens on whichever of this notify,
	// its montage poll and the schedule comes first. A notify counts only while this swing's window is still scheduled
	// and its own montage has reached the start: a late queued notify never reopens a closed window, and a replaced
	// swing's notify never opens the next swing's.
	const bool bAuthority = IsAuthority();
	if (bAuthority && (!bAuthorityScheduled || !HasActiveMontageReached(WindowStartTime)))
	{
		LogLifecycle(TEXT("AttackWindowStart.NotifyIgnored"));
		return;
	}

	bReceivedStartNotify = true;
	LogLifecycle(TEXT("AttackWindowStart.Notify"));
	OpenWindow();
}

void FRpgSwingTraceWindow::HandleEndNotify()
{
	if (!IsAbilityActive())
	{
		return;
	}

	// Authority closes on whichever comes first as well, so its window keeps the authored length.
	const bool bAuthority = IsAuthority();
	if (bAuthority && !HasActiveMontageReached(WindowEndTime))
	{
		LogLifecycle(TEXT("AttackWindowEnd.NotifyIgnored"));
		return;
	}

	bReceivedEndNotify = true;
	LogLifecycle(TEXT("AttackWindowEnd.Notify"));
	// Closing clears the schedule. A window that is not open yet, because its start notify did not arrive, stays with
	// the schedule, which still opens and closes it.
	if (!bAuthority || bWindowOpen)
	{
		StopTracing(false);
	}
}

void FRpgSwingTraceWindow::OnAuthorityScheduleStarted()
{
	if (!IsAbilityActive() || !IsAuthority())
	{
		return;
	}

	// A start notify or the montage poll that opened the window already cleared this timer.
	bOpenedBySchedule = true;
	LogLifecycle(
		TEXT("AttackWindowStart.AuthoritySchedule"),
		TEXT("Neither the notify nor the montage had reached the start; authority fallback opened the window."));
	OpenWindow();
}

void FRpgSwingTraceWindow::OnAuthorityMontagePoll()
{
	if (!IsAbilityActive() || !bAuthorityScheduled)
	{
		return;
	}

	if (!bWindowOpen)
	{
		if (HasActiveMontageReached(WindowStartTime))
		{
			LogLifecycle(TEXT("AttackWindowStart.AuthorityMontage"));
			OpenWindow();
		}
		// A montage that passed the whole window this frame still gets one traced frame before it closes.
		return;
	}
	if (HasActiveMontageReached(WindowEndTime))
	{
		LogLifecycle(TEXT("AttackWindowEnd.AuthorityMontage"));
		StopTracing(false);
	}
}

void FRpgSwingTraceWindow::OnAuthorityScheduleEnded()
{
	if (!IsAbilityActive() || !IsAuthority())
	{
		return;
	}

	bClosedBySchedule = true;
	LogLifecycle(
		TEXT("AttackWindowEnd.AuthoritySchedule"),
		bReceivedEndNotify
			? TEXT("Notify already arrived; close is idempotent.")
			: TEXT("Notify had not arrived; authority fallback closed the window."));
	StopTracing(false);
}

bool FRpgSwingTraceWindow::HasActiveMontageReached(const float MontageTime) const
{
	const UAnimMontage* ActiveMontage = Montage.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetActorInfo();
	const UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	// Montage_GetPosition reads the active instance; a replaced or stopping instance of the same montage is not it.
	return ActiveMontage && AnimInstance && Ability->GetCurrentMontage() == ActiveMontage && AnimInstance->Montage_IsPlaying(ActiveMontage) &&
		AnimInstance->Montage_GetPosition(ActiveMontage) + UE_KINDA_SMALL_NUMBER >= MontageTime;
}

void FRpgSwingTraceWindow::OpenWindow()
{
	if (bWindowOpen)
	{
		return;
	}
	const ERpgSwingTraceRole Role = GetRole();
	if (Role == ERpgSwingTraceRole::Authority && bClosedBySchedule)
	{
		LogLifecycle(
			TEXT("AttackWindowStartIgnored"),
			TEXT("The authority window already reached its terminal close edge."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthorityStartTimerHandle);
	}

	bWindowOpen = true;
	PreviousSample.Reset();

	// Authority traces for gameplay; a remote owner traces the same window only for its hit feedback without a round trip.
	Sink.OnWindowOpened.ExecuteIfBound(Role);
	if (Role == ERpgSwingTraceRole::None)
	{
		return;
	}

	const bool bAuthority = Role == ERpgSwingTraceRole::Authority;
#if WITH_DEV_AUTOMATION_TESTS
	if (bAuthority)
	{
		++AuthorityWindowOpenCountForTests;
	}
#endif

	if (!GatherSample(PreviousSample) && bAuthority)
	{
		++AuthorityTracePointFailures;
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("SwingTraceWindow[%s] opened an attack window before valid trace points were available; authority will retry until the window closes."),
			*GetNameSafe(Ability));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SampleInterval = FMath::Max(0.001f, Params.TraceSampleInterval);
	SetWindowTimer(TraceSampleTimerHandle, &FRpgSwingTraceWindow::PerformSample, SampleInterval, true, SampleInterval);

	// Seed or sample immediately so short windows do not lose their first interval and a
	// transient socket/actor readiness miss is retried instead of disabling the whole swing.
	PerformSample();
	LogLifecycle(bAuthority ? TEXT("AttackWindowOpened.Authority") : TEXT("AttackWindowOpened.OwnerFeedback"));
}

void FRpgSwingTraceWindow::StopTracing(const bool bLogMissingEndNotify)
{
	const bool bWasOpen = bWindowOpen;
	ClearAuthoritySchedule();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TraceSampleTimerHandle);
	}

	const ERpgSwingTraceRole Role = GetRole();
	if (bWasOpen && Role == ERpgSwingTraceRole::OwnerFeedback)
	{
		PerformSample();
	}
	else if (bWasOpen && Role == ERpgSwingTraceRole::Authority)
	{
		const int32 TraceSamplesBeforeFinalSample = AuthorityTraceSamples;
		PerformSample();
		LogLifecycleLazy(
			TEXT("AuthorityFinalTrace"),
			[this, TraceSamplesBeforeFinalSample]()
			{
				return FString::Printf(
					TEXT("SampleAdvanced=%d TotalSamples=%d"),
					AuthorityTraceSamples > TraceSamplesBeforeFinalSample,
					AuthorityTraceSamples);
			});

#if WITH_DEV_AUTOMATION_TESTS
		++AuthorityWindowCloseCountForTests;
#endif
	}

	if (bWasOpen && bLogMissingEndNotify && !bReceivedEndNotify)
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("SwingTraceWindow[%s] montage [%s] ended while the attack window was open; closing defensively. Check for missing AttackWindowEnd notify."),
			*GetNameSafe(Ability),
			*GetNameSafe(Montage.Get()));
	}

	bWindowOpen = false;
	PreviousSample.Reset();
	if (bWasOpen)
	{
		Sink.OnWindowClosed.ExecuteIfBound(Role);
	}
}

bool FRpgSwingTraceWindow::GatherSample(RpgWeaponTraceInterpolation::FBladeSample& OutSample)
{
	OutSample.Reset();
	const AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!AvatarActor)
	{
		return false;
	}

	// The gameplay mesh carries the body pose the blade swings against, including a listen server's mesh smoothing.
	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(AvatarActor);
	OutSample.BodyTransform = Mesh ? Mesh->GetComponentTransform() : AvatarActor->GetActorTransform();
	OutSample.TracePointTransforms.Reserve(Params.TracePointSockets.Num());

	const URpgEquipmentInstance* EquipmentInstance = Equipment.Get();
	auto TryGetEquipmentTransform = [EquipmentInstance](const FName SocketName, FTransform& OutTransform)
	{
		if (!EquipmentInstance || SocketName.IsNone())
		{
			return false;
		}

		for (AActor* SpawnedActor : EquipmentInstance->GetSpawnedActors())
		{
			if (!SpawnedActor)
			{
				continue;
			}

			TInlineComponentArray<USceneComponent*> SceneComponents;
			SpawnedActor->GetComponents(SceneComponents);
			for (const USceneComponent* SceneComponent : SceneComponents)
			{
				if (SceneComponent && SceneComponent->GetFName() == SocketName)
				{
					OutTransform = SceneComponent->GetComponentTransform();
					return true;
				}

				if (SceneComponent && SceneComponent->DoesSocketExist(SocketName))
				{
					OutTransform = SceneComponent->GetSocketTransform(SocketName);
					return true;
				}
			}
		}

		return false;
	};
	auto TryGetAvatarTransform = [Mesh](const FName SocketName, FTransform& OutTransform)
	{
		if (SocketName.IsNone() || !Mesh || !Mesh->DoesSocketExist(SocketName))
		{
			return false;
		}

		OutTransform = Mesh->GetSocketTransform(SocketName);
		return true;
	};

	for (const FName SocketName : Params.TracePointSockets)
	{
		FTransform SocketTransform = FTransform::Identity;
		if (!TryGetEquipmentTransform(SocketName, SocketTransform) && !TryGetAvatarTransform(SocketName, SocketTransform))
		{
			if (!bLoggedTracePointFailure)
			{
				bLoggedTracePointFailure = true;
				UE_LOG(
					LogRpgWeapons,
					Warning,
					TEXT("SwingTraceWindow[%s] could not resolve trace point [%s] for [%s]; later authority samples will retry silently."),
					*GetNameSafe(Ability),
					*SocketName.ToString(),
					*GetNameSafe(EquipmentInstance));
			}
			OutSample.Reset();
			return false;
		}

		OutSample.TracePointTransforms.Add(SocketTransform);
	}

	Params.ExtendTip(OutSample.TracePointTransforms);
	return OutSample.TracePointTransforms.Num() >= 2;
}

void FRpgSwingTraceWindow::PerformSample()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!World || !AvatarActor || !bWindowOpen)
	{
		return;
	}

	const ERpgSwingTraceRole Role = GetRole();
	const bool bAuthority = Role == ERpgSwingTraceRole::Authority;
	RpgWeaponTraceInterpolation::FBladeSample CurrentSample;
	if (!GatherSample(CurrentSample))
	{
		if (bAuthority)
		{
			++AuthorityTracePointFailures;
		}
		return;
	}

	if (PreviousSample.TracePointTransforms.Num() != CurrentSample.TracePointTransforms.Num())
	{
		PreviousSample = MoveTemp(CurrentSample);
		return;
	}

	// A pose that advanced several frames since the last sample, such as a remote client's montage on the server, which
	// moves with every received move, is traced along its swing arc in sub-steps instead of one straight chord.
	const int32 SubstepCount = RpgWeaponTraceInterpolation::ComputeSubstepCount(PreviousSample, CurrentSample);
	TArray<TArray<FVector>> SubstepLocations;
	RpgWeaponTraceInterpolation::BuildSubstepLocations(PreviousSample, CurrentSample, SubstepCount, SubstepLocations);

	if (bAuthority)
	{
		++AuthorityTraceSamples;
#if WITH_DEV_AUTOMATION_TESTS
		++AuthorityTraceSampleCountForTests;
#endif
		AuthorityMaxTraceSubsteps = FMath::Max(AuthorityMaxTraceSubsteps, SubstepCount);
		if (AuthorityTraceSamples == 1)
		{
			LogLifecycle(TEXT("AuthorityTraceStarted"));
		}
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgSwingTraceWindow), false, AvatarActor);
	BuildTraceQueryParams(QueryParams);
	TraceSubsteps(*World, SubstepLocations, QueryParams, Role);

	if (bWindowOpen)
	{
		PreviousSample = MoveTemp(CurrentSample);
	}
}

void FRpgSwingTraceWindow::BuildTraceQueryParams(FCollisionQueryParams& QueryParams) const
{
	AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	QueryParams.AddIgnoredActor(AvatarActor);
	if (const URpgEquipmentInstance* EquipmentInstance = Equipment.Get())
	{
		for (AActor* SpawnedActor : EquipmentInstance->GetSpawnedActors())
		{
			if (SpawnedActor)
			{
				QueryParams.AddIgnoredActor(SpawnedActor);
			}
		}
	}
}

void FRpgSwingTraceWindow::TraceSubsteps(
	UWorld& World,
	const TArray<TArray<FVector>>& SubstepLocations,
	const FCollisionQueryParams& QueryParams,
	const ERpgSwingTraceRole Role)
{
	// A hit may end the ability and close the window; later sub-steps then neither trace nor seed a new sample.
	for (int32 SubstepIndex = 1; SubstepIndex < SubstepLocations.Num() && bWindowOpen; ++SubstepIndex)
	{
		const TArray<FVector>& SubstepStartLocations = SubstepLocations[SubstepIndex - 1];
		const TArray<FVector>& SubstepEndLocations = SubstepLocations[SubstepIndex];
		switch (Params.TraceMode)
		{
		case ERpgWeaponAttackTraceMode::LineTrace:
			PerformLineTraceRibbon(World, SubstepStartLocations, SubstepEndLocations, QueryParams, Role);
			break;
		case ERpgWeaponAttackTraceMode::SphereSweep:
		case ERpgWeaponAttackTraceMode::CapsuleSweep:
		case ERpgWeaponAttackTraceMode::BoxSweep:
			PerformSweepTraceRibbon(World, SubstepStartLocations, SubstepEndLocations, QueryParams, Role);
			break;
		default:
			break;
		}
	}
}

void FRpgSwingTraceWindow::BuildInterpolatedTracePointPairs(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	TArray<FVector>& OutPreviousTraceLocations,
	TArray<FVector>& OutCurrentTraceLocations) const
{
	OutPreviousTraceLocations.Reset();
	OutCurrentTraceLocations.Reset();

	if (PreviousSocketLocations.Num() != CurrentSocketLocations.Num() || PreviousSocketLocations.IsEmpty())
	{
		return;
	}

	if (!Params.bTraceBetweenSockets)
	{
		OutPreviousTraceLocations = PreviousSocketLocations;
		OutCurrentTraceLocations = CurrentSocketLocations;
		return;
	}

	const float InterpolationDistance = FMath::Max(1.0f, Params.TraceInterpolationDistance);
	OutPreviousTraceLocations.Add(PreviousSocketLocations[0]);
	OutCurrentTraceLocations.Add(CurrentSocketLocations[0]);

	for (int32 SocketIndex = 0; SocketIndex < PreviousSocketLocations.Num() - 1; ++SocketIndex)
	{
		const FVector& PreviousA = PreviousSocketLocations[SocketIndex];
		const FVector& PreviousB = PreviousSocketLocations[SocketIndex + 1];
		const FVector& CurrentA = CurrentSocketLocations[SocketIndex];
		const FVector& CurrentB = CurrentSocketLocations[SocketIndex + 1];

		const float PreviousSegmentLength = FVector::Distance(PreviousA, PreviousB);
		const float CurrentSegmentLength = FVector::Distance(CurrentA, CurrentB);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(FMath::Max(PreviousSegmentLength, CurrentSegmentLength) / InterpolationDistance));

		for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
		{
			const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
			OutPreviousTraceLocations.Add(FMath::Lerp(PreviousA, PreviousB, Alpha));
			OutCurrentTraceLocations.Add(FMath::Lerp(CurrentA, CurrentB, Alpha));
		}
	}
}

void FRpgSwingTraceWindow::PerformLineTraceRibbon(
	UWorld& World,
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams,
	const ERpgSwingTraceRole Role)
{
	if (PreviousSocketLocations.Num() != CurrentSocketLocations.Num() || PreviousSocketLocations.Num() < 2)
	{
		return;
	}

	TArray<FVector> PreviousTraceLocations;
	TArray<FVector> CurrentTraceLocations;
	BuildInterpolatedTracePointPairs(PreviousSocketLocations, CurrentSocketLocations, PreviousTraceLocations, CurrentTraceLocations);

	for (int32 TracePointIndex = 0; TracePointIndex < PreviousTraceLocations.Num(); ++TracePointIndex)
	{
		TraceLine(World, PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams, Role);
	}

	if (!Params.bTraceBetweenSockets)
	{
		return;
	}

	for (int32 SocketIndex = 0; SocketIndex < PreviousSocketLocations.Num() - 1; ++SocketIndex)
	{
		const FVector& PreviousA = PreviousSocketLocations[SocketIndex];
		const FVector& PreviousB = PreviousSocketLocations[SocketIndex + 1];
		const FVector& CurrentA = CurrentSocketLocations[SocketIndex];
		const FVector& CurrentB = CurrentSocketLocations[SocketIndex + 1];

		TraceLine(World, PreviousA, PreviousB, QueryParams, Role);
		TraceLine(World, CurrentA, CurrentB, QueryParams, Role);

		// Diagonals approximate the swept quad between neighboring sockets without needing mesh collision.
		TraceLine(World, PreviousA, CurrentB, QueryParams, Role);
		TraceLine(World, PreviousB, CurrentA, QueryParams, Role);
	}
}

void FRpgSwingTraceWindow::PerformSweepTraceRibbon(
	UWorld& World,
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams,
	const ERpgSwingTraceRole Role)
{
	TArray<FVector> PreviousTraceLocations;
	TArray<FVector> CurrentTraceLocations;
	BuildInterpolatedTracePointPairs(PreviousSocketLocations, CurrentSocketLocations, PreviousTraceLocations, CurrentTraceLocations);

	for (int32 TracePointIndex = 0; TracePointIndex < PreviousTraceLocations.Num(); ++TracePointIndex)
	{
		TraceSweep(World, PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams, Role);
	}
}

void FRpgSwingTraceWindow::TraceLine(
	UWorld& World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams,
	const ERpgSwingTraceRole Role)
{
	TArray<FHitResult> HitResults;
	World.LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, Params.TraceChannel, QueryParams);
	Sink.OnTraceQuery.ExecuteIfBound(TraceStart, TraceEnd, ERpgRecordedTraceShape::Line, FVector::ZeroVector);

#if ENABLE_DRAW_DEBUG
	DrawSwingTraceLine(&World, TraceStart, TraceEnd, HitResults);
#endif

	ReportHits(HitResults, Role);
}

void FRpgSwingTraceWindow::TraceSweep(
	UWorld& World,
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams,
	const ERpgSwingTraceRole Role)
{
	FCollisionShape Shape;
	switch (Params.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::SphereSweep:
		Shape = FCollisionShape::MakeSphere(FMath::Max(1.0f, Params.TraceRadius));
		break;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		Shape = FCollisionShape::MakeCapsule(
			FMath::Max(1.0f, Params.TraceRadius),
			FMath::Max(Params.TraceRadius, Params.TraceCapsuleHalfHeight));
		break;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		Shape = FCollisionShape::MakeBox(FVector(
			FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.X)),
			FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.Y)),
			FMath::Max(1.0f, FMath::Abs(Params.TraceBoxExtent.Z))));
		break;
	default:
		return;
	}

	TArray<FHitResult> HitResults;
	World.SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, Params.TraceChannel, Shape, QueryParams);
	switch (Params.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::SphereSweep:
		Sink.OnTraceQuery.ExecuteIfBound(TraceStart, TraceEnd, ERpgRecordedTraceShape::Sphere, FVector(Shape.GetSphereRadius(), 0.0, 0.0));
		break;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		Sink.OnTraceQuery.ExecuteIfBound(TraceStart, TraceEnd, ERpgRecordedTraceShape::Capsule,
			FVector(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight(), 0.0));
		break;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		Sink.OnTraceQuery.ExecuteIfBound(TraceStart, TraceEnd, ERpgRecordedTraceShape::Box, Shape.GetBox());
		break;
	default:
		break;
	}

#if ENABLE_DRAW_DEBUG
	DrawSwingTraceSweep(&World, TraceStart, TraceEnd, Params, HitResults);
#endif

	ReportHits(HitResults, Role);
}

void FRpgSwingTraceWindow::ReportHits(const TArray<FHitResult>& HitResults, const ERpgSwingTraceRole Role) const
{
	if (!HitResults.IsEmpty())
	{
		Sink.OnHits.ExecuteIfBound(HitResults, Role);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FRpgSwingTraceWindow::TraceSampleMotionForTests(
	UWorld& World,
	const FRpgSwingTraceParams& InParams,
	const RpgWeaponTraceInterpolation::FBladeSample& Previous,
	const RpgWeaponTraceInterpolation::FBladeSample& Current,
	FRpgSwingTraceSink InSink)
{
	Params = InParams;
	Sink = MoveTemp(InSink);
	// The samples hold the authored trace points, as GatherSample reads them, before the tip extension.
	RpgWeaponTraceInterpolation::FBladeSample ExtendedPrevious = Previous;
	RpgWeaponTraceInterpolation::FBladeSample ExtendedCurrent = Current;
	Params.ExtendTip(ExtendedPrevious.TracePointTransforms);
	Params.ExtendTip(ExtendedCurrent.TracePointTransforms);
	const int32 SubstepCount = RpgWeaponTraceInterpolation::ComputeSubstepCount(ExtendedPrevious, ExtendedCurrent);
	TArray<TArray<FVector>> SubstepLocations;
	RpgWeaponTraceInterpolation::BuildSubstepLocations(ExtendedPrevious, ExtendedCurrent, SubstepCount, SubstepLocations);

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgSwingTraceWindow), false);
	TGuardValue<bool> OpenGuard(bWindowOpen, true);
	TraceSubsteps(World, SubstepLocations, QueryParams, ERpgSwingTraceRole::Authority);
	return SubstepCount;
}
#endif
