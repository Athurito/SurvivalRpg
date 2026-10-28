#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestBurn.h"
#include "Harvesting/RpgHarvestChainComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgGameplayEvent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Harvest)

namespace
{
	/**
	 * Most target cues one commit or stride pulse executes. Each is one unreliable multicast, and net.MaxRPCPerNetUpdate
	 * (10 in DefaultEngine.ini) drops what exceeds it per net update, so the success cue keeps room.
	 */
	constexpr int32 MaxTargetCuesPerCommit = 8;

	/** Bounds of the resource Hit addresses as currently presented: the struck instance for instanced resources. */
	FBox GetPresentedTargetBounds(const FHitResult& Hit)
	{
		const UPrimitiveComponent* Component = Hit.GetComponent();
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			FTransform InstanceTransform;
			if (Instances->GetStaticMesh() && Instances->GetInstanceTransform(Hit.Item, InstanceTransform, true))
			{
				return Instances->GetStaticMesh()->GetBounds().GetBox().TransformBy(InstanceTransform);
			}
			return FBox(Hit.ImpactPoint, Hit.ImpactPoint);
		}
		return Component ? Component->Bounds.GetBox() : FBox(Hit.ImpactPoint, Hit.ImpactPoint);
	}
}

URpgGameplayAbility_Harvest::URpgGameplayAbility_Harvest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;
	CommitEventTag = RpgHarvestingMagicGameplayTags::GameplayEvent_Harvesting_Commit;

	const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"), false);
	if (DeathTag.IsValid())
	{
		ActivationBlockedTags.AddTag(DeathTag);
	}
}

URpgGameplayAbility_Harvest::~URpgGameplayAbility_Harvest()
{
	// EndAbility delivers a stride's rewards; an instance destroyed without it only takes them along with its world.
	if (StrideRewardBatch)
	{
		StrideRewardBatch->Discard();
	}
}

bool URpgGameplayAbility_Harvest::CanActivateAbility(
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

	// Input-bound tool abilities only react while their equipment holds the matching hand role.
	const FGameplayTag InputTag = GetInputTagFromSpec(Handle, ActorInfo);
	if (InputTag.IsValid())
	{
		const UAbilitySystemComponent* AbilitySystem = ActorInfo->AbilitySystemComponent.Get();
		const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(Handle) : nullptr;
		const URpgEquipmentInstance* Equipment = Spec ? Cast<URpgEquipmentInstance>(Spec->SourceObject.Get()) : nullptr;
		if (!IsEquipmentActiveForInput(Equipment, InputTag))
		{
			return false;
		}
	}

	if (!MeetsSkillRequirement(*ActorInfo))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RpgHarvestingMagicGameplayTags::Ability_ActivateFail_Harvesting_SkillLevel);
		}
		return false;
	}
	return true;
}

void URpgGameplayAbility_Harvest::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bCommitPending = false;

	if (bAimWhileInputHeld)
	{
		SetLocalAimPreview(true);
		if (AimCameraMode && IsLocallyControlled())
		{
			SetCameraMode(AimCameraMode);
		}
		UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleAimInputReleased);
		ReleaseTask->ReadyForActivation();
		return;
	}

	BeginHarvestExecution();
}

void URpgGameplayAbility_Harvest::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// A stride that ends early, for example through a tool switch or death, still delivers what it harvested.
	FinishStride();

	// A commit that has not happened yet never happens after the activation ends.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CommitTimerHandle);
	}
	bCommitPending = false;
	bHasCommitView = false;
	bHasCommitValues = false;
	SetLocalAimPreview(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Harvest::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();
	const UAbilitySystemComponent* AbilitySystem = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(Handle) : nullptr;
	if (!CooldownEffect || !Spec)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	const FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownEffect->GetClass(),
		GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpec.IsValid())
	{
		return;
	}

	// Only effects with a duration can be tuned; the server and the predicting client resolve the same tree state.
	const float BaseDuration = CooldownSpec.Data->GetDuration();
	if (BaseDuration > 0.0f)
	{
		const float TunedDuration = GetTunedValueForSpec(
			*Spec,
			*ActorInfo,
			RpgHarvestingMagicGameplayTags::Ability_Tuning_Harvest_Cooldown,
			BaseDuration);
		if (FMath::IsFinite(TunedDuration) && !FMath::IsNearlyEqual(TunedDuration, BaseDuration))
		{
			CooldownSpec.Data->SetDuration(FMath::Max(0.05f, TunedDuration), true);
		}
	}
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
}

void URpgGameplayAbility_Harvest::HandleAimInputReleased(const float TimeHeld)
{
	(void)TimeHeld;
	BeginHarvestExecution();
}

