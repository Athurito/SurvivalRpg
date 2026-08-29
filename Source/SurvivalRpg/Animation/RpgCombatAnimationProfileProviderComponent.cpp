// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgCombatAnimationProfileProviderComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "RpgAnimInstance.h"
#include "RpgCombatAnimationProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatAnimationProfileProviderComponent)

#define LOCTEXT_NAMESPACE "RpgCombatAnimationProfileProvider"

URpgCombatAnimationProfileProviderComponent::URpgCombatAnimationProfileProviderComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

const URpgCombatAnimationProfileProviderComponent*
URpgCombatAnimationProfileProviderComponent::FindForActor(const AActor* Actor)
{
	const URpgCombatAnimationProfileProviderComponent* Provider = Actor
		? Actor->FindComponentByClass<URpgCombatAnimationProfileProviderComponent>()
		: nullptr;
	return IsValid(Provider) && Provider->IsRegistered() ? Provider : nullptr;
}

void URpgCombatAnimationProfileProviderComponent::OnUnregister()
{
	// A hidden or culled mesh may not run another animation update before the GameFeature unloads.
	// Finish any worker evaluation and release every profile-owned animation reference while this
	// provider and its feature content are still alive.
	if (AActor* Owner = GetOwner())
	{
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Owner);
		for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
		{
			if (!IsValid(SkeletalMesh))
			{
				continue;
			}

			SkeletalMesh->HandleExistingParallelEvaluationTask(true, true);
			if (URpgAnimInstance* AnimInstance =
					Cast<URpgAnimInstance>(SkeletalMesh->GetAnimInstance()))
			{
				AnimInstance->HandleCombatAnimationProfileProviderUnregistering(this);
			}
		}
	}

	Super::OnUnregister();
}

#if WITH_EDITOR
EDataValidationResult URpgCombatAnimationProfileProviderComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (!CombatAnimationProfile)
	{
		Context.AddError(LOCTEXT(
			"MissingCombatAnimationProfile",
			"A combat-animation profile provider must bind a combat animation profile in its class defaults."));
		return EDataValidationResult::Invalid;
	}

	if (!CombatAnimationProfile->ValidateProfile().IsValid())
	{
		Context.AddError(LOCTEXT(
			"InvalidCombatAnimationProfile",
			"The bound combat animation profile has an invalid fallback, trait map, skeleton, mask, animation, or blend contract."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
