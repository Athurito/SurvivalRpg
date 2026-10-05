#include "Harvesting/RpgHarvestTargeting.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestTargeting)

namespace RpgHarvestTargeting
{
	/** Height above the clamped ray end at which the ground probe of an unaimed area starts, in centimeters. */
	constexpr double AreaGroundProbeHeight = 200.0;

	/** Depth below the clamped ray end that the ground probe of an unaimed area searches, in centimeters. */
	constexpr double AreaGroundProbeDepth = 2000.0;

	struct FAreaCandidate
	{
		UObject* Receiver = nullptr;
		FHitResult Hit;
		double DistanceSquared = 0.0;
	};

	void AppendEvaluation(
		UObject* Receiver,
		const FHitResult& Hit,
		const bool bInReach,
		const FVector& ViewLocation,
		const FRpgHarvestRequest& RequestTemplate,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets)
	{
		FRpgHarvestRequest Request = RequestTemplate;
		Request.Hit = Hit;
		Request.TraceOrigin = ViewLocation;
		Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Receiver, Hit);

		FRpgHarvestTargetEvaluation& Evaluation = OutTargets.AddDefaulted_GetRef();
		Evaluation.Receiver = Receiver;
		Evaluation.Hit = Hit;
		Evaluation.bInReach = bInReach;
		Evaluation.Result = IRpgHarvestableTarget::Execute_EvaluateHarvest(Receiver, Request);
	}

	FVector GetOverlapLocation(const FOverlapResult& Overlap)
	{
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Overlap.GetComponent()))
		{
			FTransform InstanceTransform;
			if (Overlap.ItemIndex != INDEX_NONE && Instances->GetInstanceTransform(Overlap.ItemIndex, InstanceTransform, true))
			{
				return InstanceTransform.GetLocation();
			}
		}
		const AActor* Actor = Overlap.GetActor();
		return Actor ? Actor->GetActorLocation() : Overlap.GetComponent()->GetComponentLocation();
	}
}

bool FRpgHarvestPreview::IsEquivalent(const FRpgHarvestPreview& Other, const float LocationTolerance) const
{
	if (AbilityId != Other.AbilityId ||
		bIsAiming != Other.bIsAiming ||
		bHasArea != Other.bHasArea ||
		!FMath::IsNearlyEqual(AreaRadius, Other.AreaRadius) ||
		(bHasArea && !AimPoint.Equals(Other.AimPoint, LocationTolerance)) ||
		Targets.Num() != Other.Targets.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		const FRpgHarvestTargetEvaluation& A = Targets[Index];
		const FRpgHarvestTargetEvaluation& B = Other.Targets[Index];
		if (A.Receiver != B.Receiver ||
			A.Hit.Item != B.Hit.Item ||
			A.bInReach != B.bInReach ||
			A.Result.Outcome != B.Result.Outcome ||
			A.Result.SectionsTaken != B.Result.SectionsTaken ||
			A.Result.bWeakPointHit != B.Result.bWeakPointHit ||
			A.Result.RemainingSections != B.Result.RemainingSections ||
			A.Result.SectionCount != B.Result.SectionCount)
		{
			return false;
		}
	}
	return true;
}

UObject* FRpgHarvestTargeting::FindReceiver(const FHitResult& Hit)
{
	// Instance identity belongs to the hit component. Prefer it before an actor-level
	// implementation or unrelated components on an actor with multiple resource meshes.
	if (UPrimitiveComponent* HitComponent = Hit.GetComponent();
		HitComponent && HitComponent->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
	{
		return HitComponent;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return nullptr;
	}
	if (HitActor->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
	{
		return HitActor;
	}

	TInlineComponentArray<UActorComponent*> Components(HitActor);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component != Hit.GetComponent() &&
			Component->GetClass()->ImplementsInterface(URpgHarvestableTarget::StaticClass()))
		{
			return Component;
		}
	}
	return nullptr;
}

