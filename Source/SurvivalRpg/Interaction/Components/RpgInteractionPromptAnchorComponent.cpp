// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionPromptAnchorComponent.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionPromptAnchorComponent)

URpgInteractionPromptAnchorComponent::URpgInteractionPromptAnchorComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetIsReplicatedByDefault(false);
	SetHiddenInGame(true);

#if WITH_EDITORONLY_DATA
	ArrowColor = FColor(80, 200, 255);
	ArrowSize = 0.5f;
	bIsScreenSizeScaled = true;
#endif
}

void URpgInteractionPromptAnchorComponent::OnComponentDestroyed(
	const bool bDestroyingHierarchy)
{
	bIsBeingDestroyed = true;
	PromptAnchorDestroyedNative.Broadcast(this);
	PromptAnchorDestroyedNative.Clear();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
EDataValidationResult URpgInteractionPromptAnchorComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (AnchorId.IsNone())
	{
		Context.AddError(NSLOCTEXT(
			"RpgInteraction",
			"PromptAnchorMissingId",
			"Interaction prompt anchors require a non-empty Anchor Id."));
		Result = EDataValidationResult::Invalid;
	}

	if (const AActor* Owner = GetOwner())
	{
		TInlineComponentArray<URpgInteractionPromptAnchorComponent*> Anchors(Owner);
		for (const URpgInteractionPromptAnchorComponent* Other : Anchors)
		{
			if (IsValid(Other) && Other != this && Other->AnchorId == AnchorId)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT(
						"RpgInteraction",
						"PromptAnchorDuplicateId",
						"Interaction prompt anchor id '{0}' is also used by component '{1}'. Anchor ids must be unique per actor."),
					FText::FromName(AnchorId),
					FText::FromName(Other->GetFName())));
				Result = EDataValidationResult::Invalid;
				break;
			}
		}
	}

	return Result;
}
#endif
