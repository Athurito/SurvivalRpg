#include "Harvesting/RpgHarvestChainComponent.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestTargeting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestChainComponent)

namespace RpgHarvestChains
{
	/** Identifies a target across selections: the receiver and, for instanced receivers, the instance. */
	TPair<const UObject*, int32> MakeTargetKey(const FRpgHarvestTargetEvaluation& Target)
	{
		const UObject* Receiver = Target.Receiver.Get();
		return TPair<const UObject*, int32>(
			Receiver,
			Receiver && Receiver == Target.Hit.GetComponent() ? Target.Hit.Item : INDEX_NONE);
	}

	/** Request that takes the whole remaining stock of a chained resource, like an area harvest from the trigger. */
	FRpgHarvestRequest MakeChainRequest(const FRpgHarvestRequest& Template, const FVector& TriggerLocation)
	{
		FRpgHarvestRequest Request = Template;
		Request.RequestedSections = URpgHarvestProfile::MaxSectionCount;
		Request.bAreaHarvest = true;
		Request.bCanHitWeakPoint = false;
		Request.TraceOrigin = TriggerLocation;
		Request.PresentationDelaySeconds = 0.0f;
		return Request;
	}

	/**
	 * Appends every harvestable resource inside Chain, nearest to TriggerLocation first, evaluated with ChainRequest. The
	 * trigger itself is listed too; it reports its stock as it is now.
	 */
	void CollectChainTargets(
		const UWorld& World,
		const URpgHarvestChainComponent& Chain,
		const FVector& TriggerLocation,
		const FRpgHarvestRequest& ChainRequest,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets)
	{
		// A sphere around the trigger that reaches the box's bounding sphere covers the whole box.
		const float Radius = static_cast<float>(FVector::Dist(TriggerLocation, Chain.Bounds.Origin) + Chain.Bounds.SphereRadius);
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgHarvestChain), false);
		TArray<FRpgHarvestTargetEvaluation> Candidates;
		FRpgHarvestTargeting::CollectAreaTargets(
			World,
			TriggerLocation,
			Radius,
			Chain.GetResourceChannel(),
			QueryParams,
			TriggerLocation,
			true,
			ChainRequest,
			0,
			Candidates);
		for (FRpgHarvestTargetEvaluation& Candidate : Candidates)
		{
			if (Chain.ContainsLocation(FRpgHarvestChains::GetTargetLocation(Candidate)))
			{
				OutTargets.Add(MoveTemp(Candidate));
			}
		}
	}
}

URpgHarvestChainComponent::URpgHarvestChainComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	InitBoxExtent(FVector(600.0f, 600.0f, 400.0f));
	ShapeColor = FColor(120, 220, 90);
}

bool URpgHarvestChainComponent::ContainsLocation(const FVector& WorldLocation) const
{
	// The inverse transform removes rotation and scale, so the unscaled extent describes the box.
	const FVector LocalLocation = GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= Extent.X &&
		FMath::Abs(LocalLocation.Y) <= Extent.Y &&
		FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

void URpgHarvestChainComponent::ConfigureChain(const int32 InMaxChainedTargets, const float InChainSpeed)
{
	MaxChainedTargets = InMaxChainedTargets;
	ChainSpeed = InChainSpeed;
}

void URpgHarvestChainComponent::BeginPlay()
{
	Super::BeginPlay();
	if (URpgHarvestChainSubsystem* Subsystem = UWorld::GetSubsystem<URpgHarvestChainSubsystem>(GetWorld()))
	{
		Subsystem->RegisterChain(*this);
	}
}

void URpgHarvestChainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgHarvestChainSubsystem* Subsystem = UWorld::GetSubsystem<URpgHarvestChainSubsystem>(GetWorld()))
	{
		Subsystem->UnregisterChain(*this);
	}
	Super::EndPlay(EndPlayReason);
}

