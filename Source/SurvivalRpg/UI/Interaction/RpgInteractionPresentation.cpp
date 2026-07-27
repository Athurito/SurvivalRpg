// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionPresentation.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

bool RpgInteractionPresentation::IsFullPromptState(
	const ERpgInteractionPromptState State)
{
	return State == ERpgInteractionPromptState::Ready ||
		State == ERpgInteractionPromptState::Blocked;
}

void RpgInteractionPresentation::SelectNearbyOptionsForDisplay(
	TArray<FInteractionOption>& InOutOptions,
	const FVector& ViewOrigin,
	const int32 MaxVisibleOptions)
{
	if (MaxVisibleOptions <= 0)
	{
		InOutOptions.Reset();
		return;
	}

	InOutOptions.StableSort([&ViewOrigin](
		const FInteractionOption& A,
		const FInteractionOption& B)
	{
		if (A.Prompt.InteractionPriority != B.Prompt.InteractionPriority)
		{
			return A.Prompt.InteractionPriority > B.Prompt.InteractionPriority;
		}
		const float ADistance = FVector::DistSquared(
			ViewOrigin,
			A.GetInteractionWorldLocation());
		const float BDistance = FVector::DistSquared(
			ViewOrigin,
			B.GetInteractionWorldLocation());
		if (!FMath::IsNearlyEqual(ADistance, BDistance, 1.0f))
		{
			return ADistance < BDistance;
		}
		const FString ASlotKey =
			UInteractionStatics::MakePresentationSlotKey(A);
		const FString BSlotKey =
			UInteractionStatics::MakePresentationSlotKey(B);
		if (ASlotKey != BSlotKey)
		{
			return ASlotKey < BSlotKey;
		}
		const FString APresentationKey =
			UInteractionStatics::MakePresentationOptionKey(A);
		const FString BPresentationKey =
			UInteractionStatics::MakePresentationOptionKey(B);
		return APresentationKey != BPresentationKey
			? APresentationKey < BPresentationKey
			: UInteractionStatics::MakeStableOptionKey(A) <
				UInteractionStatics::MakeStableOptionKey(B);
	});

	TSet<FString> SeenSlotKeys;
	InOutOptions.RemoveAll([&SeenSlotKeys](const FInteractionOption& Option)
	{
		const FString SlotKey =
			UInteractionStatics::MakePresentationSlotKey(Option);
		if (SeenSlotKeys.Contains(SlotKey))
		{
			return true;
		}
		SeenSlotKeys.Add(SlotKey);
		return false;
	});
	if (InOutOptions.Num() <= MaxVisibleOptions)
	{
		return;
	}

	TArray<FInteractionOption> SelectedOptions;
	SelectedOptions.Reserve(MaxVisibleOptions);
	TSet<FString> SelectedOwnerKeys;
	TSet<FString> SelectedSlotKeys;
	for (const FInteractionOption& Option : InOutOptions)
	{
		const AActor* TargetActor = Option.TargetRef.TargetActor.Get();
		if (!TargetActor)
		{
			TargetActor = UInteractionStatics::GetActorFromInteractableTarget(
				Option.InteractableTarget);
		}
		const FString OwnerKey = GetPathNameSafe(
			TargetActor ? static_cast<const UObject*>(TargetActor) :
				Option.InteractableTarget.GetObject());
		if (SelectedOwnerKeys.Contains(OwnerKey))
		{
			continue;
		}

		SelectedOwnerKeys.Add(OwnerKey);
		SelectedSlotKeys.Add(
			UInteractionStatics::MakePresentationSlotKey(Option));
		SelectedOptions.Add(Option);
		if (SelectedOptions.Num() >= MaxVisibleOptions)
		{
			break;
		}
	}

	for (const FInteractionOption& Option : InOutOptions)
	{
		if (SelectedOptions.Num() >= MaxVisibleOptions)
		{
			break;
		}
		const FString SlotKey =
			UInteractionStatics::MakePresentationSlotKey(Option);
		if (!SelectedSlotKeys.Contains(SlotKey))
		{
			SelectedSlotKeys.Add(SlotKey);
			SelectedOptions.Add(Option);
		}
	}

	InOutOptions = MoveTemp(SelectedOptions);
}

bool RpgInteractionPresentation::ConfigureDescriptorPlacement(
	UIndicatorDescriptor& Descriptor,
	const FInteractionOption& Option)
{
	Descriptor.SetComponentSocketName(NAME_None);
	Descriptor.SetWorldPositionOffset(FVector::ZeroVector);
	Descriptor.SetScreenSpaceOffset(FVector2D::ZeroVector);
	Descriptor.SetBoundingBoxAnchor(FVector(0.5f));
	Descriptor.SetClampToScreen(false);

	if (USceneComponent* PromptAnchor =
		UInteractionStatics::FindPromptAnchorComponent(Option))
	{
		Descriptor.SetSceneComponent(PromptAnchor);
		Descriptor.SetProjectionMode(
			EActorCanvasProjectionMode::ComponentPoint);
		Descriptor.ClearWorldPositionOverride();
		return true;
	}

	if (Option.TargetRef.InstanceIndex != INDEX_NONE)
	{
		UInstancedStaticMeshComponent* InstancedMesh =
			Cast<UInstancedStaticMeshComponent>(
				Option.TargetRef.TargetComponent.Get());
		FTransform InstanceWorldTransform;
		if (!InstancedMesh ||
			Option.TargetRef.InstanceIndex < 0 ||
			Option.TargetRef.InstanceIndex >=
				InstancedMesh->GetInstanceCount() ||
			!InstancedMesh->GetInstanceTransform(
				Option.TargetRef.InstanceIndex,
				InstanceWorldTransform,
				true))
		{
			Descriptor.SetSceneComponent(nullptr);
			Descriptor.ClearWorldPositionOverride();
			return false;
		}

		const UStaticMesh* StaticMesh = InstancedMesh->GetStaticMesh();
		const FVector LocalBoundsCenter = StaticMesh
			? StaticMesh->GetBounds().Origin
			: FVector::ZeroVector;
		Descriptor.SetSceneComponent(InstancedMesh);
		Descriptor.SetProjectionMode(
			EActorCanvasProjectionMode::ComponentPoint);
		Descriptor.SetWorldPositionOverride(
			InstanceWorldTransform.TransformPosition(LocalBoundsCenter));
		return true;
	}

	if (AActor* TargetActor = Option.TargetRef.TargetActor.Get())
	{
		if (USceneComponent* RootComponent =
			TargetActor->GetRootComponent())
		{
			Descriptor.SetSceneComponent(RootComponent);
			Descriptor.SetProjectionMode(
				EActorCanvasProjectionMode::ActorBoundingBox);
			Descriptor.ClearWorldPositionOverride();
			return true;
		}
	}

	if (USceneComponent* TargetComponent =
		Option.TargetRef.TargetComponent.Get())
	{
		Descriptor.SetSceneComponent(TargetComponent);
		Descriptor.SetProjectionMode(
			EActorCanvasProjectionMode::ComponentBoundingBox);
		Descriptor.ClearWorldPositionOverride();
		return true;
	}

	Descriptor.SetSceneComponent(nullptr);
	Descriptor.ClearWorldPositionOverride();
	return false;
}
