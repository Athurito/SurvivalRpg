#include "RpgGameplayAbility_BasicWeaponAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/AnimNotifyState_RpgComboWindow.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgWeaponAttackWindow.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Combat/RpgCombatDeveloperSettings.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
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

#if ENABLE_DRAW_DEBUG
namespace
{
	float GetWeaponAttackTraceDebugDuration()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings ? FMath::Max(0.0f, Settings->WeaponAttackTraceDebugDuration) : 0.0f;
	}

	bool ShouldDrawWeaponAttackTraces()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings && Settings->bDrawWeaponAttackTraces;
	}

	FColor GetWeaponAttackTraceColor(const TArray<FHitResult>& HitResults)
	{
		return HitResults.IsEmpty() ? FColor::Green : FColor::Red;
	}

	void DrawWeaponAttackTraceLine(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawWeaponAttackTraces())
		{
			return;
		}

		const float Duration = GetWeaponAttackTraceDebugDuration();
		const FColor TraceColor = GetWeaponAttackTraceColor(HitResults);

		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.5f);
		DrawDebugPoint(World, TraceStart, 8.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 8.0f, FColor::Cyan, false, Duration);

		for (const FHitResult& HitResult : HitResults)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 18.0f, FColor::Yellow, false, Duration);
		}
	}

	void DrawWeaponAttackTraceSweep(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FRpgWeaponAttackDefinition& AttackDefinition,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawWeaponAttackTraces())
		{
			return;
		}

		const float Duration = GetWeaponAttackTraceDebugDuration();
		const FVector TraceDelta = TraceEnd - TraceStart;
		const FColor TraceColor = GetWeaponAttackTraceColor(HitResults);

		DrawDebugPoint(World, TraceStart, 12.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 12.0f, FColor::Cyan, false, Duration);
		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.0f);

		switch (AttackDefinition.TraceMode)
		{
		case ERpgWeaponAttackTraceMode::SphereSweep:
		{
			const float Radius = FMath::Max(1.0f, AttackDefinition.TraceRadius);
			const FVector TraceCenter = (TraceStart + TraceEnd) * 0.5f;
			const FQuat TraceRotation = TraceDelta.IsNearlyZero()
				? FQuat::Identity
				: FRotationMatrix::MakeFromZ(TraceDelta).ToQuat();
			DrawDebugCapsule(World, TraceCenter, Radius + (TraceDelta.Size() * 0.5f), Radius, TraceRotation, TraceColor, false, Duration, 0, 1.5f);
			break;
		}
		case ERpgWeaponAttackTraceMode::CapsuleSweep:
		{
			const float Radius = FMath::Max(1.0f, AttackDefinition.TraceRadius);
			const float HalfHeight = FMath::Max(Radius, AttackDefinition.TraceCapsuleHalfHeight);
			DrawDebugCapsule(World, TraceStart, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			DrawDebugCapsule(World, TraceEnd, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			break;
		}
		case ERpgWeaponAttackTraceMode::BoxSweep:
		{
			const FVector Extent(
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.X)),
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.Y)),
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.Z)));
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
	return TimerManager.TimerExists(TraceSampleTimerHandle) ||
		TimerManager.TimerExists(AuthorityAttackWindowStartTimerHandle) ||
		TimerManager.TimerExists(AuthorityAttackWindowEndTimerHandle) ||
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
		bAttackWindowOpen ||
		bAuthorityAttackWindowScheduled ||
		!PreviousTracePointLocations.IsEmpty() ||
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
	bAttackWindowOpen = false;
	bReceivedAttackWindowStart = false;
	bReceivedAttackWindowEnd = false;
	bAuthorityAttackWindowScheduled = false;
	bAuthorityWindowOpenedBySchedule = false;
	bAuthorityWindowClosedBySchedule = false;
	AuthorityTraceSamplesThisActivation = 0;
	AuthorityTracePointFailuresThisActivation = 0;
	AuthorityDamageHitsThisActivation = 0;
	AuthorityDuplicateHitsSkippedThisActivation = 0;
	bLoggedTracePointFailureThisActivation = false;
	PreviousTracePointLocations.Reset();
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
	if (!ResolveAttackWindowTiming(
			OutStep,
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
	if (!ScheduleAuthorityAttackWindow())
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
	CloseAttackWindow(false);
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
	bReceivedAttackWindowStart = false;
	bReceivedAttackWindowEnd = false;
	bAuthorityWindowOpenedBySchedule = false;
	bAuthorityWindowClosedBySchedule = false;
	HitActorsThisWindow.Reset();
	PreviousTracePointLocations.Reset();

	ActiveAttackDefinition = MoveTemp(NextStep);
	ActiveAttackTag = NextAttackTag;
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

	CloseAttackWindow(false);
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
	const UWorld* World = GetWorld();
	const FTimerManager* TimerManager = World ? &World->GetTimerManager() : nullptr;
	const bool bHasPendingTimers = TimerManager &&
		(TimerManager->TimerExists(TraceSampleTimerHandle) ||
		 TimerManager->TimerExists(AuthorityAttackWindowStartTimerHandle) ||
		 TimerManager->TimerExists(AuthorityAttackWindowEndTimerHandle));
	LogAttackLifecycleLazy(
		TEXT("AbilityEnded"),
		[this, bWasCancelled, bHasPendingTimers]()
		{
			return FString::Printf(
				TEXT("Cancelled=%d NotifyStart=%d NotifyEnd=%d ScheduledOpen=%d ScheduledClose=%d TraceSamples=%d TracePointFailures=%d DamageHits=%d DuplicateHitsSkipped=%d PendingTimers=%d"),
				bWasCancelled,
				bReceivedAttackWindowStart,
				bReceivedAttackWindowEnd,
				bAuthorityWindowOpenedBySchedule,
				bAuthorityWindowClosedBySchedule,
				AuthorityTraceSamplesThisActivation,
				AuthorityTracePointFailuresThisActivation,
				AuthorityDamageHitsThisActivation,
				AuthorityDuplicateHitsSkippedThisActivation,
				bHasPendingTimers);
		});

	ActiveWeaponInstance = nullptr;
	ActiveMontageTask = nullptr;
	ActiveAttackDefinition = FRpgWeaponAttackDefinition();
	ActiveAttackTag = FGameplayTag();
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
	bAttackWindowOpen = false;
	bReceivedAttackWindowStart = false;
	bReceivedAttackWindowEnd = false;
	bAuthorityAttackWindowScheduled = false;
	bAuthorityWindowOpenedBySchedule = false;
	bAuthorityWindowClosedBySchedule = false;
	AuthorityTraceSamplesThisActivation = 0;
	AuthorityTracePointFailuresThisActivation = 0;
	AuthorityDamageHitsThisActivation = 0;
	AuthorityDuplicateHitsSkippedThisActivation = 0;
	bLoggedTracePointFailureThisActivation = false;
	PreviousTracePointLocations.Reset();
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

	bReceivedAttackWindowStart = true;
	LogAttackLifecycle(TEXT("AttackWindowStart.Notify"));
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		// Authority owns the deterministic timer schedule. The montage notify is telemetry only
		// so a delayed queued notify can never reopen a window that authority already closed.
		return;
	}
	OpenAttackWindow();
}