void URpgHarvestChainSubsystem::FindChainsAt(
	const FVector& WorldLocation,
	TArray<const URpgHarvestChainComponent*>& OutChains) const
{
	for (const TWeakObjectPtr<const URpgHarvestChainComponent>& Chain : Chains)
	{
		if (const URpgHarvestChainComponent* ChainComponent = Chain.Get();
			ChainComponent && ChainComponent->ContainsLocation(WorldLocation))
		{
			OutChains.Add(ChainComponent);
		}
	}
}

void URpgHarvestChainSubsystem::RegisterChain(const URpgHarvestChainComponent& Chain)
{
	Chains.RemoveAll([](const TWeakObjectPtr<const URpgHarvestChainComponent>& Existing)
	{
		return !Existing.IsValid();
	});
	Chains.AddUnique(&Chain);
}

void URpgHarvestChainSubsystem::UnregisterChain(const URpgHarvestChainComponent& Chain)
{
	Chains.RemoveAll([&Chain](const TWeakObjectPtr<const URpgHarvestChainComponent>& Existing)
	{
		return !Existing.IsValid() || Existing.Get() == &Chain;
	});
}

void FRpgHarvestChains::AppendPreview(
	const UWorld& World,
	const FRpgHarvestRequest& RequestTemplate,
	TArray<FRpgHarvestTargetEvaluation>& InOutTargets)
{
	using namespace RpgHarvestChains;

	const URpgHarvestChainSubsystem* Subsystem = World.GetSubsystem<URpgHarvestChainSubsystem>();
	if (!Subsystem || Subsystem->GetNumChains() == 0)
	{
		return;
	}

	TSet<TObjectKey<URpgHarvestChainComponent>> ChainedBoxes;
	TArray<const URpgHarvestChainComponent*> Boxes;
	TArray<FRpgHarvestTargetEvaluation> Members;
	const int32 NumSelected = InOutTargets.Num();
	for (int32 TriggerIndex = 0; TriggerIndex < NumSelected; ++TriggerIndex)
	{
		const FRpgHarvestTargetEvaluation& Trigger = InOutTargets[TriggerIndex];
		if (!Trigger.WouldHarvest() || Trigger.Result.RemainingSections > 0)
		{
			continue;
		}
		const FVector TriggerLocation = GetTargetLocation(Trigger);
		Boxes.Reset();
		Subsystem->FindChainsAt(TriggerLocation, Boxes);
		for (const URpgHarvestChainComponent* Box : Boxes)
		{
			bool bAlreadyChained = false;
			ChainedBoxes.Add(Box, &bAlreadyChained);
			if (bAlreadyChained)
			{
				continue;
			}

			Members.Reset();
			CollectChainTargets(World, *Box, TriggerLocation, MakeChainRequest(RequestTemplate, TriggerLocation), Members);
			int32 NumChained = 0;
			for (FRpgHarvestTargetEvaluation& Member : Members)
			{
				if (NumChained >= Box->GetMaxChainedTargets())
				{
					break;
				}
				if (!Member.WouldHarvest())
				{
					continue;
				}

				const TPair<const UObject*, int32> MemberKey = MakeTargetKey(Member);
				FRpgHarvestTargetEvaluation* Listed = InOutTargets.FindByPredicate([&MemberKey](const FRpgHarvestTargetEvaluation& Target)
				{
					return MakeTargetKey(Target) == MemberKey;
				});
				if (!Listed)
				{
					Member.bChained = true;
					InOutTargets.Add(MoveTemp(Member));
					++NumChained;
				}
				else if (!Listed->WouldHarvest())
				{
					// The harvest itself would skip it, for example beyond its reach; the chain still takes it.
					Listed->Result = Member.Result;
					Listed->bInReach = true;
					Listed->bChained = true;
					++NumChained;
				}
				else if (Listed->Result.RemainingSections > 0)
				{
					// The harvest takes part of its stock; the chain takes the rest.
					Listed->Result.SectionsTaken += Listed->Result.RemainingSections;
					Listed->Result.RemainingSections = 0;
					++NumChained;
				}
			}
		}
	}
}