void URpgGameplayAbility_Harvest::HandleMontageCompleted()
{
	// The swing's facing ends with it; a stride lets the harvester move freely.
	EndFacingCommitment();
	// A stride outlasts its montage; the server ends the activation when the stride is over.
	if (!HasStride())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Harvest::HandleMontageInterrupted()
{
	EndFacingCommitment();
	if (!HasStride())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void URpgGameplayAbility_Harvest::BeginHarvestExecution()
{
	// What the player aimed at when committing to the harvest is what the authoritative commit selects from,
	// even though the commit happens later in the swing and the aim camera blends back right away.
	bHasCommitView = HasAuthority(&CurrentActivationInfo) && CurrentActorInfo &&
		GetViewPoint(*CurrentActorInfo, CommitViewLocation, CommitViewRotation);

	// The tuned values are captured at the same moment, so learning or resetting a node mid-swing changes nothing.
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	bHasCommitValues = HasAuthority(&CurrentActivationInfo) && CurrentActorInfo && Spec;
	if (bHasCommitValues)
	{
		ResolveTunedValues(*Spec, *CurrentActorInfo, CommitValues);
	}
	SetLocalAimPreview(false);
	ClearCameraMode();
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const bool bAuthority = HasAuthority(&CurrentActivationInfo);
	if (!HarvestMontage)
	{
		// Without a montage the server ends the activation after its commit and replicates the end.
		if (bAuthority)
		{
			ScheduleAuthorityCommit(CommitDelaySeconds);
		}
		return;
	}

	float CommitDelay = 0.0f;
	FString FailureReason;
	if (!ResolveCommitDelay(HarvestMontage, CommitEventTag, MontagePlayRate, CommitDelay, FailureReason))
	{
		UE_LOG(
			LogRpgHarvesting,
			Error,
			TEXT("%s cannot schedule its harvest commit: %s"),
			*GetNameSafe(GetClass()),
			*FailureReason);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		HarvestMontage,
		MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
	// The aim was captured above; the swing holds the facing that chose it.
	if (IsActive())
	{
		BeginFacingCommitment(0.0f);
	}

	// The commit time comes from authored montage data instead of server-side notify delivery.
	if (bAuthority && IsActive())
	{
		ScheduleAuthorityCommit(CommitDelay);
	}
}

void URpgGameplayAbility_Harvest::ScheduleAuthorityCommit(const float DelaySeconds)
{
	bCommitPending = true;
	UWorld* World = GetWorld();
	if (!World || DelaySeconds <= UE_SMALL_NUMBER)
	{
		ExecuteAuthorityCommit();
		return;
	}
	World->GetTimerManager().SetTimer(
		CommitTimerHandle,
		this,
		&ThisClass::ExecuteAuthorityCommit,
		DelaySeconds,
		false);
}

void URpgGameplayAbility_Harvest::ExecuteAuthorityCommit()
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!bCommitPending || !IsActive() || !CurrentActorInfo || !Spec)
	{
		return;
	}
	bCommitPending = false;

	// Targets are selected again at the moment of extraction with the same query the client previewed, from the
	// aim and tuned values captured when execution started; stock and reach are evaluated now.
	FRpgHarvestTunedValues Values = CommitValues;
	if (!bHasCommitValues)
	{
		ResolveTunedValues(*Spec, *CurrentActorInfo, Values);
	}
	FRpgHarvestRequest RequestTemplate;
	BuildRequestTemplate(*Spec, *CurrentActorInfo, Values, RequestTemplate);
	// Everything this execution harvests, including chained resources, a swarm, and stride pulses, shares one root.
	RequestTemplate.Provenance.RootActionId = FRpgHarvestProvenance::MakeRootActionId();

	// A stride harvests around the walking harvester in pulses until its duration is over.
	if (HasStride())
	{
		StartStride(RequestTemplate, Values);
		return;
	}

	FVector ViewLocation = CommitViewLocation;
	FRotator ViewRotation = CommitViewRotation;
	FRpgHarvestPreview Selection;
	if (bHasCommitView || GetViewPoint(*CurrentActorInfo, ViewLocation, ViewRotation))
	{
		// Chains follow from what the commit actually depletes, so the selection leaves them out.
		EvaluateTargetsFromView(*Spec, *CurrentActorInfo, Values, ViewLocation, ViewRotation, Selection, false);
	}

	// A swarm harvests the selection later, creature by creature; this commit only summons it.
	if (SummonsSwarm())
	{
		const bool bReserved = SummonSwarm(Selection, RequestTemplate, Values.Swarm);
		ExecuteHarvestCue(
			bReserved ? Values.SuccessGameplayCue : NoYieldGameplayCue,
			Selection.AimPoint,
			FVector::UpVector,
			Values.Targeting.AreaRadius);
		if (!HarvestMontage && IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return;
	}

	// Every target extracts its own stock, but their rewards reach the harvester as one delivery: one atomic
	// inventory batch, or one drop when it does not fit.
	const FRpgHarvestTargetEvaluation* FirstHarvested = nullptr;
	{
		FRpgHarvestRewardBatch RewardBatch(RequestTemplate.Harvester);
		RewardBatch.SetYieldConversions(RequestTemplate.YieldConversions);
		FirstHarvested = CommitSelection(Selection, RequestTemplate, Values);

		// A single target drops overflow where it was struck; an area drops it at the harvester's feet.
		FTransform DropTransform = RequestTemplate.Harvester ? RequestTemplate.Harvester->GetActorTransform() : FTransform::Identity;
		if (FirstHarvested && !RequestTemplate.bAreaHarvest)
		{
			DropTransform.SetLocation(FirstHarvested->Hit.ImpactPoint);
		}
		const ERpgHarvestDelivery BatchDelivery = FRpgHarvestStockRules::ToDelivery(RewardBatch.Deliver(DropTransform));
		for (FRpgHarvestTargetEvaluation& Target : Selection.Targets)
		{
			if (Target.Result.IsSuccess() && Target.Result.Delivery == ERpgHarvestDelivery::None)
			{
				Target.Result.Delivery = BatchDelivery;
			}
		}
	}

	if (FirstHarvested)
	{
		ExecuteSuccessCue(Values, Selection, *FirstHarvested);
	}
	else
	{
		ExecuteHarvestCue(NoYieldGameplayCue, Selection.AimPoint, FVector::UpVector);
	}

	K2_OnHarvestResolved(Selection.Targets);
	if (!HarvestMontage && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

const FRpgHarvestTargetEvaluation* URpgGameplayAbility_Harvest::CommitSelection(
	FRpgHarvestPreview& Selection,
	const FRpgHarvestRequest& RequestTemplate,
	const FRpgHarvestTunedValues& Values)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// The presentation wave starts at the harvested target nearest to the harvester and travels outward.
	const FVector WaveOrigin = RequestTemplate.Harvester ? RequestTemplate.Harvester->GetActorLocation() : FVector::ZeroVector;
	double NearestTargetDistance = 0.0;
	if (PresentationWaveSpeed > UE_KINDA_SMALL_NUMBER)
	{
		NearestTargetDistance = TNumericLimits<double>::Max();
		for (const FRpgHarvestTargetEvaluation& Target : Selection.Targets)
		{
			if (Target.WouldHarvest())
			{
				NearestTargetDistance = FMath::Min(NearestTargetDistance, FVector::Dist2D(WaveOrigin, Target.Hit.ImpactPoint));
			}
		}
	}

	int32 FirstHarvestedIndex = INDEX_NONE;
	int32 TargetCues = 0;
	TArray<float, TInlineAllocator<16>> PresentationDelays;
	PresentationDelays.SetNumZeroed(Selection.Targets.Num());
	for (int32 TargetIndex = 0; TargetIndex < Selection.Targets.Num(); ++TargetIndex)
	{
		FRpgHarvestTargetEvaluation& Target = Selection.Targets[TargetIndex];
		UObject* Receiver = Target.Receiver.Get();
		if (!Target.WouldHarvest() || !Receiver)
		{
			continue;
		}

		// The commit may shrink or hide the resource right away, so its target cue takes the bounds from before.
		const bool bTargetCue = TargetGameplayCue.IsValid() && TargetCues < MaxTargetCuesPerCommit;
		const FBox TargetBounds = bTargetCue ? GetPresentedTargetBounds(Target.Hit) : FBox(ForceInit);

		FRpgHarvestRequest Request = RequestTemplate;
		Request.Hit = Target.Hit;
		Request.TraceOrigin = Target.Hit.TraceStart;
		Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Receiver, Target.Hit);
		if (PresentationWaveSpeed > UE_KINDA_SMALL_NUMBER)
		{
			const double WaveDistance = FVector::Dist2D(WaveOrigin, Target.Hit.ImpactPoint) - NearestTargetDistance;
			Request.PresentationDelaySeconds = static_cast<float>(FMath::Clamp(
				WaveDistance / PresentationWaveSpeed,
				0.0,
				static_cast<double>(URpgHarvestInstanceStockComponent::MaxPresentationDelaySeconds)));
		}
		PresentationDelays[TargetIndex] = Request.PresentationDelaySeconds;

		// A weak-point hit earlier armed a follow-up on this target; the next hit on it does more work.
		const bool bFollowUp = Values.FollowUpSeconds > 0.0f && FollowUpDeadline >= Now &&
			FollowUpReceiver.Get() == Receiver && FollowUpItem == Target.Hit.Item;
		if (bFollowUp)
		{
			Request.WorkScale = FMath::Min(Request.WorkScale * Values.FollowUpWorkScale, RpgHarvestWork::MaxWorkScale);
		}
		Target.Result = IRpgHarvestableTarget::Execute_CommitHarvest(Receiver, Request);
		if (!Target.Result.IsSuccess())
		{
			continue;
		}
		if (bFollowUp)
		{
			FollowUpDeadline = -1.0;
			FollowUpReceiver.Reset();
		}
		if (Target.Result.bWeakPointHit && Values.FollowUpSeconds > 0.0f)
		{
			FollowUpReceiver = Receiver;
			FollowUpItem = Target.Hit.Item;
			FollowUpDeadline = Now + Values.FollowUpSeconds;
		}
		// A socketed rune such as Brand sets the resource burning for this player; the burn's work per second is a
		// fraction of this hit's own work, without follow-up or weak-point bonus. Talents decide what a further hit
		// does to a running burn.
		if (BurnClass && Values.BurnWork > 0.0f && Values.BurnSeconds > 0.0f && Target.Result.RemainingWork > 0)
		{
			FRpgHarvestBurnParams BurnParams;
			BurnParams.WorkPerSecond = Values.BurnWork * RequestTemplate.RequestedSections * RequestTemplate.WorkScale;
			BurnParams.DurationSeconds = Values.BurnSeconds;
			BurnParams.bRefresh = Values.bBurnRefresh;
			BurnParams.DischargeMultiplier = Values.BurnDischarge;
			ARpgHarvestBurn::Light(Receiver, Request, BurnClass, BurnParams);
		}
		if (FirstHarvestedIndex == INDEX_NONE)
		{
			FirstHarvestedIndex = TargetIndex;
		}
		if (bTargetCue)
		{
			const FVector Center = TargetBounds.GetCenter();
			const FVector Extent = TargetBounds.GetExtent();
			ExecuteHarvestCue(
				TargetGameplayCue,
				FVector(Center.X, Center.Y, TargetBounds.Max.Z),
				FVector::UpVector,
				static_cast<float>(FMath::Max(Extent.X, Extent.Y)));
			++TargetCues;
		}
	}

	// A target depleted inside a chain box takes the box's other resources along. Their presentation continues from
	// the moment the trigger is presented, and their rewards join the open batch.
	if (UWorld* World = GetWorld(); World && FirstHarvestedIndex != INDEX_NONE)
	{
		TSet<TObjectKey<URpgHarvestChainComponent>> ChainedBoxes;
		TArray<FRpgHarvestTargetEvaluation> Chained;
		FRpgHarvestRequest TriggerRequest = RequestTemplate;
		for (int32 TargetIndex = 0; TargetIndex < PresentationDelays.Num(); ++TargetIndex)
		{
			TriggerRequest.PresentationDelaySeconds = PresentationDelays[TargetIndex];
			FRpgHarvestChains::Commit(*World, TriggerRequest, Selection.Targets[TargetIndex], ChainedBoxes, Chained);
		}
		Selection.Targets.Append(MoveTemp(Chained));
	}
	return FirstHarvestedIndex != INDEX_NONE ? &Selection.Targets[FirstHarvestedIndex] : nullptr;
}

void URpgGameplayAbility_Harvest::StartStride(const FRpgHarvestRequest& RequestTemplate, const FRpgHarvestTunedValues& Values)
{
	UWorld* World = GetWorld();
	if (!World || bStriding)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	bStriding = true;
	StrideValues = Values;
	StrideRequestTemplate = RequestTemplate;
	StrideEndTime = World->GetTimeSeconds() + Stride.DurationSeconds;

	// The batch opens only around each pulse, so it never collects the player's other harvests in between.
	StrideRewardBatch = MakeUnique<FRpgHarvestRewardBatch>(RequestTemplate.Harvester, false);
	StrideRewardBatch->SetYieldConversions(RequestTemplate.YieldConversions);

	UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem && StrideGameplayCue.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Instigator = CurrentActorInfo->AvatarActor.Get();
		CueParameters.EffectCauser = CurrentActorInfo->AvatarActor.Get();
		CueParameters.RawMagnitude = Values.Targeting.AreaRadius;
		// The stride starts inside the owning client's predicted activation, but no client predicts this cue. Without
		// that prediction key, the owning client plays it from replication like every other machine instead of
		// skipping it as already predicted.
		FScopedPredictionWindow UnpredictedCue(AbilitySystem, FPredictionKey(), false);
		AbilitySystem->AddGameplayCue(StrideGameplayCue, CueParameters);
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(StrideEndTimerHandle, this, &ThisClass::HandleStrideElapsed, Stride.DurationSeconds, false);
	TimerManager.SetTimer(
		StridePulseTimerHandle,
		this,
		&ThisClass::PulseStride,
		FMath::Max(0.1f, Stride.PulseIntervalSeconds),
		true);
	PulseStride();
}

void URpgGameplayAbility_Harvest::PulseStride()
{
	UWorld* World = GetWorld();
	if (!bStriding || !IsActive() || !World || !CurrentActorInfo)
	{
		return;
	}
	// The last pulse happens before the stride's end, which delivers.
	if (World->GetTimeSeconds() >= StrideEndTime - 0.001)
	{
		return;
	}
	// A dead or vanished harvester harvests nothing more; ending delivers what was harvested so far.
	const AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	const UAbilitySystemComponent* AbilitySystem = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!Avatar || (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(RpgGameplayTags::Status_Death)))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Each pulse selects again around where the harvester is now, with the values captured when the stride started.
	FRpgHarvestPreview Selection;
	Selection.AimPoint = FRpgHarvestTargeting::SelectAndEvaluate(
		*World,
		StrideValues.Targeting,
		Avatar->GetActorLocation(),
		Avatar->GetActorRotation(),
		*Avatar,
		StrideRequestTemplate,
		Selection.Targets);

	const bool bBatchOpen = StrideRewardBatch && StrideRewardBatch->Open();
	const FRpgHarvestTargetEvaluation* FirstHarvested = CommitSelection(Selection, StrideRequestTemplate, StrideValues);
	if (bBatchOpen)
	{
		StrideRewardBatch->Close();
	}

	if (FirstHarvested)
	{
		ExecuteSuccessCue(StrideValues, Selection, *FirstHarvested);
		K2_OnHarvestResolved(Selection.Targets);
	}
}

void URpgGameplayAbility_Harvest::HandleStrideElapsed()
{
	if (bStriding && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Harvest::FinishStride()
{
	if (!bStriding)
	{
		return;
	}
	bStriding = false;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(StridePulseTimerHandle);
		World->GetTimerManager().ClearTimer(StrideEndTimerHandle);
	}
	UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilitySystem && StrideGameplayCue.IsValid())
	{
		AbilitySystem->RemoveGameplayCue(StrideGameplayCue);
	}

	if (StrideRewardBatch)
	{
		// The rewards of all pulses arrive as one delivery, also when the stride ends early; only a world that ends
		// takes them along. Overflow lands at the harvester's feet like any area harvest.
		if (World && !World->bIsTearingDown && !World->IsBeingCleanedUp())
		{
			const AActor* Harvester = StrideRequestTemplate.Harvester;
			StrideRewardBatch->Deliver(IsValid(Harvester) ? Harvester->GetActorTransform() : FTransform::Identity);
		}
		else if (StrideRewardBatch->HasPendingRewards())
		{
			UE_LOG(LogRpgHarvesting, Warning, TEXT("%s ended with the world before its stride delivered."), *GetNameSafe(GetClass()));
		}
		StrideRewardBatch->Discard();
		StrideRewardBatch.Reset();
	}
	StrideRequestTemplate = FRpgHarvestRequest();
}

void URpgGameplayAbility_Harvest::ExecuteHarvestCue(
	const FGameplayTag CueTag,
	const FVector& Location,
	const FVector& Normal,
	const float Radius) const
{
	UAbilitySystemComponent* AbilitySystem = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystem || !CueTag.IsValid())
	{
		return;
	}
	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = CurrentActorInfo->AvatarActor.Get();
	CueParameters.EffectCauser = CurrentActorInfo->AvatarActor.Get();
	CueParameters.Location = Location;
	CueParameters.Normal = Normal;
	CueParameters.RawMagnitude = Radius;
	// A commit without a montage or delay, such as a stride's first pulse, runs inside the owning client's predicted
	// activation. No client predicts these cues; without the prediction key, the owning client plays them from
	// replication like every other machine instead of skipping them as already predicted.
	FScopedPredictionWindow UnpredictedCue(AbilitySystem, FPredictionKey(), false);
	AbilitySystem->ExecuteGameplayCue(CueTag, CueParameters);
}

