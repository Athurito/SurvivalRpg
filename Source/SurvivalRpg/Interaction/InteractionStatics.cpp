// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionStatics.h"

#include "Components/PrimitiveComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "IInteractableTarget.h"
#include "InteractionMessages.h"
#include "InteractionOption.h"
#include "InteractionQuery.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InteractionStatics)

DEFINE_LOG_CATEGORY_STATIC(LogRpgInteraction, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarRpgInteractionDebugServerRejections(
		TEXT("r.Rpg.Interaction.DebugServerRejections"),
		0,
		TEXT("Log rejected authoritative interactions and draw their reconstructed target point."),
		ECVF_Cheat);

	void DebugServerInteractionRejection(
		const AActor* RequestingActor,
		const AActor* RequestedTarget,
		const FGameplayTag& RequestedTag,
		const FText& FailureReason,
		const FHitResult* ReconstructedHit)
	{
#if !UE_BUILD_SHIPPING
		if (CVarRpgInteractionDebugServerRejections.GetValueOnGameThread() == 0)
		{
			return;
		}

		UE_LOG(
			LogRpgInteraction,
			Warning,
			TEXT("Server rejected interaction requester=%s target=%s tag=%s reason=%s"),
			*GetNameSafe(RequestingActor),
			*GetNameSafe(RequestedTarget),
			*RequestedTag.ToString(),
			*FailureReason.ToString());

#if ENABLE_DRAW_DEBUG
		const UWorld* World = RequestingActor
			? RequestingActor->GetWorld()
			: RequestedTarget
				? RequestedTarget->GetWorld()
				: nullptr;
		if (World)
		{
			const FVector RejectionLocation = ReconstructedHit
				? FVector(ReconstructedHit->ImpactPoint)
				: RequestedTarget
					? RequestedTarget->GetActorLocation()
					: RequestingActor->GetActorLocation();
			DrawDebugSphere(World, RejectionLocation, 18.0f, 12, FColor::Red, false, 2.0f);
			DrawDebugString(
				World,
				RejectionLocation + FVector(0.0, 0.0, 30.0),
				FailureReason.ToString(),
				nullptr,
				FColor::Red,
				2.0f,
				true);
		}
#endif
#endif
	}
}

UInteractionStatics::UInteractionStatics()
	: Super(FObjectInitializer::Get())
{
}

AActor* UInteractionStatics::GetActorFromInteractableTarget(TScriptInterface<IInteractableTarget> InteractableTarget)
{
	if (UObject* Object = InteractableTarget.GetObject())
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			return Actor;
		}
		else if (UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
		{
			return ActorComponent->GetOwner();
		}
		else
		{
			return nullptr;
		}
	}

	return nullptr;
}

void UInteractionStatics::GetInteractableTargetsFromActor(AActor* Actor, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets)
{
	if (!Actor)
	{
		return;
	}

	// If the actor is directly interactable, return that.
	TScriptInterface<IInteractableTarget> InteractableActor(Actor);
	if (InteractableActor)
	{
		OutInteractableTargets.AddUnique(InteractableActor);
	}

	// If the actor isn't interactable, it might have a component that has a interactable interface.
	TArray<UActorComponent*> InteractableComponents = Actor->GetComponentsByInterface(UInteractableTarget::StaticClass());
	for (UActorComponent* InteractableComponent : InteractableComponents)
	{
		OutInteractableTargets.AddUnique(TScriptInterface<IInteractableTarget>(InteractableComponent));
	}
}

