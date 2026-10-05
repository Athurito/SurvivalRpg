#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "Inventory/RpgInventoryFragment_HarvestingTool.h"
#include "SurvivalRpg/Animation/AnimNotify_RpgGameplayEvent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Harvest)

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
	// A commit that has not happened yet never happens after the activation ends.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CommitTimerHandle);
	}
	bCommitPending = false;
	bHasCommitView = false;
	SetLocalAimPreview(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Harvest::HandleAimInputReleased(const float TimeHeld)
{
	(void)TimeHeld;
	BeginHarvestExecution();
}

void URpgGameplayAbility_Harvest::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URpgGameplayAbility_Harvest::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URpgGameplayAbility_Harvest::BeginHarvestExecution()
{
	// What the player aimed at when committing to the harvest is what the authoritative commit selects from,
	// even though the commit happens later in the swing and the aim camera blends back right away.
	bHasCommitView = HasAuthority(&CurrentActivationInfo) && CurrentActorInfo &&
		GetViewPoint(*CurrentActorInfo, CommitViewLocation, CommitViewRotation);
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
	// aim captured when execution started; stock and reach are evaluated now.
	FRpgHarvestPreview Selection;
	if (bHasCommitView)
	{
		EvaluateTargetsFromView(*Spec, *CurrentActorInfo, CommitViewLocation, CommitViewRotation, Selection);
	}
	else
	{
		EvaluateTargets(*Spec, *CurrentActorInfo, Selection);
	}
	FRpgHarvestRequest RequestTemplate;
	BuildRequestTemplate(*Spec, *CurrentActorInfo, RequestTemplate);

	// A swarm harvests the selection later, creature by creature; this commit only summons it.
	if (SummonsSwarm())
	{
		const bool bReserved = SummonSwarm(Selection, RequestTemplate);
		ExecuteHarvestCue(bReserved ? SuccessGameplayCue : NoYieldGameplayCue, Selection.AimPoint, FVector::UpVector);
		if (!HarvestMontage && IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return;
	}

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

	// Every target extracts its own stock, but their rewards reach the harvester as one delivery: one atomic
	// inventory batch, or one drop when it does not fit.
	const FRpgHarvestTargetEvaluation* FirstHarvested = nullptr;
	{
		FRpgHarvestRewardBatch RewardBatch(RequestTemplate.Harvester);
		for (FRpgHarvestTargetEvaluation& Target : Selection.Targets)
		{
			UObject* Receiver = Target.Receiver.Get();
			if (!Target.WouldHarvest() || !Receiver)
			{
				continue;
			}

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
			Target.Result = IRpgHarvestableTarget::Execute_CommitHarvest(Receiver, Request);
			if (Target.Result.IsSuccess() && !FirstHarvested)
			{
				FirstHarvested = &Target;
			}
		}

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

	ExecuteHarvestCue(
		FirstHarvested ? SuccessGameplayCue : NoYieldGameplayCue,
		FirstHarvested ? FVector(FirstHarvested->Hit.ImpactPoint) : Selection.AimPoint,
		FirstHarvested ? FVector(FirstHarvested->Hit.ImpactNormal) : FVector::UpVector);

	K2_OnHarvestResolved(Selection.Targets);
	if (!HarvestMontage && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgGameplayAbility_Harvest::ExecuteHarvestCue(const FGameplayTag CueTag, const FVector& Location, const FVector& Normal) const
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
	AbilitySystem->ExecuteGameplayCue(CueTag, CueParameters);
}

bool URpgGameplayAbility_Harvest::SummonSwarm(const FRpgHarvestPreview& Selection, const FRpgHarvestRequest& RequestTemplate)
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
		!SummonedSwarm->StartSwarm(
			Avatar,
			RequestTemplate,
			Swarm,
			Targeting.AreaRadius,
			Targeting.TraceChannel,
			Selection.Targets))
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
	FRpgHarvestPreview& InOutSelection) const
{
	const ARpgHarvestSwarm* SwarmDefaults = SwarmClass ? SwarmClass->GetDefaultObject<ARpgHarvestSwarm>() : nullptr;
	if (!SwarmDefaults)
	{
		return;
	}

	const FVector Lift(0.0, 0.0, SwarmDefaults->GetStrikeHeight());
	if (Swarm.bRequireLineOfSight)
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
					Targeting.TraceChannel,
					QueryParams))
			{
				Target.bInReach = false;
			}
		}
	}

	TArray<int32> TargetIndices;
	TArray<int32> Sections;
	FRpgHarvestSwarmPlanner::Distribute(InOutSelection.Targets, Swarm.CreatureCount, SectionsPerTarget, TargetIndices, Sections);
	TArray<int32> ReservedSections;
	ReservedSections.Init(0, InOutSelection.Targets.Num());
	for (int32 CreatureIndex = 0; CreatureIndex < TargetIndices.Num(); ++CreatureIndex)
	{
		ReservedSections[TargetIndices[CreatureIndex]] += Sections[CreatureIndex];
	}

	// Every resource shows what its creatures will take, so the preview matches the swarm's plan. Resources no creature
	// reserved leave the selection; rejected ones stay so the preview can explain them.
	TArray<FRpgHarvestTargetEvaluation> PlannedTargets;
	PlannedTargets.Reserve(InOutSelection.Targets.Num());
	for (int32 TargetIndex = 0; TargetIndex < InOutSelection.Targets.Num(); ++TargetIndex)
	{
		FRpgHarvestTargetEvaluation& Target = InOutSelection.Targets[TargetIndex];
		if (Target.WouldHarvest())
		{
			if (ReservedSections[TargetIndex] <= 0)
			{
				continue;
			}
			const int32 Available = FRpgHarvestSwarmPlanner::GetAvailableSections(Target);
			Target.Result.SectionsTaken = ReservedSections[TargetIndex];
			Target.Result.RemainingSections = Available - ReservedSections[TargetIndex];
		}
		PlannedTargets.Add(MoveTemp(Target));
	}
	InOutSelection.Targets = MoveTemp(PlannedTargets);
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
	EvaluateTargetsFromView(Spec, ActorInfo, ViewLocation, ViewRotation, OutPreview);
}