void URpgGameplayAbility_Harvest::ExecuteSuccessCue(
	const FRpgHarvestTunedValues& Values,
	const FRpgHarvestPreview& Selection,
	const FRpgHarvestTargetEvaluation& FirstHarvested) const
{
	if (HarvestsArea())
	{
		ExecuteHarvestCue(Values.SuccessGameplayCue, Selection.AimPoint, FVector::UpVector, Values.Targeting.AreaRadius);
		return;
	}
	ExecuteHarvestCue(Values.SuccessGameplayCue, FirstHarvested.Hit.ImpactPoint, FirstHarvested.Hit.ImpactNormal);
}

bool URpgGameplayAbility_Harvest::SummonSwarm(
	const FRpgHarvestPreview& Selection,
	const FRpgHarvestRequest& RequestTemplate,
	const FRpgHarvestSwarmParams& SwarmParams)
{
	UWorld* World = GetWorld();
	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!World || !Avatar || !SwarmClass)
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Avatar;
	SpawnParameters.Instigator = Cast<APawn>(Avatar);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgHarvestSwarm* SummonedSwarm = World->SpawnActor<ARpgHarvestSwarm>(
		SwarmClass,
		FTransform(Selection.AimPoint),
		SpawnParameters);
	if (!SummonedSwarm ||
		!SummonedSwarm->StartSwarm(Avatar, RequestTemplate, SwarmParams, Selection.Targets))
	{
		if (SummonedSwarm)
		{
			SummonedSwarm->Destroy();
		}
		UE_LOG(LogRpgHarvesting, Error, TEXT("%s could not summon its swarm."), *GetNameSafe(GetClass()));
		return false;
	}

	for (const FRpgHarvestSwarmCreature& Creature : SummonedSwarm->GetCreatures())
	{
		if (Creature.State == ERpgHarvestSwarmCreatureState::Flying)
		{
			return true;
		}
	}
	return false;
}

