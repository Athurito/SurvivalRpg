// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "InteractionTypes.h"

#include "InteractionStatics.generated.h"

template <typename InterfaceType> class TScriptInterface;

class AActor;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class URpgInteractionPromptAnchorComponent;
class IInteractableTarget;
class UObject;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;
struct FInteractionOption;
struct FInteractionQuery;
struct FFrame;
struct FHitResult;
struct FOverlapResult;

/** Shared target discovery, deterministic selection, payload, and server-validation helpers. */
UCLASS()
class SURVIVALRPG_API UInteractionStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UInteractionStatics();

public:
	/** Resolves the owning actor for an actor- or component-backed interaction provider. */
	UFUNCTION(BlueprintCallable)
	static AActor* GetActorFromInteractableTarget(TScriptInterface<IInteractableTarget> InteractableTarget);

	/** Appends the actor and any native interactable components it owns. */
	UFUNCTION(BlueprintCallable)
	static void GetInteractableTargetsFromActor(AActor* Actor, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets);

	static void AppendInteractableTargetsFromOverlapResults(const TArray<FOverlapResult>& OverlapResults, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets);
	static void AppendInteractableTargetsFromHitResult(const FHitResult& HitResult, TArray<TScriptInterface<IInteractableTarget>>& OutInteractableTargets);

	/** Fills runtime target data, deprecated prompt fallbacks, and default tags for a gathered option. */
	static void NormalizeInteractionOption(const FInteractionQuery& Query, TScriptInterface<IInteractableTarget> InteractableTarget, FInteractionOption& InOutOption);

	/** Builds the GAS event payload consumed by both generic and specialized interaction abilities. */
	static bool BuildInteractionEventData(const FInteractionOption& Option, AActor* Instigator, UAbilitySystemComponent* InstigatorAbilitySystem, FGameplayEventData& OutPayload);

	/** Re-gathers and validates the exact option represented by a server-side GAS event payload. */
	static bool ValidateInteractionEventData(const FGameplayAbilityActorInfo& ActorInfo, const FGameplayEventData* TriggerEventData, FInteractionOption& OutValidatedOption, FInteractionQuery& OutAuthoritativeQuery, FText& OutFailureReason);

	/** Computes the local presentation state using the canonical range and availability precedence. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Interaction")
	static ERpgInteractionPromptState DeterminePromptState(const FInteractionOption& Option, float Distance, bool bFocused, bool bAbilityAvailable, bool bHasLineOfSight);

	/** Returns true when Candidate should replace Current under deterministic priority/view/distance scoring. */
	static bool IsBetterFocusCandidate(const FInteractionOption& Candidate, const FInteractionOption& Current, const FVector& ViewOrigin, const FVector& ViewDirection);

	/** Builds the complete stable identity used for gameplay target comparison and deterministic focus ties. */
	static FString MakeStableOptionKey(const FInteractionOption& Option);

	/**
	 * Builds the cosmetic identity used to reconcile world prompts without depending on an incidental collision component.
	 * Gameplay validation and payloads must continue to use MakeStableOptionKey and the complete target reference.
	 */
	static FString MakePresentationOptionKey(const FInteractionOption& Option);

	/**
	 * Resolves the option's designer-authored cosmetic anchor, choosing duplicate ids deterministically by component name.
	 * Returns null when the target has no matching anchor; callers then apply their presentation-only fallback.
	 */
	static URpgInteractionPromptAnchorComponent* FindPromptAnchorComponent(const FInteractionOption& Option);

	/** Performs the shared visibility check used by focus and authority validation. */
	static bool HasInteractionLineOfSight(const AActor* RequestingActor, const FInteractionOption& Option);

	/** Emits a server-local interaction lifecycle message through GameplayMessageSubsystem. */
	static void BroadcastInteractionMessage(UObject* WorldContextObject, const FGameplayTag& Channel, const FInteractionOption& Option, AActor* Instigator, bool bSucceeded, const FGameplayTag& ResultTag = FGameplayTag());
};