void UInteractionStatics::NormalizeInteractionOption(
	const FInteractionQuery& Query,
	TScriptInterface<IInteractableTarget> InteractableTarget,
	FInteractionOption& InOutOption)
{
	InOutOption.InteractableTarget = InteractableTarget;
	InOutOption.NormalizeLegacyFields();
	if (!InOutOption.InteractionTag.IsValid())
	{
		InOutOption.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	}

	if (!InOutOption.TargetRef.TargetActor.IsValid())
	{
		InOutOption.TargetRef.TargetActor = GetActorFromInteractableTarget(InteractableTarget);
	}
	if (!InOutOption.TargetRef.TargetComponent.IsValid())
	{
		InOutOption.TargetRef.TargetComponent = Query.CandidateHit.GetComponent();
	}
	if (InOutOption.TargetRef.InstanceIndex == INDEX_NONE)
	{
		InOutOption.TargetRef.InstanceIndex = Query.CandidateHit.Item;
	}
	if (InOutOption.TargetRef.WorldLocation.IsNearlyZero())
	{
		if (Query.CandidateHit.bBlockingHit || Query.CandidateHit.GetActor() || Query.CandidateHit.GetComponent())
		{
			InOutOption.TargetRef.WorldLocation = Query.CandidateHit.ImpactPoint.IsNearlyZero()
				? Query.CandidateHit.Location
				: Query.CandidateHit.ImpactPoint;
		}
		else if (const AActor* TargetActor = InOutOption.TargetRef.TargetActor.Get())
		{
			InOutOption.TargetRef.WorldLocation = TargetActor->GetActorLocation();
		}
	}
	if (!Query.CandidateHit.ImpactNormal.IsNearlyZero())
	{
		InOutOption.TargetRef.WorldNormal = Query.CandidateHit.ImpactNormal;
	}
}

bool UInteractionStatics::BuildInteractionEventData(
	const FInteractionOption& Option,
	AActor* Instigator,
	UAbilitySystemComponent* InstigatorAbilitySystem,
	FGameplayEventData& OutPayload)
{
	AActor* TargetActor = Option.TargetRef.TargetActor.Get();
	if (!TargetActor)
	{
		TargetActor = GetActorFromInteractableTarget(Option.InteractableTarget);
	}
	if (!Instigator || !TargetActor || !Option.InteractableTarget || !Option.InteractionTag.IsValid())
	{
		return false;
	}

	OutPayload = FGameplayEventData();
	OutPayload.EventTag = RpgGameplayTags::Ability_Interaction_Activate;
	OutPayload.Instigator = Instigator;
	OutPayload.Target = TargetActor;
	OutPayload.OptionalObject2 = Option.InteractableTarget.GetObject();
	OutPayload.TargetTags.AddTag(Option.InteractionTag);
	OutPayload.EventMagnitude = static_cast<float>(Option.TargetRef.Revision);

	if (InstigatorAbilitySystem)
	{
		FGameplayEffectContextHandle Context = InstigatorAbilitySystem->MakeEffectContext();
		FHitResult Hit(TargetActor, Option.TargetRef.TargetComponent.Get(), Option.GetInteractionWorldLocation(), Option.TargetRef.WorldNormal);
		Hit.Item = Option.TargetRef.InstanceIndex;
		Hit.ImpactPoint = Option.GetInteractionWorldLocation();
		Hit.Location = Hit.ImpactPoint;
		Context.AddHitResult(Hit, true);
		OutPayload.ContextHandle = MoveTemp(Context);
	}

	Option.InteractableTarget->CustomizeInteractionEventData(RpgGameplayTags::Ability_Interaction_Activate, OutPayload);
	return true;
}

namespace
{
	FGameplayTag FindInteractionActionTag(const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (Tag != RpgGameplayTags::Rpg_Interaction_Action && Tag.MatchesTag(RpgGameplayTags::Rpg_Interaction_Action))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}
}