void URpgGameplayAbility_Harvest::PlanSwarm(
	const UWorld& World,
	const AActor& Avatar,
	const FRpgHarvestTunedValues& Values,
	FRpgHarvestPreview& InOutSelection) const
{
	const ARpgHarvestSwarm* SwarmDefaults = SwarmClass ? SwarmClass->GetDefaultObject<ARpgHarvestSwarm>() : nullptr;
	if (!SwarmDefaults)
	{
		return;
	}

	const FVector Lift(0.0, 0.0, SwarmDefaults->GetStrikeHeight());
	if (Values.Swarm.bRequireLineOfSight)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgHarvestSwarmPlan), false, &Avatar);
		if (AActor* AvatarOwner = Avatar.GetOwner())
		{
			QueryParams.AddIgnoredActor(AvatarOwner);
		}
		for (FRpgHarvestTargetEvaluation& Target : InOutSelection.Targets)
		{
			if (Target.WouldHarvest() &&
				!FRpgHarvestSwarmPlanner::HasLineOfSight(
					World,
					InOutSelection.AimPoint + Lift,
					Target.Hit.ImpactPoint + Lift,
					Values.Targeting.TraceChannel,
					QueryParams))
			{
				Target.bInReach = false;
			}
		}
	}

	// The swarm works until every selected resource is empty, so each one shows its whole remaining stock.
	for (FRpgHarvestTargetEvaluation& Target : InOutSelection.Targets)
	{
		if (Target.WouldHarvest())
		{
			Target.Result.SectionsTaken = FRpgHarvestSwarmPlanner::GetAvailableSections(Target);
			Target.Result.RemainingSections = 0;
		}
	}
}

