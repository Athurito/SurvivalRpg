// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitForInteractableTargets_FocusSweep.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_WaitForInteractableTargets_FocusSweep)

namespace
{
	TAutoConsoleVariable<int32> CVarRpgInteractionDebugFocus(
		TEXT("r.Rpg.Interaction.DebugFocus"),
		0,
		TEXT("Draw local/server interaction focus sweeps and the winning point."),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarRpgInteractionDebugSelectionScores(
		TEXT("r.Rpg.Interaction.DebugSelectionScores"),
		0,
		TEXT("Draw priority, view dot, normalized distance, and instance index for focus candidates."),
		ECVF_Cheat);
}

UAbilityTask_WaitForInteractableTargets_FocusSweep::UAbilityTask_WaitForInteractableTargets_FocusSweep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitForInteractableTargets_FocusSweep* UAbilityTask_WaitForInteractableTargets_FocusSweep::WaitForInteractableTargets_FocusSweep(
	UGameplayAbility* OwningAbility,
	FInteractionQuery InInteractionQuery,
	FGameplayAbilityTargetingLocationInfo InStartLocation,
	TEnumAsByte<ECollisionChannel> InInteractionTraceChannel,
	float InMaxFocusRange,
	float InScanRate,
	float InSweepRadius,
	int32 InMaxCandidates,
	bool bInShowDebug)
{
	ThisClass* Task = NewAbilityTask<ThisClass>(OwningAbility);
	Task->InteractionQuery = MoveTemp(InInteractionQuery);
	Task->StartLocation = MoveTemp(InStartLocation);
	Task->InteractionTraceChannel = InInteractionTraceChannel;
	Task->MaxFocusRange = FMath::Max(0.0f, InMaxFocusRange);
	Task->ScanRate = FMath::Max(0.01f, InScanRate);
	Task->SweepRadius = FMath::Max(0.0f, InSweepRadius);
	Task->MaxCandidates = FMath::Max(1, InMaxCandidates);
	Task->bShowDebug = bInShowDebug;
	return Task;
}

void UAbilityTask_WaitForInteractableTargets_FocusSweep::Activate()
{
	SetWaitingOnAvatar();
	PerformSweep();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::PerformSweep, ScanRate, true);
	}
}

void UAbilityTask_WaitForInteractableTargets_FocusSweep::OnDestroy(bool AbilityEnded)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitForInteractableTargets_FocusSweep::ScanNow()
{
	PerformSweep();
}

bool UAbilityTask_WaitForInteractableTargets_FocusSweep::GetFocusedOption(FInteractionOption& OutOption) const
{
	if (CurrentOptions.IsEmpty())
	{
		return false;
	}
	OutOption = CurrentOptions[0];
	return true;
}

void UAbilityTask_WaitForInteractableTargets_FocusSweep::PerformSweep()
{
	AActor* AvatarActor = GetAvatarActor();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World || !Ability)
	{
		CommitInteractableOptions(TArray<FInteractionOption>());
		return;
	}

	FVector ViewOrigin = StartLocation.GetTargetingTransform().GetLocation();
	FRotator ViewRotation = AvatarActor->GetActorRotation();
	if (const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo())
	{
		if (APlayerController* PlayerController = ActorInfo->PlayerController.Get())
		{
			PlayerController->GetPlayerViewPoint(ViewOrigin, ViewRotation);
		}
		else
		{
			AvatarActor->GetActorEyesViewPoint(ViewOrigin, ViewRotation);
		}
	}
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceEnd = ViewOrigin + ViewDirection * MaxFocusRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgInteractionFocusSweep), false, AvatarActor);
	if (Ability->GetCurrentActorInfo() && Ability->GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		Params.AddIgnoredActor(Ability->GetCurrentActorInfo()->OwnerActor.Get());
	}

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		ViewOrigin,
		TraceEnd,
		FQuat::Identity,
		InteractionTraceChannel,
		FCollisionShape::MakeSphere(SweepRadius),
		Params);
	Hits.StableSort([](const FHitResult& A, const FHitResult& B) { return A.Distance < B.Distance; });

	TOptional<FInteractionOption> BestOption;
	int32 ConsideredCandidates = 0;
	for (const FHitResult& Hit : Hits)
	{
		if (ConsideredCandidates >= MaxCandidates)
		{
			break;
		}
		TArray<TScriptInterface<IInteractableTarget>> Targets;
		UInteractionStatics::AppendInteractableTargetsFromHitResult(Hit, Targets);
		if (Targets.IsEmpty())
		{
			continue;
		}

		FInteractionQuery Query = InteractionQuery;
		Query.QueryMode = ERpgInteractionQueryMode::Focus;
		Query.QueryOrigin = AvatarActor->GetActorLocation();
		Query.QueryRadius = MaxFocusRange;
		Query.CandidateHit = Hit;
		TArray<FInteractionOption> HitOptions;
		GatherInteractableOptions(Query, Targets, HitOptions);
		for (FInteractionOption& Option : HitOptions)
		{
			if (ConsideredCandidates >= MaxCandidates)
			{
				break;
			}
			++ConsideredCandidates;
			const float Distance = FVector::Distance(AvatarActor->GetActorLocation(), Option.GetInteractionWorldLocation());
			const bool bHasLineOfSight = UInteractionStatics::HasInteractionLineOfSight(AvatarActor, Option);
			Option.PromptState = UInteractionStatics::DeterminePromptState(
				Option,
				Distance,
				true,
				Option.Availability == ERpgInteractionAvailability::Available,
				bHasLineOfSight);
#if ENABLE_DRAW_DEBUG
			if (CVarRpgInteractionDebugSelectionScores.GetValueOnGameThread() != 0)
			{
				const FVector CandidateDelta = Option.GetInteractionWorldLocation() - ViewOrigin;
				const float ViewDot = FVector::DotProduct(
					ViewDirection,
					CandidateDelta.GetSafeNormal());
				const float NormalizedDistance = CandidateDelta.Size() /
					FMath::Max(Option.Prompt.FocusRange, 1.0f);
				DrawDebugString(
					World,
					Option.GetInteractionWorldLocation() + FVector(0.0, 0.0, 25.0),
					FString::Printf(
						TEXT("P:%d Dot:%.3f ND:%.3f Inst:%d"),
						Option.Prompt.InteractionPriority,
						ViewDot,
						NormalizedDistance,
						Option.TargetRef.InstanceIndex),
					nullptr,
					Option.Availability == ERpgInteractionAvailability::Available
						? FColor::White
						: FColor::Orange,
					ScanRate,
					true);
			}
#endif
			if (Option.Availability == ERpgInteractionAvailability::Hidden ||
				Distance > Option.Prompt.FocusRange)
			{
				continue;
			}
			if (!BestOption.IsSet() || UInteractionStatics::IsBetterFocusCandidate(Option, BestOption.GetValue(), ViewOrigin, ViewDirection))
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
	if (bShowDebug || CVarRpgInteractionDebugFocus.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(World, ViewOrigin, TraceEnd, CurrentOptions.IsEmpty() ? FColor::Yellow : FColor::Green, false, ScanRate);
		if (!CurrentOptions.IsEmpty())
		{
			DrawDebugSphere(World, CurrentOptions[0].GetInteractionWorldLocation(), SweepRadius * 1.5f, 16, FColor::Green, false, ScanRate);
		}
	}
#endif
}