void URpgGameplayAbility_BasicWeaponAttack::OnAttackWindowEnded(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	bReceivedAttackWindowEnd = true;
	LogAttackLifecycle(TEXT("AttackWindowEnd.Notify"));
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		return;
	}
	CloseAttackWindow(false);
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

	if (bAttackWindowOpen)
	{
		CloseAttackWindow(!bWasCancelled);
	}
	else if (!bWasCancelled && !bReceivedAttackWindowStart)
	{
		if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
		{
			if (bAuthorityWindowOpenedBySchedule)
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

void URpgGameplayAbility_BasicWeaponAttack::OnAuthorityAttackWindowStarted()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	bAuthorityWindowOpenedBySchedule = true;
	LogAttackLifecycle(
		TEXT("AttackWindowStart.AuthoritySchedule"),
		bReceivedAttackWindowStart
			? TEXT("Notify already arrived; start is idempotent.")
			: TEXT("Notify had not arrived; authority fallback opened the window."));
	OpenAttackWindow();
}

void URpgGameplayAbility_BasicWeaponAttack::OnAuthorityAttackWindowEnded()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	bAuthorityWindowClosedBySchedule = true;
	LogAttackLifecycle(
		TEXT("AttackWindowEnd.AuthoritySchedule"),
		bReceivedAttackWindowEnd
			? TEXT("Notify already arrived; close is idempotent.")
			: TEXT("Notify had not arrived; authority fallback closed the window."));
	CloseAttackWindow(false);
}