void URpgGameplayAbility_Harvest::EvaluateTargets(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	FRpgHarvestPreview& OutPreview) const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ActorInfo, ViewLocation, ViewRotation))
	{
		OutPreview = FRpgHarvestPreview();
		return;
	}
	FRpgHarvestTunedValues Values;
	ResolveTunedValues(Spec, ActorInfo, Values);
	EvaluateTargetsFromView(Spec, ActorInfo, Values, ViewLocation, ViewRotation, OutPreview, true);
}

void URpgGameplayAbility_Harvest::ResolveTunedValues(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	FRpgHarvestTunedValues& OutValues) const
{
	using namespace RpgHarvestingMagicGameplayTags;

	auto Tune = [&Spec, &ActorInfo](const FGameplayTag TuningTag, const float BaseValue)
	{
		const float Value = GetTunedValueForSpec(Spec, ActorInfo, TuningTag, BaseValue);
		return FMath::IsFinite(Value) ? Value : BaseValue;
	};
	auto TuneCount = [&Tune](const FGameplayTag TuningTag, const int32 BaseValue, const int32 MinValue, const int32 MaxValue)
	{
		return FMath::Clamp(FMath::RoundToInt(Tune(TuningTag, static_cast<float>(BaseValue))), MinValue, MaxValue);
	};

	OutValues.Targeting = Targeting;
	OutValues.Targeting.MaxReachFromAvatar = FMath::Max(0.0f, Tune(Ability_Tuning_Harvest_Reach, Targeting.MaxReachFromAvatar));
	// The aim ray grows with the reach, so a longer reach can be aimed at as well.
	OutValues.Targeting.MaxAimDistance = Targeting.MaxAimDistance +
		FMath::Max(0.0f, OutValues.Targeting.MaxReachFromAvatar - Targeting.MaxReachFromAvatar);
	if (HarvestsArea())
	{
		OutValues.Targeting.AreaRadius = FMath::Max(0.0f, Tune(Ability_Tuning_Harvest_AreaRadius, Targeting.AreaRadius));
		OutValues.Targeting.MaxTargets = TuneCount(Ability_Tuning_Harvest_MaxTargets, Targeting.MaxTargets, 1, 64);
	}
	OutValues.SectionsPerTarget = TuneCount(Ability_Tuning_Harvest_Sections, FMath::Max(1, SectionsPerTarget), 1, 64);
	OutValues.WorkScale = FMath::Clamp(Tune(Ability_Tuning_Harvest_Work, 1.0f), 0.01f, RpgHarvestWork::MaxWorkScale);
	OutValues.WeakPointWorkScale = FMath::Clamp(Tune(Ability_Tuning_Harvest_WeakPointWork, 1.0f), 0.0f, RpgHarvestWork::MaxWorkScale);
	OutValues.FollowUpWorkScale = FMath::Clamp(Tune(Ability_Tuning_Harvest_FollowUpWork, 1.0f), 0.01f, RpgHarvestWork::MaxWorkScale);
	OutValues.FollowUpSeconds = FMath::Max(0.0f, Tune(Ability_Tuning_Harvest_FollowUpWindow, 0.0f));
	OutValues.BurnWork = FMath::Clamp(Tune(Ability_Tuning_Harvest_BurnWork, 0.0f), 0.0f, RpgHarvestWork::MaxWorkScale);
	OutValues.BurnSeconds = FMath::Clamp(Tune(Ability_Tuning_Harvest_BurnDuration, 0.0f), 0.0f, 60.0f);
	OutValues.bBurnRefresh = Tune(Ability_Tuning_Harvest_BurnRefresh, 0.0f) > 0.0f;
	OutValues.BurnDischarge = FMath::Clamp(Tune(Ability_Tuning_Harvest_BurnDischarge, 0.0f), 0.0f, RpgHarvestWork::MaxWorkScale);

	OutValues.Swarm = Swarm;
	if (SummonsSwarm())
	{
		OutValues.Swarm.CreatureCount =
			TuneCount(Ability_Tuning_Harvest_Creatures, Swarm.CreatureCount, 1, ARpgHarvestSwarm::MaxCreatures);
		OutValues.Swarm.StrikeIntervalSeconds =
			FMath::Max(0.0f, Tune(Ability_Tuning_Harvest_StrikeInterval, Swarm.StrikeIntervalSeconds));
		OutValues.Swarm.StrikeRadius = FMath::Max(0.0f, Tune(Ability_Tuning_Harvest_StrikeRadius, Swarm.StrikeRadius));
	}

	// Forms of the skill tree switch conversions on through loose tags they grant while the weapon is in use.
	OutValues.YieldConversions.Reset();
	const UAbilitySystemComponent* AbilitySystem = ActorInfo.AbilitySystemComponent.Get();
	for (const FRpgHarvestYieldConversion& Conversion : YieldConversions)
	{
		if (Conversion.IsValid() &&
			(!Conversion.RequiredOwnerTag.IsValid() ||
				(AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Conversion.RequiredOwnerTag))))
		{
			OutValues.YieldConversions.Add(Conversion);
		}
	}

	// A form can also replace the power's look.
	OutValues.SuccessGameplayCue = SuccessGameplayCue;
	for (const FRpgHarvestFormGameplayCue& FormCue : FormSuccessGameplayCues)
	{
		if (FormCue.RequiredOwnerTag.IsValid() && AbilitySystem && AbilitySystem->HasMatchingGameplayTag(FormCue.RequiredOwnerTag))
		{
			OutValues.SuccessGameplayCue = FormCue.GameplayCue;
			break;
		}
	}
}

