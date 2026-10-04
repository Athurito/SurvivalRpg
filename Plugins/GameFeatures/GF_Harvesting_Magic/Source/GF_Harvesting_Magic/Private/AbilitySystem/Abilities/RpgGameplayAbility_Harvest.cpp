#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
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

	const FRpgHarvestTargetEvaluation* FirstHarvested = nullptr;
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
		Target.Result = IRpgHarvestableTarget::Execute_CommitHarvest(Receiver, Request);
		if (Target.Result.IsSuccess() && !FirstHarvested)
		{
			FirstHarvested = &Target;
		}
	}

	if (UAbilitySystemComponent* AbilitySystem = CurrentActorInfo->AbilitySystemComponent.Get())
	{
		const FGameplayTag CueTag = FirstHarvested ? SuccessGameplayCue : NoYieldGameplayCue;
		if (CueTag.IsValid())
		{
			FGameplayCueParameters CueParameters;
			CueParameters.Instigator = CurrentActorInfo->AvatarActor.Get();
			CueParameters.EffectCauser = CurrentActorInfo->AvatarActor.Get();
			CueParameters.Location = FirstHarvested ? FVector(FirstHarvested->Hit.ImpactPoint) : Selection.AimPoint;
			CueParameters.Normal = FirstHarvested ? FVector(FirstHarvested->Hit.ImpactNormal) : FVector::UpVector;
			AbilitySystem->ExecuteGameplayCue(CueTag, CueParameters);
		}
	}

	K2_OnHarvestResolved(Selection.Targets);
	if (!HarvestMontage && IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
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
	OutPreview.AimPoint = FRpgHarvestTargeting::SelectAndEvaluate(
		*World,
		Targeting,
		ViewLocation,
		ViewRotation,
		*Avatar,
		RequestTemplate,
		OutPreview.Targets);
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
