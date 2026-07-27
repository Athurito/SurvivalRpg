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