void URpgGameplayAbility_Harvest::EvaluateTargetsFromView(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	const FRpgHarvestTunedValues& Values,
	const FVector& ViewLocation,
	const FRotator& ViewRotation,
	FRpgHarvestPreview& OutPreview,
	const bool bPreviewChains) const
{
	OutPreview = FRpgHarvestPreview();
	OutPreview.AbilityId = HarvestAbilityId;
	OutPreview.bHasArea = HarvestsArea();
	OutPreview.AreaRadius = OutPreview.bHasArea ? Values.Targeting.AreaRadius : 0.0f;
	OutPreview.YieldConversions = Values.YieldConversions;

	const AActor* Avatar = ActorInfo.AvatarActor.Get();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FRpgHarvestRequest RequestTemplate;
	BuildRequestTemplate(Spec, ActorInfo, Values, RequestTemplate);
	OutPreview.AimPoint = FRpgHarvestTargeting::SelectAndEvaluate(
		*World,
		Values.Targeting,
		ViewLocation,
		ViewRotation,
		*Avatar,
		RequestTemplate,
		OutPreview.Targets);
	if (SummonsSwarm())
	{
		PlanSwarm(*World, *Avatar, Values, OutPreview);
	}
	if (bPreviewChains)
	{
		FRpgHarvestChains::AppendPreview(*World, RequestTemplate, OutPreview.Targets);
	}
}