bool URpgGameplayAbility_BasicWeaponAttack::IsAttackWindowEndBeforeAutoBlendOut(
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

#if WITH_DEV_AUTOMATION_TESTS
bool URpgGameplayAbility_BasicWeaponAttack::IsAttackWindowEndBeforeAutoBlendOutForTests(
	const float MontageLength,
	const float WindowEndTime,
	const float EffectivePlayRate,
	const float AuthoredBlendOutTriggerTime)
{
	float RemainingPlayTime = 0.0f;
	float BlendOutTriggerSeconds = 0.0f;
	return IsAttackWindowEndBeforeAutoBlendOut(
		MontageLength,
		WindowEndTime,
		EffectivePlayRate,
		AuthoredBlendOutTriggerTime,
		RemainingPlayTime,
		BlendOutTriggerSeconds);
}
#endif

bool URpgGameplayAbility_BasicWeaponAttack::ResolveAttackWindowTiming(
	const FRpgWeaponAttackDefinition& AttackDefinition,
	const float EffectivePlayRate,
	float& OutStartTime,
	float& OutEndTime,
	FString& OutFailureReason) const
{
	OutStartTime = 0.0f;
	OutEndTime = 0.0f;
	OutFailureReason.Reset();

	const UAnimMontage* Montage = AttackDefinition.Montage;
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
		if (!IsAttackWindowEndBeforeAutoBlendOut(
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

bool URpgGameplayAbility_BasicWeaponAttack::ScheduleAuthorityAttackWindow()
{
	if (!CurrentActorInfo || !IsActive())
	{
		return false;
	}
	if (!CurrentActorInfo->IsNetAuthority())
	{
		return true;
	}

	ClearAuthorityAttackWindowSchedule();
	auto RejectSchedule = [this](const FString& Reason)
	{
		UE_LOG(
			LogRpgWeapons,
			Error,
			TEXT("BasicWeaponAttack[%s] aborted accepted authority activation: %s"),
			*GetNameSafe(this),
			*Reason);
		LogAttackLifecycle(TEXT("AuthorityScheduleRejected"), Reason);
		return false;
	};

	UWorld* World = GetWorld();
	UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance();
	UAnimMontage* Montage = ActiveAttackDefinition.Montage;
	if (!World || !AnimInstance || !Montage || GetCurrentMontage() != Montage || !AnimInstance->Montage_IsPlaying(Montage))
	{
		return RejectSchedule(TEXT("The accepted server activation did not start the configured montage."));
	}

	float StartTime = 0.0f;
	float EndTime = 0.0f;
	FString FailureReason;
	const float EffectivePlayRate = AnimInstance->Montage_GetEffectivePlayRate(Montage);
	if (!ResolveAttackWindowTiming(
			ActiveAttackDefinition,
			EffectivePlayRate,
			StartTime,
			EndTime,
			FailureReason))
	{
		return RejectSchedule(FailureReason);
	}

	const float MontagePosition = AnimInstance->Montage_GetPosition(Montage);
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

	const float StartDelay = FMath::Max(0.0f, (StartTime - MontagePosition) / EffectivePlayRate);
	const float EndDelay = FMath::Max(0.0f, (EndTime - MontagePosition) / EffectivePlayRate);
	bAuthorityAttackWindowScheduled = true;

	World->GetTimerManager().SetTimer(
		AuthorityAttackWindowEndTimerHandle,
		this,
		&ThisClass::OnAuthorityAttackWindowEnded,
		EndDelay,
		false);

	if (StartDelay <= UE_SMALL_NUMBER)
	{
		OnAuthorityAttackWindowStarted();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			AuthorityAttackWindowStartTimerHandle,
			this,
			&ThisClass::OnAuthorityAttackWindowStarted,
			StartDelay,
			false);
	}

	LogAttackLifecycleLazy(
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

void URpgGameplayAbility_BasicWeaponAttack::ClearAuthorityAttackWindowSchedule()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthorityAttackWindowStartTimerHandle);
		World->GetTimerManager().ClearTimer(AuthorityAttackWindowEndTimerHandle);
	}
	bAuthorityAttackWindowScheduled = false;
}

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

bool URpgGameplayAbility_BasicWeaponAttack::TryGetSocketLocationFromWeapon(const URpgWeaponInstance* WeaponInstance, FName SocketName, FVector& OutLocation) const
{
	if (!WeaponInstance || SocketName.IsNone())
	{
		return false;
	}

	for (AActor* SpawnedActor : WeaponInstance->GetSpawnedActors())
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
				OutLocation = SceneComponent->GetComponentLocation();
				return true;
			}

			if (SceneComponent && SceneComponent->DoesSocketExist(SocketName))
			{
				OutLocation = SceneComponent->GetSocketLocation(SocketName);
				return true;
			}
		}
	}

	return false;
}