bool UInteractionStatics::ValidateInteractionEventData(
	const FGameplayAbilityActorInfo& ActorInfo,
	const FGameplayEventData* TriggerEventData,
	FInteractionOption& OutValidatedOption,
	FInteractionQuery& OutAuthoritativeQuery,
	FText& OutFailureReason)
{
	OutValidatedOption = FInteractionOption();
	OutAuthoritativeQuery = FInteractionQuery();
	OutFailureReason = FText::GetEmpty();

	AActor* RequestingActor = ActorInfo.AvatarActor.Get();
	if (!ActorInfo.IsNetAuthority() || !RequestingActor || !TriggerEventData)
	{
		OutFailureReason = NSLOCTEXT("RpgInteraction", "AuthorityRequired", "Server validation required");
		DebugServerInteractionRejection(
			RequestingActor,
			nullptr,
			FGameplayTag(),
			OutFailureReason,
			nullptr);
		return false;
	}

	const FGameplayTag RequestedTag = FindInteractionActionTag(TriggerEventData->TargetTags);
	const UObject* SourceObject = TriggerEventData->OptionalObject2;
	AActor* RequestedTargetActor = const_cast<AActor*>(ToRawPtr(TriggerEventData->Target));
	if (!RequestedTag.IsValid() || !RequestedTargetActor)
	{
		OutFailureReason = NSLOCTEXT("RpgInteraction", "InvalidInteractionRequest", "Invalid interaction target");
		DebugServerInteractionRejection(
			RequestingActor,
			RequestedTargetActor,
			RequestedTag,
			OutFailureReason,
			TriggerEventData->ContextHandle.GetHitResult());
		return false;
	}
	AActor* RequestedSourceActor = nullptr;
	if (SourceObject && SourceObject->GetClass()->ImplementsInterface(UInteractableTarget::StaticClass()))
	{
		RequestedSourceActor = GetActorFromInteractableTarget(
			TScriptInterface<IInteractableTarget>(const_cast<UObject*>(SourceObject)));
	}
	if (!RequestedSourceActor)
	{
		RequestedSourceActor = RequestedTargetActor;
	}

	OutAuthoritativeQuery.RequestingAvatar = RequestingActor;
	OutAuthoritativeQuery.RequestingController = ActorInfo.PlayerController.Get();
	OutAuthoritativeQuery.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
	OutAuthoritativeQuery.QueryOrigin = RequestingActor->GetActorLocation();
	if (const FHitResult* Hit = TriggerEventData->ContextHandle.GetHitResult())
	{
		OutAuthoritativeQuery.CandidateHit = *Hit;
	}
	if (!OutAuthoritativeQuery.CandidateHit.GetActor())
	{
		OutAuthoritativeQuery.CandidateHit = FHitResult(
			RequestedSourceActor,
			Cast<UPrimitiveComponent>(const_cast<UObject*>(SourceObject)),
			RequestedSourceActor->GetActorLocation(),
			FVector::UpVector);
	}
	if (const UPrimitiveComponent* RequestedComponent = OutAuthoritativeQuery.CandidateHit.GetComponent();
		(RequestedComponent && RequestedComponent->GetOwner() != RequestedSourceActor) ||
		(OutAuthoritativeQuery.CandidateHit.Item != INDEX_NONE && !RequestedComponent))
	{
		OutFailureReason = NSLOCTEXT("RpgInteraction", "InvalidInteractionComponent", "Invalid interaction component");
		DebugServerInteractionRejection(
			RequestingActor,
			RequestedTargetActor,
			RequestedTag,
			OutFailureReason,
			&OutAuthoritativeQuery.CandidateHit);
		return false;
	}

	TArray<TScriptInterface<IInteractableTarget>> Targets;
	if (SourceObject && SourceObject->GetClass()->ImplementsInterface(UInteractableTarget::StaticClass()))
	{
		Targets.Add(TScriptInterface<IInteractableTarget>(const_cast<UObject*>(SourceObject)));
	}
	else
	{
		AppendInteractableTargetsFromHitResult(OutAuthoritativeQuery.CandidateHit, Targets);
		if (Targets.IsEmpty())
		{
			GetInteractableTargetsFromActor(RequestedTargetActor, Targets);
		}
	}

	for (const TScriptInterface<IInteractableTarget>& Target : Targets)
	{
		TArray<FInteractionOption> Options;
		FInteractionOptionBuilder Builder(Target, Options);
		Target->GatherInteractionOptions(OutAuthoritativeQuery, Builder);
		for (FInteractionOption& Option : Options)
		{
			NormalizeInteractionOption(OutAuthoritativeQuery, Target, Option);
			if (!Option.TargetRef.IsValid() || Option.InteractionTag != RequestedTag ||
				Option.Availability != ERpgInteractionAvailability::Available)
			{
				continue;
			}
			if (Option.TargetRef.TargetActor.Get() != RequestedSourceActor)
			{
				continue;
			}
			if (const UPrimitiveComponent* RequestedComponent = OutAuthoritativeQuery.CandidateHit.GetComponent();
				RequestedComponent && Option.TargetRef.TargetComponent.Get() != RequestedComponent)
			{
				continue;
			}
			if (OutAuthoritativeQuery.CandidateHit.Item != INDEX_NONE && Option.TargetRef.InstanceIndex != OutAuthoritativeQuery.CandidateHit.Item)
			{
				continue;
			}
			if (Option.TargetRef.Revision != INDEX_NONE &&
				Option.TargetRef.Revision != FMath::RoundToInt(TriggerEventData->EventMagnitude))
			{
				continue;
			}

			UAbilitySystemComponent* TargetAbilitySystem = Option.TargetAbilitySystem;
			FGameplayAbilitySpec* InteractionAbilitySpec = nullptr;
			if (TargetAbilitySystem && Option.TargetInteractionAbilityHandle.IsValid())
			{
				InteractionAbilitySpec = TargetAbilitySystem->FindAbilitySpecFromHandle(
					Option.TargetInteractionAbilityHandle);
			}
			else if (Option.InteractionAbilityToGrant && ActorInfo.AbilitySystemComponent.IsValid())
			{
				TargetAbilitySystem = ActorInfo.AbilitySystemComponent.Get();
				InteractionAbilitySpec = TargetAbilitySystem->FindAbilitySpecFromClass(
					Option.InteractionAbilityToGrant);
				if (InteractionAbilitySpec)
				{
					Option.TargetAbilitySystem = TargetAbilitySystem;
					Option.TargetInteractionAbilityHandle = InteractionAbilitySpec->Handle;
				}
			}
			if (!InteractionAbilitySpec || !InteractionAbilitySpec->Ability || !TargetAbilitySystem ||
				!TargetAbilitySystem->AbilityActorInfo.IsValid())
			{
				continue;
			}
			// An interaction ability revalidates from inside ActivateAbility, where its spec is
			// already active. Inactive specs must still pass the same GAS gate used before dispatch.
			if (!InteractionAbilitySpec->IsActive() &&
				!InteractionAbilitySpec->Ability->CanActivateAbility(
					InteractionAbilitySpec->Handle,
					TargetAbilitySystem->AbilityActorInfo.Get()))
			{
				continue;
			}

			const float Distance = FVector::Distance(RequestingActor->GetActorLocation(), Option.GetInteractionWorldLocation());
			if (Distance > Option.Prompt.InteractionRange || !HasInteractionLineOfSight(RequestingActor, Option))
			{
				continue;
			}

			OutValidatedOption = MoveTemp(Option);
			OutAuthoritativeQuery.QueryRadius = OutValidatedOption.Prompt.InteractionRange;
			return true;
		}
	}

	OutFailureReason = NSLOCTEXT("RpgInteraction", "InteractionNoLongerAvailable", "Interaction is no longer available");
	DebugServerInteractionRejection(
		RequestingActor,
		RequestedTargetActor,
		RequestedTag,
		OutFailureReason,
		&OutAuthoritativeQuery.CandidateHit);
	return false;
}