bool URpgGameplayAbility_Harvest::ResolveCommitDelay(
	const UAnimMontage* Montage,
	const FGameplayTag InCommitEventTag,
	const float PlayRate,
	float& OutDelaySeconds,
	FString& OutFailureReason)
{
	OutDelaySeconds = 0.0f;
	OutFailureReason.Reset();
	if (!Montage)
	{
		OutFailureReason = TEXT("No harvest montage is configured.");
		return false;
	}
	if (!InCommitEventTag.IsValid())
	{
		OutFailureReason = TEXT("No commit GameplayEvent tag is configured.");
		return false;
	}

	const float EffectivePlayRate = PlayRate * Montage->RateScale;
	if (!FMath::IsFinite(EffectivePlayRate) || EffectivePlayRate <= UE_SMALL_NUMBER)
	{
		OutFailureReason = FString::Printf(TEXT("The effective play rate must be positive; found %.3f."), EffectivePlayRate);
		return false;
	}
	if (Montage->TimeStretchCurve.IsValid())
	{
		OutFailureReason = TEXT("Harvest montages with a time-stretch curve are unsupported by the authored commit schedule.");
		return false;
	}
	if (Montage->CompositeSections.Num() > 1 ||
		(Montage->CompositeSections.Num() == 1 && !Montage->CompositeSections[0].NextSectionName.IsNone()))
	{
		OutFailureReason = TEXT("Harvest montages must have at most one section without a section link.");
		return false;
	}

	int32 CommitNotifyCount = 0;
	float CommitTime = 0.0f;
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UAnimNotify_RpgGameplayEvent* GameplayEventNotify = Cast<UAnimNotify_RpgGameplayEvent>(NotifyEvent.Notify);
		if (GameplayEventNotify && GameplayEventNotify->GetEventTag() == InCommitEventTag)
		{
			++CommitNotifyCount;
			CommitTime = NotifyEvent.GetTriggerTime();
		}
	}
	if (CommitNotifyCount != 1)
	{
		OutFailureReason = FString::Printf(
			TEXT("Expected exactly one RPG Gameplay Event notify sending %s, found %d."),
			*InCommitEventTag.ToString(),
			CommitNotifyCount);
		return false;
	}

	const float PlayLength = Montage->GetPlayLength();
	if (!FMath::IsFinite(CommitTime) || CommitTime < 0.0f || CommitTime >= PlayLength)
	{
		OutFailureReason = FString::Printf(
			TEXT("The commit notify time %.3f must lie inside the montage length %.3f."),
			CommitTime,
			PlayLength);
		return false;
	}

	OutDelaySeconds = CommitTime / EffectivePlayRate;
	return true;
}