bool URpgGameplayAbility_BasicWeaponAttack::TryGetSocketLocationFromAvatar(FName SocketName, FVector& OutLocation) const
{
	if (SocketName.IsNone())
	{
		return false;
	}

	const USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(GetAvatarActorFromActorInfo());
	if (Mesh && Mesh->DoesSocketExist(SocketName))
	{
		OutLocation = Mesh->GetSocketLocation(SocketName);
		return true;
	}

	return false;
}

bool URpgGameplayAbility_BasicWeaponAttack::GatherTracePointLocations(TArray<FVector>& OutLocations)
{
	OutLocations.Reset();
	OutLocations.Reserve(ActiveAttackDefinition.TracePointSockets.Num());

	for (const FName SocketName : ActiveAttackDefinition.TracePointSockets)
	{
		FVector SocketLocation = FVector::ZeroVector;
		if (!TryGetSocketLocationFromWeapon(ActiveWeaponInstance, SocketName, SocketLocation) &&
			!TryGetSocketLocationFromAvatar(SocketName, SocketLocation))
		{
			if (!bLoggedTracePointFailureThisActivation)
			{
				bLoggedTracePointFailureThisActivation = true;
				UE_LOG(
					LogRpgWeapons,
					Warning,
					TEXT("BasicWeaponAttack[%s] could not resolve weapon trace point [%s] for [%s]; later authority samples will retry silently."),
					*GetNameSafe(this),
					*SocketName.ToString(),
					*GetNameSafe(ActiveWeaponInstance));
			}
			return false;
		}

		OutLocations.Add(SocketLocation);
	}

	return OutLocations.Num() >= 2;
}

void URpgGameplayAbility_BasicWeaponAttack::BuildInterpolatedTracePointPairs(
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

	if (!ActiveAttackDefinition.bTraceBetweenSockets)
	{
		OutPreviousTraceLocations = PreviousSocketLocations;
		OutCurrentTraceLocations = CurrentSocketLocations;
		return;
	}

	const float InterpolationDistance = FMath::Max(1.0f, ActiveAttackDefinition.TraceInterpolationDistance);
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

void URpgGameplayAbility_BasicWeaponAttack::OpenAttackWindow()
{
	// Owner (notify) and authority (schedule) each hold their own facing from the swing until the attack ends.
	HoldFacingCommitment();
	if (bAttackWindowOpen)
	{
		return;
	}
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && bAuthorityWindowClosedBySchedule)
	{
		LogAttackLifecycle(
			TEXT("AttackWindowStartIgnored"),
			TEXT("The authority window already reached its terminal close edge."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthorityAttackWindowStartTimerHandle);
	}

	bAttackWindowOpen = true;
	HitActorsThisWindow.Reset();
	PreviousTracePointLocations.Reset();

	// Authority traces for damage; a remote owner traces the same window only to play its hit feedback without a round trip.
	const bool bAuthority = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	BeginSwingRecord(bAuthority);
	if (!bAuthority && !ShouldPredictImpactFeedback())
	{
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bAuthority)
	{
		++AuthorityWindowOpenCountForTests;
	}
#endif

	if (!GatherTracePointLocations(PreviousTracePointLocations) && bAuthority)
	{
		++AuthorityTracePointFailuresThisActivation;
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] opened an attack window before valid trace points were available; authority will retry until the window closes."),
			*GetNameSafe(this));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SampleInterval = FMath::Max(0.001f, ActiveAttackDefinition.TraceSampleInterval);
	World->GetTimerManager().SetTimer(
		TraceSampleTimerHandle,
		this,
		&ThisClass::PerformBladeTraceSample,
		SampleInterval,
		true,
		SampleInterval);

	// Seed or sample immediately so short windows do not lose their first interval and a
	// transient socket/actor readiness miss is retried instead of disabling the whole attack.
	PerformBladeTraceSample();
	LogAttackLifecycle(bAuthority ? TEXT("AttackWindowOpened.Authority") : TEXT("AttackWindowOpened.OwnerFeedback"));
}