ERpgInteractionPromptState UInteractionStatics::DeterminePromptState(
	const FInteractionOption& Option,
	float Distance,
	bool bFocused,
	bool bAbilityAvailable,
	bool bHasLineOfSight)
{
	if (Option.Availability == ERpgInteractionAvailability::Hidden || Distance > Option.Prompt.AwarenessRange)
	{
		return ERpgInteractionPromptState::Hidden;
	}
	if (!bFocused || Distance > Option.Prompt.FocusRange)
	{
		return Option.Prompt.bShowNearbyIndicator ? ERpgInteractionPromptState::Nearby : ERpgInteractionPromptState::Hidden;
	}
	if (Distance > Option.Prompt.InteractionRange)
	{
		return ERpgInteractionPromptState::FocusedOutOfRange;
	}
	if (Option.Availability == ERpgInteractionAvailability::Blocked || !bAbilityAvailable || (Option.Prompt.bRequiresLineOfSight && !bHasLineOfSight))
	{
		return ERpgInteractionPromptState::Blocked;
	}
	return ERpgInteractionPromptState::Ready;
}

bool UInteractionStatics::IsBetterFocusCandidate(
	const FInteractionOption& Candidate,
	const FInteractionOption& Current,
	const FVector& ViewOrigin,
	const FVector& ViewDirection)
{
	if (Candidate.Prompt.InteractionPriority != Current.Prompt.InteractionPriority)
	{
		return Candidate.Prompt.InteractionPriority > Current.Prompt.InteractionPriority;
	}

	const FVector CandidateDelta = Candidate.GetInteractionWorldLocation() - ViewOrigin;
	const FVector CurrentDelta = Current.GetInteractionWorldLocation() - ViewOrigin;
	const float CandidateDot = FVector::DotProduct(ViewDirection, CandidateDelta.GetSafeNormal());
	const float CurrentDot = FVector::DotProduct(ViewDirection, CurrentDelta.GetSafeNormal());
	if (!FMath::IsNearlyEqual(CandidateDot, CurrentDot, KINDA_SMALL_NUMBER))
	{
		return CandidateDot > CurrentDot;
	}
	const float CandidateDistanceScore = CandidateDelta.Size() / FMath::Max(Candidate.Prompt.FocusRange, 1.0f);
	const float CurrentDistanceScore = CurrentDelta.Size() / FMath::Max(Current.Prompt.FocusRange, 1.0f);
	if (!FMath::IsNearlyEqual(CandidateDistanceScore, CurrentDistanceScore, KINDA_SMALL_NUMBER))
	{
		return CandidateDistanceScore < CurrentDistanceScore;
	}

	return MakeStableOptionKey(Candidate) < MakeStableOptionKey(Current);
}