void URpgGameplayAbility_Harvest::SetLocalAimPreview(const bool bAiming) const
{
	if (!bAimWhileInputHeld || !CurrentActorInfo || !IsLocallyControlled())
	{
		return;
	}
	if (URpgHarvestTargetingComponent* TargetingComponent =
			URpgHarvestTargetingComponent::FindForController(GetControllerFromActorInfo()))
	{
		TargetingComponent->SetAimingAbility(CurrentSpecHandle, bAiming);
	}
}

bool URpgGameplayAbility_Harvest::MeetsSkillRequirement(const FGameplayAbilityActorInfo& ActorInfo) const
{
	if (!RequiredSkillTag.IsValid())
	{
		return true;
	}

	const ARpgPlayerState* PlayerState = FRpgHarvestRewardService::ResolveHarvesterPlayerState(ActorInfo.OwnerActor.Get());
	if (!PlayerState)
	{
		PlayerState = FRpgHarvestRewardService::ResolveHarvesterPlayerState(ActorInfo.AvatarActor.Get());
	}
	const URpgTradeSkillProgressionComponent* TradeSkills =
		PlayerState ? PlayerState->GetTradeSkillProgressionComponent() : nullptr;
	return TradeSkills &&
		TradeSkills->GetSkillLevelByTag(RequiredSkillTag) >= FMath::Clamp(MinimumSkillLevel, 1, 100);
}

void URpgGameplayAbility_Harvest::BuildRequestTemplate(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	const FRpgHarvestTunedValues& Values,
	FRpgHarvestRequest& OutRequest) const
{
	OutRequest = FRpgHarvestRequest();
	OutRequest.Harvester = ActorInfo.AvatarActor.Get();
	OutRequest.AbilityId = HarvestAbilityId;
	OutRequest.RequestedSections = FMath::Max(1, Values.SectionsPerTarget);
	OutRequest.WorkScale = Values.WorkScale;
	OutRequest.WeakPointWorkScale = Values.WeakPointWorkScale;
	OutRequest.YieldConversions = Values.YieldConversions;
	OutRequest.bCanHitWeakPoint = bCanHitWeakPoints && Targeting.Shape == ERpgHarvestTargetShape::SingleTarget;
	OutRequest.bAreaHarvest = HarvestsArea();

	float ToolPower = 1.0f;
	const URpgEquipmentInstance* Equipment = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
	const URpgInventoryItemInstance* Item = Equipment ? Cast<URpgInventoryItemInstance>(Equipment->GetInstigator()) : nullptr;
	if (const URpgInventoryFragment_HarvestingTool* Tool =
			Item ? Item->FindFragmentByClass<URpgInventoryFragment_HarvestingTool>() : nullptr)
	{
		OutRequest.ToolTag = Tool->ToolTag;
		ToolPower = Tool->HarvestPower;
	}

	const float Power = ToolPower * HarvestPowerScale;
	OutRequest.HarvestPower = FMath::IsFinite(Power) ? FMath::Max(0.01f, Power) : 1.0f;
}

bool URpgGameplayAbility_Harvest::GetViewPoint(
	const FGameplayAbilityActorInfo& ActorInfo,
	FVector& OutLocation,
	FRotator& OutRotation)
{
	if (const APlayerController* PlayerController = ActorInfo.PlayerController.Get())
	{
		PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}

	const AActor* Avatar = ActorInfo.AvatarActor.Get();
	if (!Avatar)
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Avatar); Pawn && Pawn->GetController())
	{
		Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}
	Avatar->GetActorEyesViewPoint(OutLocation, OutRotation);
	return true;
}
