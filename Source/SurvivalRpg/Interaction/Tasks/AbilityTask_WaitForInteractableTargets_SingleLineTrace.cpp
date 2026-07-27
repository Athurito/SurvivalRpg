// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitForInteractableTargets_SingleLineTrace.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_WaitForInteractableTargets_SingleLineTrace)

UAbilityTask_WaitForInteractableTargets_SingleLineTrace::UAbilityTask_WaitForInteractableTargets_SingleLineTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitForInteractableTargets_SingleLineTrace* UAbilityTask_WaitForInteractableTargets_SingleLineTrace::WaitForInteractableTargets_SingleLineTrace(UGameplayAbility* OwningAbility, FInteractionQuery InteractionQuery, FCollisionProfileName TraceProfile, FGameplayAbilityTargetingLocationInfo StartLocation, float InteractionScanRange, float InteractionScanRate, bool bShowDebug)
{
	UAbilityTask_WaitForInteractableTargets_SingleLineTrace* MyObj = NewAbilityTask<UAbilityTask_WaitForInteractableTargets_SingleLineTrace>(OwningAbility);
	MyObj->InteractionScanRange = InteractionScanRange;
	MyObj->InteractionScanRate = InteractionScanRate;
	MyObj->StartLocation = StartLocation;
	MyObj->InteractionQuery = InteractionQuery;
	MyObj->TraceProfile = TraceProfile;
	MyObj->bShowDebug = bShowDebug;

	return MyObj;
}

void UAbilityTask_WaitForInteractableTargets_SingleLineTrace::Activate()
{
	SetWaitingOnAvatar();

	PerformTrace();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::PerformTrace, InteractionScanRate, true);
	}
}

void UAbilityTask_WaitForInteractableTargets_SingleLineTrace::OnDestroy(bool AbilityEnded)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitForInteractableTargets_SingleLineTrace::PerformTrace()
{
	AActor* AvatarActor = Ability->GetCurrentActorInfo()->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	UWorld* World = GetWorld();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);

	const bool bTraceComplex = false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAbilityTask_WaitForInteractableTargets_SingleLineTrace), bTraceComplex);
	Params.AddIgnoredActors(ActorsToIgnore);

	FVector TraceStart = StartLocation.GetTargetingTransform().GetLocation();
	FVector TraceEnd;
	AimWithPlayerController(AvatarActor, Params, TraceStart, InteractionScanRange, OUT TraceEnd);

	TArray<FHitResult> HitResults;
	World->SweepMultiByProfile(
		HitResults,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		TraceProfile.Name,
		FCollisionShape::MakeSphere(SweepRadius),
		Params);

	TOptional<FInteractionOption> BestOption;
	const FVector ViewDirection = (TraceEnd - TraceStart).GetSafeNormal();
	for (const FHitResult& HitResult : HitResults)
	{
		TArray<TScriptInterface<IInteractableTarget>> InteractableTargets;
		UInteractionStatics::AppendInteractableTargetsFromHitResult(HitResult, InteractableTargets);

		FInteractionQuery Query = InteractionQuery;
		Query.QueryMode = ERpgInteractionQueryMode::Focus;
		Query.QueryOrigin = AvatarActor->GetActorLocation();
		Query.QueryRadius = InteractionScanRange;
		Query.CandidateHit = HitResult;
		TArray<FInteractionOption> HitOptions;
		GatherInteractableOptions(Query, InteractableTargets, HitOptions);
		for (FInteractionOption& Option : HitOptions)
		{
			const float Distance = FVector::Distance(Query.QueryOrigin, Option.GetInteractionWorldLocation());
			const bool bHasLineOfSight = UInteractionStatics::HasInteractionLineOfSight(AvatarActor, Option);
			Option.PromptState = UInteractionStatics::DeterminePromptState(
				Option,
				Distance,
				true,
				Option.Availability == ERpgInteractionAvailability::Available,
				bHasLineOfSight);
			if (Distance <= Option.Prompt.FocusRange &&
				(!BestOption.IsSet() || UInteractionStatics::IsBetterFocusCandidate(
					Option, BestOption.GetValue(), TraceStart, ViewDirection)))
			{
				BestOption = MoveTemp(Option);
			}
		}
	}

	TArray<FInteractionOption> SelectedOptions;
	if (BestOption.IsSet())
	{
		SelectedOptions.Add(MoveTemp(BestOption.GetValue()));
	}
	CommitInteractableOptions(MoveTemp(SelectedOptions));

#if ENABLE_DRAW_DEBUG
	if (bShowDebug)
	{
		const bool bHasHit = !HitResults.IsEmpty();
		FColor DebugColor = bHasHit ? FColor::Red : FColor::Green;
		if (bHasHit)
		{
			DrawDebugLine(World, TraceStart, HitResults[0].Location, DebugColor, false, InteractionScanRate);
			DrawDebugSphere(World, HitResults[0].Location, SweepRadius, 16, DebugColor, false, InteractionScanRate);
		}
		else
		{
			DrawDebugLine(World, TraceStart, TraceEnd, DebugColor, false, InteractionScanRate);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