int32 FRpgHarvestChains::Commit(
	UWorld& World,
	const FRpgHarvestRequest& TriggerRequest,
	const FRpgHarvestTargetEvaluation& Trigger,
	TSet<TObjectKey<URpgHarvestChainComponent>>& InOutChainedBoxes,
	TArray<FRpgHarvestTargetEvaluation>& OutChained)
{
	using namespace RpgHarvestChains;

	const URpgHarvestChainSubsystem* Subsystem = World.GetSubsystem<URpgHarvestChainSubsystem>();
	if (!Subsystem || Subsystem->GetNumChains() == 0 || !Trigger.Result.IsSuccess() || !Trigger.Result.bDepleted)
	{
		return 0;
	}

	const FVector TriggerLocation = GetTargetLocation(Trigger);
	TArray<const URpgHarvestChainComponent*> Boxes;
	Subsystem->FindChainsAt(TriggerLocation, Boxes);
	int32 SectionsTaken = 0;
	TArray<FRpgHarvestTargetEvaluation> Members;
	for (const URpgHarvestChainComponent* Box : Boxes)
	{
		bool bAlreadyChained = false;
		InOutChainedBoxes.Add(Box, &bAlreadyChained);
		if (bAlreadyChained)
		{
			continue;
		}

		// Members are evaluated as they are now, after the harvest's own commits; emptied ones are skipped.
		const FRpgHarvestRequest ChainRequest = MakeChainRequest(TriggerRequest, TriggerLocation);
		Members.Reset();
		CollectChainTargets(World, *Box, TriggerLocation, ChainRequest, Members);
		const float ChainSpeed = Box->GetChainSpeed();
		int32 NumChained = 0;
		for (const FRpgHarvestTargetEvaluation& Member : Members)
		{
			UObject* Receiver = Member.Receiver.Get();
			if (NumChained >= Box->GetMaxChainedTargets())
			{
				break;
			}
			if (!Member.WouldHarvest() || !Receiver)
			{
				continue;
			}

			FRpgHarvestRequest Request = ChainRequest;
			Request.Hit = Member.Hit;
			Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Receiver, Member.Hit);
			const double TravelSeconds = ChainSpeed > UE_KINDA_SMALL_NUMBER
				? FVector::Dist2D(TriggerLocation, GetTargetLocation(Member)) / ChainSpeed
				: 0.0;
			Request.PresentationDelaySeconds = static_cast<float>(FMath::Clamp(
				TriggerRequest.PresentationDelaySeconds + TravelSeconds,
				0.0,
				static_cast<double>(URpgHarvestInstanceStockComponent::MaxPresentationDelaySeconds)));

			FRpgHarvestTargetEvaluation& Chained = OutChained.Add_GetRef(Member);
			Chained.bChained = true;
			Chained.Result = IRpgHarvestableTarget::Execute_CommitHarvest(Receiver, Request);
			if (Chained.Result.IsSuccess())
			{
				SectionsTaken += Chained.Result.SectionsTaken;
				++NumChained;
			}
		}
	}
	return SectionsTaken;
}

FVector FRpgHarvestChains::GetTargetLocation(const FRpgHarvestTargetEvaluation& Target)
{
	const UObject* Receiver = Target.Receiver.Get();
	if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Receiver);
		Instances && Target.Hit.Item != INDEX_NONE)
	{
		FTransform InstanceTransform;
		if (Instances->GetInstanceTransform(Target.Hit.Item, InstanceTransform, true))
		{
			return InstanceTransform.GetLocation();
		}
	}
	if (const UActorComponent* Component = Cast<UActorComponent>(Receiver))
	{
		if (const AActor* Owner = Component->GetOwner())
		{
			return Owner->GetActorLocation();
		}
	}
	if (const AActor* Actor = Cast<AActor>(Receiver))
	{
		return Actor->GetActorLocation();
	}
	return Target.Hit.ImpactPoint;
}