FString UInteractionStatics::MakeStableOptionKey(const FInteractionOption& Option)
{
	const UObject* TargetObject = Option.TargetRef.TargetActor.IsValid()
		? static_cast<const UObject*>(Option.TargetRef.TargetActor.Get())
		: Option.InteractableTarget.GetObject();
	return FString::Printf(
		TEXT("%s|%s|%d|%s"),
		*GetPathNameSafe(TargetObject),
		*GetPathNameSafe(Option.TargetRef.TargetComponent.Get()),
		Option.TargetRef.InstanceIndex,
		*Option.InteractionTag.ToString());
}

FString UInteractionStatics::MakePresentationOptionKey(const FInteractionOption& Option)
{
	const AActor* TargetActor = Option.TargetRef.TargetActor.Get();
	if (!TargetActor)
	{
		TargetActor = GetActorFromInteractableTarget(Option.InteractableTarget);
	}
	return FString::Printf(
		TEXT("%s|%s|%d|%s"),
		*GetPathNameSafe(Option.InteractableTarget.GetObject()),
		*GetPathNameSafe(TargetActor),
		Option.TargetRef.InstanceIndex,
		*Option.InteractionTag.ToString());
}

URpgInteractionPromptAnchorComponent* UInteractionStatics::FindPromptAnchorComponent(
	const FInteractionOption& Option)
{
	AActor* TargetActor = Option.TargetRef.TargetActor.Get();
	if (!TargetActor)
	{
		TargetActor = GetActorFromInteractableTarget(Option.InteractableTarget);
	}
	if (!TargetActor || Option.Prompt.PromptAnchorId.IsNone())
	{
		return nullptr;
	}

	TInlineComponentArray<URpgInteractionPromptAnchorComponent*> Anchors(TargetActor);
	Anchors.RemoveAll([&Option](const URpgInteractionPromptAnchorComponent* Anchor)
	{
		return !IsValid(Anchor) ||
			!Anchor->IsAvailableForPromptPlacement() ||
			Anchor->GetAnchorId() != Option.Prompt.PromptAnchorId;
	});
	Anchors.Sort([](
		const URpgInteractionPromptAnchorComponent& A,
		const URpgInteractionPromptAnchorComponent& B)
	{
		return A.GetName() < B.GetName();
	});

	return Anchors.IsEmpty() ? nullptr : Anchors[0];
}

