// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitForInteractableTargets_Nearby.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPresentation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_WaitForInteractableTargets_Nearby)

namespace
{
	TAutoConsoleVariable<int32> CVarRpgInteractionDebugNearby(
		TEXT("r.Rpg.Interaction.DebugNearby"),
		0,
		TEXT("Draw the local interaction awareness sphere."),
		ECVF_Cheat);
}

UAbilityTask_WaitForInteractableTargets_Nearby::UAbilityTask_WaitForInteractableTargets_Nearby(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitForInteractableTargets_Nearby* UAbilityTask_WaitForInteractableTargets_Nearby::WaitForInteractableTargets_Nearby(
	UGameplayAbility* OwningAbility,
	FInteractionQuery InInteractionQuery,
	TEnumAsByte<ECollisionChannel> InInteractionTraceChannel,
	float InScanRange,
	float InScanRate,
	int32 InMaxVisibleOptions,
	bool bInShowDebug)
{
	ThisClass* Task = NewAbilityTask<ThisClass>(OwningAbility);
	Task->InteractionQuery = MoveTemp(InInteractionQuery);
	Task->InteractionTraceChannel = InInteractionTraceChannel;
	Task->ScanRange = FMath::Max(0.0f, InScanRange);
	Task->ScanRate = FMath::Max(0.05f, InScanRate);
	Task->MaxVisibleOptions = FMath::Max(1, InMaxVisibleOptions);
	Task->bShowDebug = bInShowDebug;
	return Task;
}

void UAbilityTask_WaitForInteractableTargets_Nearby::Activate()
{
	SetWaitingOnAvatar();
	QueryNearby();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::QueryNearby, ScanRate, true);
	}
}

void UAbilityTask_WaitForInteractableTargets_Nearby::OnDestroy(bool AbilityEnded)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitForInteractableTargets_Nearby::QueryNearby()
{
	AActor* AvatarActor = GetAvatarActor();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World)
	{
		CommitInteractableOptions(TArray<FInteractionOption>());
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgInteractionNearby), false, AvatarActor);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		AvatarActor->GetActorLocation(),
		FQuat::Identity,
		InteractionTraceChannel,
		FCollisionShape::MakeSphere(ScanRange),
		Params);

	TArray<FInteractionOption> NewOptions;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!Overlap.GetActor() && !Overlap.GetComponent())
		{
			continue;
		}
		TArray<TScriptInterface<IInteractableTarget>> Targets;
		TArray<FOverlapResult> SingleOverlap{Overlap};
		UInteractionStatics::AppendInteractableTargetsFromOverlapResults(SingleOverlap, Targets);
		if (Targets.IsEmpty())
		{
			continue;
		}

		FInteractionQuery Query = InteractionQuery;
		Query.QueryMode = ERpgInteractionQueryMode::Nearby;
		Query.QueryOrigin = AvatarActor->GetActorLocation();
		Query.QueryRadius = ScanRange;
		const FVector CandidateLocation = Overlap.GetComponent()
			? Overlap.GetComponent()->Bounds.Origin
			: Overlap.GetActor()->GetActorLocation();
		Query.CandidateHit = FHitResult(
			Overlap.GetActor(),
			Overlap.GetComponent(),
			CandidateLocation,
			FVector::UpVector);
		GatherInteractableOptions(Query, Targets, NewOptions);
	}

	NewOptions.RemoveAll([AvatarActor](const FInteractionOption& Option)
	{
		return Option.Availability == ERpgInteractionAvailability::Hidden ||
			!Option.Prompt.bShowNearbyIndicator ||
			FVector::Distance(AvatarActor->GetActorLocation(), Option.GetInteractionWorldLocation()) > Option.Prompt.AwarenessRange;
	});
	RpgInteractionPresentation::SelectNearbyOptionsForDisplay(
		NewOptions,
		AvatarActor->GetActorLocation(),
		MaxVisibleOptions);
	for (FInteractionOption& Option : NewOptions)
	{
		Option.PromptState = ERpgInteractionPromptState::Nearby;
	}
	CommitInteractableOptions(MoveTemp(NewOptions));

#if ENABLE_DRAW_DEBUG
	if (bShowDebug || CVarRpgInteractionDebugNearby.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(World, AvatarActor->GetActorLocation(), ScanRange, 32, FColor::Cyan, false, ScanRate);
	}
#endif
}