void URpgGameplayAbility_BasicWeaponAttack::CloseAttackWindow(bool bLogMissingEndNotify)
{
	const bool bWasOpen = bAttackWindowOpen;
	ClearAuthorityAttackWindowSchedule();

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TraceSampleTimerHandle);
	}

	if (bWasOpen && ShouldPredictImpactFeedback())
	{
		PerformBladeTraceSample();
	}
	else if (bWasOpen && CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		const int32 TraceSamplesBeforeFinalSample = AuthorityTraceSamplesThisActivation;
		PerformBladeTraceSample();
		LogAttackLifecycleLazy(
			TEXT("AuthorityFinalTrace"),
			[this, TraceSamplesBeforeFinalSample]()
			{
				return FString::Printf(
					TEXT("SampleAdvanced=%d TotalSamples=%d"),
					AuthorityTraceSamplesThisActivation > TraceSamplesBeforeFinalSample,
					AuthorityTraceSamplesThisActivation);
			});

#if WITH_DEV_AUTOMATION_TESTS
		++AuthorityWindowCloseCountForTests;
#endif
	}

	if (bWasOpen && bLogMissingEndNotify && !bReceivedAttackWindowEnd)
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] montage [%s] ended while the attack window was open; closing defensively. Check for missing AttackWindowEnd notify."),
			*GetNameSafe(this),
			*GetNameSafe(ActiveAttackDefinition.Montage));
	}

	bAttackWindowOpen = false;
	PreviousTracePointLocations.Reset();
	FinishSwingRecord();

	if (bWasOpen)
	{
		LogAttackLifecycleLazy(
			TEXT("AttackWindowClosed"),
			[this]()
			{
				return FString::Printf(
					TEXT("TraceSamples=%d TracePointFailures=%d DamageHits=%d DuplicateHitsSkipped=%d"),
					AuthorityTraceSamplesThisActivation,
					AuthorityTracePointFailuresThisActivation,
					AuthorityDamageHitsThisActivation,
					AuthorityDuplicateHitsSkippedThisActivation);
			});
	}
}

void URpgGameplayAbility_BasicWeaponAttack::BuildTraceQueryParams(FCollisionQueryParams& QueryParams) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	QueryParams.AddIgnoredActor(AvatarActor);
	if (ActiveWeaponInstance)
	{
		for (AActor* SpawnedActor : ActiveWeaponInstance->GetSpawnedActors())
		{
			if (SpawnedActor)
			{
				QueryParams.AddIgnoredActor(SpawnedActor);
			}
		}
	}
}

void URpgGameplayAbility_BasicWeaponAttack::PerformBladeTraceSample()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!World || !AvatarActor || !bAttackWindowOpen || !ActiveAttackDefinition.CanApplyDamage())
	{
		return;
	}

	const bool bAuthority = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	TArray<FVector> CurrentTracePointLocations;
	if (!GatherTracePointLocations(CurrentTracePointLocations))
	{
		if (bAuthority)
		{
			++AuthorityTracePointFailuresThisActivation;
		}
		return;
	}

	if (PreviousTracePointLocations.Num() != CurrentTracePointLocations.Num())
	{
		PreviousTracePointLocations = CurrentTracePointLocations;
		return;
	}

	if (bAuthority)
	{
		++AuthorityTraceSamplesThisActivation;
#if WITH_DEV_AUTOMATION_TESTS
		++AuthorityTraceSampleCountForTests;
#endif
		if (AuthorityTraceSamplesThisActivation == 1)
		{
			LogAttackLifecycle(TEXT("AuthorityTraceStarted"));
		}
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgBasicWeaponAttack), false, AvatarActor);
	BuildTraceQueryParams(QueryParams);

	switch (ActiveAttackDefinition.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::LineTrace:
		PerformLineTraceRibbon(PreviousTracePointLocations, CurrentTracePointLocations, QueryParams);
		break;
	case ERpgWeaponAttackTraceMode::SphereSweep:
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
	case ERpgWeaponAttackTraceMode::BoxSweep:
		PerformSweepTraceRibbon(PreviousTracePointLocations, CurrentTracePointLocations, QueryParams);
		break;
	default:
		break;
	}

	PreviousTracePointLocations = CurrentTracePointLocations;
}