bool UInteractionStatics::HasInteractionLineOfSight(const AActor* RequestingActor, const FInteractionOption& Option)
{
	if (!RequestingActor || !Option.Prompt.bRequiresLineOfSight)
	{
		return RequestingActor != nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	RequestingActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	if (const APawn* Pawn = Cast<APawn>(RequestingActor))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgInteractionLineOfSight), false, RequestingActor);
	FHitResult Hit;
	const bool bHit = RequestingActor->GetWorld()->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		Option.GetInteractionWorldLocation(),
		ECC_Visibility,
		Params);
	if (!bHit)
	{
		return true;
	}

	if (const UPrimitiveComponent* TargetComponent = Option.TargetRef.TargetComponent.Get();
		TargetComponent && Option.TargetRef.InstanceIndex != INDEX_NONE)
	{
		if (Hit.GetComponent() != TargetComponent)
		{
			return false;
		}
		return Hit.Item == Option.TargetRef.InstanceIndex;
	}

	return Hit.GetActor() == Option.TargetRef.TargetActor.Get() ||
		Hit.GetComponent() == Option.TargetRef.TargetComponent.Get();
}

void UInteractionStatics::BroadcastInteractionMessage(
	UObject* WorldContextObject,
	const FGameplayTag& Channel,
	const FInteractionOption& Option,
	AActor* Instigator,
	bool bSucceeded,
	const FGameplayTag& ResultTag)
{
	if (!WorldContextObject || !Channel.IsValid())
	{
		return;
	}
	FRpgInteractionExecutionMessage Message;
	Message.Instigator = Instigator;
	Message.TargetActor = Option.TargetRef.TargetActor.Get();
	Message.TargetComponent = Option.TargetRef.TargetComponent.Get();
	Message.InteractionTag = Option.InteractionTag;
	Message.InstanceIndex = Option.TargetRef.InstanceIndex;
	Message.ResultTag = ResultTag;
	Message.bSucceeded = bSucceeded;
	UGameplayMessageSubsystem::Get(WorldContextObject).BroadcastMessage(Channel, Message);
}

void UInteractionStatics::AppendInteractableTargetsFromOverlapResults(const TArray<FOverlapResult>& OverlapResults, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets)
{
	for (const FOverlapResult& Overlap : OverlapResults)
	{
		GetInteractableTargetsFromActor(Overlap.GetActor(), OutInteractableTargets);

		TScriptInterface<IInteractableTarget> InteractableComponent(Overlap.GetComponent());
		if (InteractableComponent)
		{
			OutInteractableTargets.AddUnique(InteractableComponent);
		}
	}
}

void UInteractionStatics::AppendInteractableTargetsFromHitResult(const FHitResult& HitResult, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets)
{
	GetInteractableTargetsFromActor(HitResult.GetActor(), OutInteractableTargets);

	TScriptInterface<IInteractableTarget> InteractableComponent(HitResult.GetComponent());
	if (InteractableComponent)
	{
		OutInteractableTargets.AddUnique(InteractableComponent);
	}
}
