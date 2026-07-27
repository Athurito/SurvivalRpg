// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_GrantNearbyInteraction.h"

#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_GrantNearbyInteraction)

UAbilityTask_GrantNearbyInteraction::UAbilityTask_GrantNearbyInteraction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_GrantNearbyInteraction* UAbilityTask_GrantNearbyInteraction::GrantAbilitiesForNearbyInteractors(UGameplayAbility* OwningAbility, float InteractionScanRange, float InteractionScanRate)
{
	UAbilityTask_GrantNearbyInteraction* MyObj = NewAbilityTask<UAbilityTask_GrantNearbyInteraction>(OwningAbility);
	MyObj->InteractionScanRange = InteractionScanRange;
	MyObj->InteractionScanRate = InteractionScanRate;
	return MyObj;
}

#if WITH_DEV_AUTOMATION_TESTS
UAbilityTask_GrantNearbyInteraction* UAbilityTask_GrantNearbyInteraction::CreateForTesting(
	UAbilitySystemComponent* InAbilitySystemComponent,
	const float InInteractionScanRange)
{
	ThisClass* Task = NewObject<ThisClass>(InAbilitySystemComponent);
	Task->InitTask(
		*InAbilitySystemComponent,
		InAbilitySystemComponent->GetGameplayTaskDefaultPriority());
	Task->SetAbilitySystemComponent(InAbilitySystemComponent);
	Task->InteractionScanRange = InInteractionScanRange;
	return Task;
}

void UAbilityTask_GrantNearbyInteraction::ReconcileAbilityClassesForTesting(
	const TArray<TSubclassOf<UGameplayAbility>>& RequiredAbilityClasses)
{
	TMap<TSubclassOf<UGameplayAbility>, int32> DesiredAbilityReferenceCounts;
	for (const TSubclassOf<UGameplayAbility> AbilityClass : RequiredAbilityClasses)
	{
		if (AbilityClass)
		{
			DesiredAbilityReferenceCounts.FindOrAdd(AbilityClass)++;
		}
	}
	ReconcileAbilities(MoveTemp(DesiredAbilityReferenceCounts));
}

void UAbilityTask_GrantNearbyInteraction::StartQueryTimerForTesting()
{
	StartQueryTimer();
}

bool UAbilityTask_GrantNearbyInteraction::IsQueryTimerActiveForTesting() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(QueryTimerHandle);
}
#endif

void UAbilityTask_GrantNearbyInteraction::Activate()
{
	SetWaitingOnAvatar();
	StartQueryTimer();
}

void UAbilityTask_GrantNearbyInteraction::StartQueryTimer()
{
	QueryInteractables();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(QueryTimerHandle, this, &ThisClass::QueryInteractables, InteractionScanRate, true);
	}
}

void UAbilityTask_GrantNearbyInteraction::OnDestroy(bool AbilityEnded)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueryTimerHandle);
	}

	if (AbilitySystemComponent.IsValid() && AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		for (const TPair<TSubclassOf<UGameplayAbility>, FGameplayAbilitySpecHandle>& Entry : InteractionAbilityCache)
		{
			if (FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Entry.Value))
			{
				if (Spec->IsActive())
				{
					AbilitySystemComponent->SetRemoveAbilityOnEnd(Entry.Value);
				}
				else
				{
					AbilitySystemComponent->ClearAbility(Entry.Value);
				}
			}
		}
	}
	InteractionAbilityCache.Reset();
	InteractionAbilityReferenceCounts.Reset();

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_GrantNearbyInteraction::QueryInteractables()
{
	UWorld* World = GetWorld();
	AActor* ActorOwner = AbilitySystemComponent.IsValid()
		? AbilitySystemComponent->GetAvatarActor()
		: GetAvatarActor();
	
	if (World && ActorOwner && AbilitySystemComponent.IsValid() && AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UAbilityTask_GrantNearbyInteraction), false);

		TArray<FOverlapResult> OverlapResults;
		World->OverlapMultiByChannel(OUT OverlapResults, ActorOwner->GetActorLocation(), FQuat::Identity, Rpg_TraceChannel_Interaction, FCollisionShape::MakeSphere(InteractionScanRange), Params);

		TArray<TScriptInterface<IInteractableTarget>> InteractableTargets;
		UInteractionStatics::AppendInteractableTargetsFromOverlapResults(OverlapResults, OUT InteractableTargets);
			
		FInteractionQuery InteractionQuery;
		InteractionQuery.RequestingAvatar = ActorOwner;
		InteractionQuery.RequestingController = Cast<AController>(ActorOwner->GetOwner());
		if (!InteractionQuery.RequestingController.IsValid())
		{
			if (const APawn* Pawn = Cast<APawn>(ActorOwner))
			{
				InteractionQuery.RequestingController = Pawn->GetController();
			}
		}
		InteractionQuery.QueryMode = ERpgInteractionQueryMode::Nearby;
		InteractionQuery.QueryOrigin = ActorOwner->GetActorLocation();
		InteractionQuery.QueryRadius = InteractionScanRange;

		TArray<FInteractionOption> Options;
		for (TScriptInterface<IInteractableTarget>& InteractiveTarget : InteractableTargets)
		{
			FInteractionOptionBuilder InteractionBuilder(InteractiveTarget, Options);
			InteractiveTarget->GatherInteractionOptions(InteractionQuery, InteractionBuilder);
		}

		TMap<TSubclassOf<UGameplayAbility>, int32> DesiredAbilityReferenceCounts;
		for (const FInteractionOption& Option : Options)
		{
			if (Option.InteractionAbilityToGrant && Option.Availability != ERpgInteractionAvailability::Hidden)
			{
				DesiredAbilityReferenceCounts.FindOrAdd(Option.InteractionAbilityToGrant)++;
			}
		}

		ReconcileAbilities(MoveTemp(DesiredAbilityReferenceCounts));
	}
}

void UAbilityTask_GrantNearbyInteraction::ReconcileAbilities(
	TMap<TSubclassOf<UGameplayAbility>, int32> DesiredAbilityReferenceCounts)
{
	if (!AbilitySystemComponent.IsValid() || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	for (auto It = InteractionAbilityCache.CreateIterator(); It; ++It)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(It.Value());
		if (!Spec)
		{
			It.RemoveCurrent();
			continue;
		}
		if (DesiredAbilityReferenceCounts.Contains(It.Key()))
		{
			Spec->RemoveAfterActivation = false;
			continue;
		}
		if (Spec->IsActive())
		{
			AbilitySystemComponent->SetRemoveAbilityOnEnd(It.Value());
		}
		else
		{
			AbilitySystemComponent->ClearAbility(It.Value());
			It.RemoveCurrent();
		}
	}

	AActor* AbilitySourceActor = AbilitySystemComponent->GetAvatarActor();
	for (const TPair<TSubclassOf<UGameplayAbility>, int32>& DesiredEntry : DesiredAbilityReferenceCounts)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = DesiredEntry.Key;
		if (!InteractionAbilityCache.Contains(AbilityClass))
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, AbilitySourceActor);
			InteractionAbilityCache.Add(AbilityClass, AbilitySystemComponent->GiveAbility(Spec));
		}
	}
	InteractionAbilityReferenceCounts = MoveTemp(DesiredAbilityReferenceCounts);
}