void URpgGameplayAbility_BasicWeaponAttack::PerformLineTraceRibbon(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams)
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
		TraceDamageLine(PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams);
	}

	if (!ActiveAttackDefinition.bTraceBetweenSockets)
	{
		return;
	}

	for (int32 SocketIndex = 0; SocketIndex < PreviousSocketLocations.Num() - 1; ++SocketIndex)
	{
		const FVector& PreviousA = PreviousSocketLocations[SocketIndex];
		const FVector& PreviousB = PreviousSocketLocations[SocketIndex + 1];
		const FVector& CurrentA = CurrentSocketLocations[SocketIndex];
		const FVector& CurrentB = CurrentSocketLocations[SocketIndex + 1];

		TraceDamageLine(PreviousA, PreviousB, QueryParams);
		TraceDamageLine(CurrentA, CurrentB, QueryParams);

		// Diagonals approximate the swept quad between neighboring sockets without needing mesh collision.
		TraceDamageLine(PreviousA, CurrentB, QueryParams);
		TraceDamageLine(PreviousB, CurrentA, QueryParams);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::PerformSweepTraceRibbon(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams)
{
	TArray<FVector> PreviousTraceLocations;
	TArray<FVector> CurrentTraceLocations;
	BuildInterpolatedTracePointPairs(PreviousSocketLocations, CurrentSocketLocations, PreviousTraceLocations, CurrentTraceLocations);

	for (int32 TracePointIndex = 0; TracePointIndex < PreviousTraceLocations.Num(); ++TracePointIndex)
	{
		TraceDamageSweep(PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::TraceDamageLine(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, Rpg_TraceChannel_Weapon, QueryParams);
	RecordSwingTrace(TraceStart, TraceEnd, ERpgRecordedTraceShape::Line, FVector::ZeroVector);

#if ENABLE_DRAW_DEBUG
	DrawWeaponAttackTraceLine(World, TraceStart, TraceEnd, HitResults);
#endif

	HandleTraceHitResults(HitResults);
}

void URpgGameplayAbility_BasicWeaponAttack::TraceDamageSweep(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionShape Shape;
	switch (ActiveAttackDefinition.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::SphereSweep:
		Shape = FCollisionShape::MakeSphere(FMath::Max(1.0f, ActiveAttackDefinition.TraceRadius));
		break;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		Shape = FCollisionShape::MakeCapsule(
			FMath::Max(1.0f, ActiveAttackDefinition.TraceRadius),
			FMath::Max(ActiveAttackDefinition.TraceRadius, ActiveAttackDefinition.TraceCapsuleHalfHeight));
		break;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		Shape = FCollisionShape::MakeBox(FVector(
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.X)),
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.Y)),
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.Z))));
		break;
	default:
		return;
	}

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, Rpg_TraceChannel_Weapon, Shape, QueryParams);
	switch (ActiveAttackDefinition.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::SphereSweep:
		RecordSwingTrace(TraceStart, TraceEnd, ERpgRecordedTraceShape::Sphere, FVector(Shape.GetSphereRadius(), 0.0, 0.0));
		break;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		RecordSwingTrace(TraceStart, TraceEnd, ERpgRecordedTraceShape::Capsule,
			FVector(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight(), 0.0));
		break;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		RecordSwingTrace(TraceStart, TraceEnd, ERpgRecordedTraceShape::Box, Shape.GetBox());
		break;
	default:
		break;
	}

#if ENABLE_DRAW_DEBUG
	DrawWeaponAttackTraceSweep(World, TraceStart, TraceEnd, ActiveAttackDefinition, HitResults);
#endif

	HandleTraceHitResults(HitResults);
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

void URpgGameplayAbility_BasicWeaponAttack::HandleTraceHitResults(const TArray<FHitResult>& HitResults)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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
			if (AuthorityDuplicateHitsSkippedThisActivation == 1 && CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
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
		if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
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
		float Damage = ActiveAttackDefinition.Damage;
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

bool URpgGameplayAbility_BasicWeaponAttack::ShouldPredictImpactFeedback() const
{
	return CurrentActorInfo && !CurrentActorInfo->IsNetAuthority() && CurrentActorInfo->IsLocallyControlled();
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