FVector FRpgHarvestTargeting::SelectAndEvaluate(
	const UWorld& World,
	const FRpgHarvestTargetingParams& Params,
	const FVector& ViewLocation,
	const FRotator& ViewRotation,
	const AActor& Avatar,
	const FRpgHarvestRequest& RequestTemplate,
	TArray<FRpgHarvestTargetEvaluation>& OutTargets)
{
	using namespace RpgHarvestTargeting;

	OutTargets.Reset();
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * FMath::Max(0.0f, Params.MaxAimDistance);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgHarvestTargeting), false, &Avatar);
	if (AActor* AvatarOwner = Avatar.GetOwner())
	{
		QueryParams.AddIgnoredActor(AvatarOwner);
	}

	TArray<FHitResult> Hits;
	if (Params.AimRadius > KINDA_SMALL_NUMBER)
	{
		World.SweepMultiByChannel(
			Hits,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			Params.TraceChannel,
			FCollisionShape::MakeSphere(Params.AimRadius),
			QueryParams);
	}
	else
	{
		World.LineTraceMultiByChannel(Hits, ViewLocation, TraceEnd, Params.TraceChannel, QueryParams);
	}
	Hits.StableSort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	});

	const FVector AvatarLocation = Avatar.GetActorLocation();
	const double MaxReachSquared = FMath::Square(static_cast<double>(FMath::Max(0.0f, Params.MaxReachFromAvatar)));

	if (Params.Shape == ERpgHarvestTargetShape::SingleTarget)
	{
		for (const FHitResult& Hit : Hits)
		{
			if (UObject* Receiver = FindReceiver(Hit))
			{
				const bool bInReach = FVector::DistSquared(AvatarLocation, Hit.ImpactPoint) <= MaxReachSquared;
				AppendEvaluation(Receiver, Hit, bInReach, ViewLocation, RequestTemplate, OutTargets);
				return Hit.ImpactPoint;
			}
			if (Hit.bBlockingHit)
			{
				return Hit.ImpactPoint;
			}
		}
		return TraceEnd;
	}

	FVector AimPoint = TraceEnd;
	bool bAimHit = false;
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.bBlockingHit)
		{
			AimPoint = Hit.ImpactPoint;
			bAimHit = true;
			break;
		}
	}
	if (!bAimHit)
	{
		// A level view ray rarely meets open ground: keep the ray end within reach and drop it onto the ground.
		const FVector FromAvatar = TraceEnd - AvatarLocation;
		const FVector ProbeOrigin = AvatarLocation + FromAvatar.GetClampedToMaxSize(FMath::Max(0.0f, Params.MaxReachFromAvatar));
		FHitResult GroundHit;
		if (World.LineTraceSingleByChannel(
				GroundHit,
				ProbeOrigin + FVector(0.0, 0.0, AreaGroundProbeHeight),
				ProbeOrigin - FVector(0.0, 0.0, AreaGroundProbeDepth),
				Params.TraceChannel,
				QueryParams))
		{
			// The drop to the ground can add a little distance; keep the area just inside the reach.
			AimPoint = AvatarLocation +
				(GroundHit.ImpactPoint - AvatarLocation).GetClampedToMaxSize(FMath::Max(0.0f, Params.MaxReachFromAvatar - 1.0f));
		}
	}
	const bool bAimInReach = FVector::DistSquared(AvatarLocation, AimPoint) <= MaxReachSquared;
	if (Params.AreaRadius <= KINDA_SMALL_NUMBER)
	{
		return AimPoint;
	}

	TArray<FOverlapResult> Overlaps;
	World.OverlapMultiByChannel(
		Overlaps,
		AimPoint,
		FQuat::Identity,
		Params.TraceChannel,
		FCollisionShape::MakeSphere(Params.AreaRadius),
		QueryParams);

	TArray<FAreaCandidate> Candidates;
	TSet<TPair<const UObject*, int32>> SeenTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		AActor* Actor = Overlap.GetActor();
		if (!Component || !Actor)
		{
			continue;
		}

		const FVector TargetLocation = GetOverlapLocation(Overlap);
		FHitResult Hit(Actor, Component, TargetLocation, FVector::UpVector);
		Hit.Item = Overlap.ItemIndex;
		UObject* Receiver = FindReceiver(Hit);
		if (!Receiver)
		{
			continue;
		}

		// Several primitives of one actor-backed resource collapse into one target; instances stay distinct.
		const int32 InstanceKey = Receiver == Component ? Overlap.ItemIndex : INDEX_NONE;
		bool bAlreadySeen = false;
		SeenTargets.Add(TPair<const UObject*, int32>(Receiver, InstanceKey), &bAlreadySeen);
		if (bAlreadySeen)
		{
			continue;
		}

		FAreaCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Receiver = Receiver;
		Candidate.Hit = Hit;
		Candidate.DistanceSquared = FVector::DistSquared(AimPoint, TargetLocation);
	}

	Candidates.StableSort([](const FAreaCandidate& A, const FAreaCandidate& B)
	{
		return A.DistanceSquared < B.DistanceSquared;
	});

	// Only targets the request would harvest use up MaxTargets. Nearer rejected targets, such as protected or
	// depleted resources, stay listed so the preview can explain why they are skipped.
	const int32 MaxTargets = FMath::Max(1, Params.MaxTargets);
	int32 HarvestableTargets = 0;
	for (const FAreaCandidate& Candidate : Candidates)
	{
		AppendEvaluation(
			Candidate.Receiver,
			Candidate.Hit,
			bAimInReach,
			ViewLocation,
			RequestTemplate,
			OutTargets);
		if (OutTargets.Last().Result.IsSuccess() && ++HarvestableTargets >= MaxTargets)
		{
			break;
		}
	}
	return AimPoint;
}