void URpgGameplayAbility_Harvest::EvaluateTargetsFromView(
	const FGameplayAbilitySpec& Spec,
	const FGameplayAbilityActorInfo& ActorInfo,
	const FVector& ViewLocation,
	const FRotator& ViewRotation,
	FRpgHarvestPreview& OutPreview) const
{
	OutPreview = FRpgHarvestPreview();
	OutPreview.AbilityId = HarvestAbilityId;
	OutPreview.bHasArea = Targeting.Shape == ERpgHarvestTargetShape::AreaAtAimPoint;
	OutPreview.AreaRadius = OutPreview.bHasArea ? Targeting.AreaRadius : 0.0f;

	const AActor* Avatar = ActorInfo.AvatarActor.Get();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FRpgHarvestRequest RequestTemplate;
	BuildRequestTemplate(Spec, ActorInfo, RequestTemplate);
	FRpgHarvestTargetingParams Params = Targeting;
	if (SummonsSwarm())
	{
		// Every selected resource gets at least one creature.
		Params.MaxTargets = FMath::Clamp(Swarm.CreatureCount, 1, ARpgHarvestSwarm::MaxCreatures);
	}
	OutPreview.AimPoint = FRpgHarvestTargeting::SelectAndEvaluate(
		*World,
		Params,
		ViewLocation,
		ViewRotation,
		*Avatar,
		RequestTemplate,
		OutPreview.Targets);
	if (SummonsSwarm())
	{
		PlanSwarm(*World, *Avatar, OutPreview);
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
	FRpgHarvestRequest& OutRequest) const
{
	OutRequest = FRpgHarvestRequest();
	OutRequest.Harvester = ActorInfo.AvatarActor.Get();
	OutRequest.AbilityId = HarvestAbilityId;
	OutRequest.RequestedSections = FMath::Max(1, SectionsPerTarget);
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
