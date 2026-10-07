#include "Harvesting/RpgHarvestableInstancesComponent.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestPersistenceComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableInstancesComponent)

URpgHarvestableInstancesComponent::URpgHarvestableInstancesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	// The stock replicates through the GameState; instance owners such as PCG partition actors never replicate.
	SetIsReplicatedByDefault(false);
	// Harvesting traces instances; per-instance overlap events would only add a query per instance at BeginPlay.
	SetGenerateOverlapEvents(false);
}

int32 URpgHarvestableInstancesComponent::GetHarvestRevision_Implementation(const FHitResult& Hit) const
{
	return Hit.GetComponent() == this && IsValidResourceInstance(Hit.Item) && FindStock()
		? MakeStockSnapshot(Hit.Item).Revision
		: INDEX_NONE;
}

FRpgHarvestResult URpgHarvestableInstancesComponent::EvaluateHarvest_Implementation(const FRpgHarvestRequest& Request) const
{
	const int32 InstanceIndex = Request.Hit.Item;
	if (Request.Hit.GetComponent() != this || !IsValidResourceInstance(InstanceIndex) || !HarvestProfile || !FindStock())
	{
		return FRpgHarvestResult::MakeRejected(
			ERpgHarvestOutcome::Invalid,
			GetRemainingSections(InstanceIndex),
			GetSectionCount());
	}

	// Only area requests respect protection, so only they pay for the instance location.
	FTransform InstanceTransform;
	const bool bProbeProtection = Request.bAreaHarvest && GetAuthoredInstanceTransform(InstanceIndex, InstanceTransform, true);
	const FVector ProtectionProbe = InstanceTransform.GetLocation();
	const FRpgHarvestResult Result = FRpgHarvestStockRules::Evaluate(
		HarvestProfile,
		Request,
		MakeStockSnapshot(InstanceIndex),
		nullptr,
		bProbeProtection ? &ProtectionProbe : nullptr);
	if (Result.IsSuccess() && bCommitInProgress)
	{
		return FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Stale, Result.RemainingSections, Result.SectionCount);
	}
	return Result;
}

FRpgHarvestResult URpgHarvestableInstancesComponent::CommitHarvest_Implementation(const FRpgHarvestRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestableInstancesComponent::CommitHarvest);

	// Instance owners do not replicate and report authority on clients too; the stock's GameState decides.
	URpgHarvestInstanceStockComponent* Stock = FindStock();
	const int32 InstanceIndex = Request.Hit.Item;
	FIntVector Key;
	if (!Stock || !Stock->HasStockAuthority() || !GetInstanceKey(InstanceIndex, Key))
	{
		return FRpgHarvestResult::MakeRejected(
			ERpgHarvestOutcome::Invalid,
			GetRemainingSections(InstanceIndex),
			GetSectionCount());
	}

	// Re-run the complete validation at the moment of extraction; other harvesters share the stock.
	FRpgHarvestResult Result = EvaluateHarvest_Implementation(Request);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TGuardValue<bool> CommitGuard(bCommitInProgress, true);

	const int32 SectionCount = GetSectionCount();
	const FRpgHarvestStockSnapshot Before = Stock->GetStockSnapshot(Key, SectionCount);
	FTransform InstanceTransform;
	GetAuthoredInstanceTransform(InstanceIndex, InstanceTransform, true);

	FRpgHarvestRewardRequest RewardRequest;
	// Rewarded item instances must not be outered to an instance owner that can stream out, so the GameState is
	// the loot source.
	RewardRequest.SourceActor = Stock->GetOwner();
	RewardRequest.Harvester = Request.Harvester;
	RewardRequest.DeliveryTransform = FTransform(
		InstanceTransform.GetRotation(),
		Request.Hit.ImpactPoint.IsNearlyZero() ? InstanceTransform.GetLocation() : FVector(Request.Hit.ImpactPoint));
	RewardRequest.HarvestPower = Request.HarvestPower;
	RewardRequest.RollCount = Result.SectionsTaken;
	RewardRequest.SeedSalt = HashCombine(
		GetTypeHash(Key),
		HashCombine(GetTypeHash(Before.Revision), GetTypeHash(Before.HarvestedSections)));
	const ERpgHarvestRewardDeliveryResult DeliveryResult =
		FRpgHarvestRewardService::DeliverReward(HarvestProfile, RewardRequest);
	if (DeliveryResult == ERpgHarvestRewardDeliveryResult::Failed)
	{
		// Nothing was granted, so the stock stays untouched and the request may be retried.
		return FRpgHarvestResult::MakeRejected(
			ERpgHarvestOutcome::DeliveryFailed,
			Before.GetRemainingSections(),
			Before.SectionCount);
	}

	Result.bDepleted = Result.RemainingSections <= 0;
	float RespawnDelaySeconds = 0.0f;
	if (Result.bDepleted)
	{
		const float MinimumDelay = FMath::Max(0.0f, HarvestProfile->MinimumRespawnSeconds);
		const float MaximumDelay = FMath::Max(MinimumDelay, HarvestProfile->MaximumRespawnSeconds);
		RespawnDelaySeconds = MaximumDelay > 0.0f ? FMath::Max(0.001f, FMath::FRandRange(MinimumDelay, MaximumDelay)) : 0.0f;
	}
	// The direction from whatever struck the instance toward it lets every machine present the change the same way.
	const AActor* StrikingActor = Request.GetStrikingActor();
	FVector AwayFromHarvester = InstanceTransform.GetLocation() - (StrikingActor ? StrikingActor->GetActorLocation() : Request.TraceOrigin);
	AwayFromHarvester.Z = 0.0;
	const float HarvestYawDegrees = AwayFromHarvester.IsNearlyZero() ? 0.0f : static_cast<float>(AwayFromHarvester.Rotation().Yaw);
	Stock->ExtractSections(
		Key,
		SectionCount,
		Result.SectionsTaken,
		RespawnDelaySeconds,
		Request.PresentationDelaySeconds,
		HarvestYawDegrees);
	if (URpgHarvestPersistenceComponent* Persistence = URpgHarvestPersistenceComponent::FindForWorld(GetWorld()))
	{
		Persistence->RecordInstance(*this, *Stock, Key);
	}

	FRpgHarvestRewardService::AwardExperience(HarvestProfile, Request.Harvester, Result.SectionsTaken);
	Result.Delivery = FRpgHarvestStockRules::ToDelivery(DeliveryResult);
	return Result;
}

int32 URpgHarvestableInstancesComponent::GetSectionCount() const
{
	return HarvestProfile ? HarvestProfile->GetClampedSectionCount() : 0;
}

int32 URpgHarvestableInstancesComponent::GetRemainingSections(const int32 InstanceIndex) const
{
	return HarvestProfile && IsValidResourceInstance(InstanceIndex)
		? MakeStockSnapshot(InstanceIndex).GetRemainingSections()
		: 0;
}

bool URpgHarvestableInstancesComponent::GetAuthoredInstanceTransform(
	const int32 InstanceIndex,
	FTransform& OutTransform,
	const bool bWorldSpace) const
{
	if (!IsValidResourceInstance(InstanceIndex))
	{
		return false;
	}
	if (const FTransform* AuthoredTransform = PresentedInstanceTransforms.Find(InstanceIndex))
	{
		OutTransform = bWorldSpace ? *AuthoredTransform * GetComponentTransform() : *AuthoredTransform;
		return true;
	}
	return GetInstanceTransform(InstanceIndex, OutTransform, bWorldSpace);
}

bool URpgHarvestableInstancesComponent::SetInstancePresentationScale(const int32 InstanceIndex, const float Scale)
{
	FTransform AuthoredTransform;
	if (!FMath::IsFinite(Scale) || !GetAuthoredInstanceTransform(InstanceIndex, AuthoredTransform, false))
	{
		return false;
	}

	if (FMath::IsNearlyEqual(Scale, 1.0f))
	{
		return PresentedInstanceTransforms.Remove(InstanceIndex) == 0 ||
			UpdatePresentedTransform(*this, InstanceIndex, AuthoredTransform);
	}

	PresentedInstanceTransforms.FindOrAdd(InstanceIndex, AuthoredTransform);
	FTransform PresentedTransform = AuthoredTransform;
	PresentedTransform.SetScale3D(AuthoredTransform.GetScale3D() * FMath::Max(0.0f, Scale));
	return UpdatePresentedTransform(*this, InstanceIndex, PresentedTransform);
}

bool URpgHarvestableInstancesComponent::UpdatePresentedTransform(
	UInstancedStaticMeshComponent& Component,
	const int32 InstanceIndex,
	const FTransform& LocalTransform)
{
	// UpdateInstanceTransform recomputes the bounds of every instance for the navigation update of a navigation-relevant
	// component: 0.3 ms per instance in a field of 10,000, so hiding thousands of instances took seconds (HARV-10c).
	// This component therefore collects its navigation updates and applies them once on the next tick. Linked meshes
	// have no collision and update directly.
	const bool bDeferNavigation = &Component == this && Component.bNavigationRelevant && Component.IsRegistered();
	if (!bDeferNavigation)
	{
		return Component.UpdateInstanceTransform(InstanceIndex, LocalTransform, false, true, true);
	}

	FTransform PreviousLocalTransform;
	if (Component.GetInstanceTransform(InstanceIndex, PreviousLocalTransform, false))
	{
		DeferredNavigationTransforms.Add(PreviousLocalTransform * Component.GetComponentTransform());
		DeferredNavigationTransforms.Add(LocalTransform * Component.GetComponentTransform());
	}
	Component.bNavigationRelevant = false;
	const bool bUpdated = Component.UpdateInstanceTransform(InstanceIndex, LocalTransform, false, true, true);
	Component.bNavigationRelevant = true;

	UWorld* World = GetWorld();
	if (World && !World->GetTimerManager().IsTimerActive(NavigationFlushTimerHandle))
	{
		NavigationFlushTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FlushNavigationUpdates);
	}
	return bUpdated;
}

void URpgHarvestableInstancesComponent::FlushNavigationUpdates()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestableInstancesComponent::FlushNavigationUpdates);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NavigationFlushTimerHandle);
	}
	if (!DeferredNavigationTransforms.IsEmpty() && bNavigationRelevant && IsRegistered())
	{
		if (SupportsPartialNavigationUpdate())
		{
			PartialNavigationUpdates(DeferredNavigationTransforms);
		}
		else
		{
			FullNavigationUpdate();
		}
	}
	DeferredNavigationTransforms.Reset();
}

bool URpgHarvestableInstancesComponent::GetLinkedPresentationInstance(
	const int32 InstanceIndex,
	UMeshComponent*& OutComponent,
	FTransform& OutWorldTransform) const
{
	OutComponent = nullptr;
	OutWorldTransform = FTransform::Identity;
	for (auto It = LinkedInstances.CreateConstKeyIterator(InstanceIndex); It; ++It)
	{
		UMeshComponent* Component = It.Value().Component.Get();
		if (!Component)
		{
			continue;
		}

		const int32 LinkedIndex = It.Value().InstanceIndex;
		FTransform LocalTransform;
		if (const FTransform* HiddenTransform = HiddenLinkedTransforms.Find(MakeTuple(It.Value().Component, LinkedIndex)))
		{
			LocalTransform = *HiddenTransform;
		}
		else if (const UInstancedStaticMeshComponent* StaticInstances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			StaticInstances->GetInstanceTransform(LinkedIndex, LocalTransform, false);
		}
		else if (const UInstancedSkinnedMeshComponent* SkinnedInstances = Cast<UInstancedSkinnedMeshComponent>(Component))
		{
			SkinnedInstances->GetInstanceTransform(SkinnedInstances->GetInstanceId(LinkedIndex), LocalTransform, false);
		}
		OutComponent = Component;
		OutWorldTransform = LocalTransform * Component->GetComponentTransform();
		return true;
	}
	return false;
}

bool URpgHarvestableInstancesComponent::GetInstanceHarvestDirection(const int32 InstanceIndex, FVector& OutDirection) const
{
	FIntVector Key;
	const URpgHarvestInstanceStockComponent* Stock = FindStock();
	if (!Stock || !GetInstanceKey(InstanceIndex, Key))
	{
		OutDirection = FVector::ZeroVector;
		return false;
	}
	return Stock->GetHarvestDirection(Key, OutDirection);
}

bool URpgHarvestableInstancesComponent::GetInstanceKey(const int32 InstanceIndex, FIntVector& OutKey) const
{
	if (!IsValidResourceInstance(InstanceIndex))
	{
		return false;
	}

	// Presentation only scales instances, so the stored origin is always the authored one. Reading it directly
	// avoids decomposing the instance matrix, which dominates the key build when a large field streams in.
	const FVector LocalOrigin = PerInstanceSMData[InstanceIndex].Transform.GetOrigin();
	OutKey = URpgHarvestInstanceStockComponent::MakeInstanceKey(GetComponentTransform().TransformPosition(LocalOrigin));
	return true;
}

bool URpgHarvestableInstancesComponent::FindInstanceByKey(const FIntVector& Key, int32& OutInstanceIndex) const
{
	const int32* InstanceIndex = InstanceIndexByKey.Find(Key);
	OutInstanceIndex = InstanceIndex ? *InstanceIndex : INDEX_NONE;
	return InstanceIndex != nullptr;
}

void URpgHarvestableInstancesComponent::OnInstanceStockChanged_Implementation(
	const int32 InstanceIndex,
	const int32 RemainingSections,
	const int32 SectionCount,
	const bool bActive,
	const bool bInitialState)
{
}

void URpgHarvestableInstancesComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildInstanceKeys();
	BuildLinkedInstances();
	if (!HarvestProfile)
	{
		UE_LOG(
			LogRpgHarvesting,
			Warning,
			TEXT("%s has no harvest profile and rejects every harvest request."),
			*GetPathName());
	}

	// Without a stock component yet, the stock component registers this one when it begins play.
	URpgHarvestInstanceStockComponent* Stock = FindStock();
	if (Stock && Stock->HasBegunPlay())
	{
		Stock->RegisterInstances(*this);
	}
}

void URpgHarvestableInstancesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgHarvestInstanceStockComponent* Stock = CachedStock.Get())
	{
		Stock->UnregisterInstances(*this);

		// A renewable area that unloads, such as a portal realm, starts over with its authored stock next time.
		const AActor* OwningActor = GetOwner();
		if (EndPlayReason == EEndPlayReason::RemovedFromWorld &&
			OwningActor &&
			FRpgHarvestStockRules::IsInRenewableArea(*OwningActor))
		{
			Stock->RestoreInstances(*this);
		}
	}
	CachedStock.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationTimerHandle);
		World->GetTimerManager().ClearTimer(NavigationFlushTimerHandle);
	}
	PendingPresentations.Reset();
	DeferredNavigationTransforms.Reset();
	Super::EndPlay(EndPlayReason);
}

bool URpgHarvestableInstancesComponent::IsValidResourceInstance(const int32 InstanceIndex) const
{
	return InstanceIndex >= 0 && InstanceIndex < GetInstanceCount();
}

URpgHarvestInstanceStockComponent* URpgHarvestableInstancesComponent::FindStock() const
{
	if (URpgHarvestInstanceStockComponent* Stock = CachedStock.Get())
	{
		return Stock;
	}
	URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(GetWorld());
	CachedStock = Stock;
	return Stock;
}

FRpgHarvestStockSnapshot URpgHarvestableInstancesComponent::MakeStockSnapshot(const int32 InstanceIndex) const
{
	const int32 SectionCount = FMath::Max(1, GetSectionCount());
	FIntVector Key;
	if (const URpgHarvestInstanceStockComponent* Stock = FindStock(); Stock && GetInstanceKey(InstanceIndex, Key))
	{
		return Stock->GetStockSnapshot(Key, SectionCount);
	}

	// Without stored stock every instance holds its authored, complete stock.
	FRpgHarvestStockSnapshot Snapshot;
	Snapshot.SectionCount = SectionCount;
	return Snapshot;
}

void URpgHarvestableInstancesComponent::BuildInstanceKeys()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestableInstancesComponent::BuildInstanceKeys);

	const int32 InstanceCount = GetInstanceCount();
	InstanceIndexByKey.Reset();
	InstanceIndexByKey.Reserve(InstanceCount);
	int32 DuplicateCount = 0;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		FIntVector Key;
		if (GetInstanceKey(InstanceIndex, Key) && InstanceIndexByKey.FindOrAdd(Key, InstanceIndex) != InstanceIndex)
		{
			++DuplicateCount;
		}
	}

	if (DuplicateCount > 0)
	{
		UE_LOG(
			LogRpgHarvesting,
			Warning,
			TEXT("%s has %d instances sharing a location with another instance within one centimeter; they share its stock and presentation."),
			*GetPathName(),
			DuplicateCount);
	}
}

void URpgHarvestableInstancesComponent::BuildLinkedInstances()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URpgHarvestableInstancesComponent::BuildLinkedInstances);

	LinkedInstances.Reset();
	HiddenLinkedTransforms.Reset();
	NumLinkedResources = 0;
	AActor* Owner = GetOwner();
	if (LinkedPresentationTag.IsNone() || !Owner || InstanceIndexByKey.IsEmpty())
	{
		return;
	}

	auto LinkInstance = [this](UMeshComponent& Component, const int32 LinkedIndex, const FVector& WorldOrigin)
	{
		if (const int32* ResourceIndex = InstanceIndexByKey.Find(URpgHarvestInstanceStockComponent::MakeInstanceKey(WorldOrigin)))
		{
			FLinkedInstance Link;
			Link.Component = &Component;
			Link.InstanceIndex = LinkedIndex;
			LinkedInstances.Add(*ResourceIndex, Link);
		}
	};

	TInlineComponentArray<UMeshComponent*> Meshes(Owner);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == this || !Mesh->ComponentHasTag(LinkedPresentationTag))
		{
			continue;
		}

		if (UInstancedStaticMeshComponent* StaticInstances = Cast<UInstancedStaticMeshComponent>(Mesh))
		{
			const FTransform& ComponentTransform = StaticInstances->GetComponentTransform();
			for (int32 LinkedIndex = 0; LinkedIndex < StaticInstances->GetInstanceCount(); ++LinkedIndex)
			{
				const FVector LocalOrigin = StaticInstances->PerInstanceSMData[LinkedIndex].Transform.GetOrigin();
				LinkInstance(*StaticInstances, LinkedIndex, ComponentTransform.TransformPosition(LocalOrigin));
			}
		}
		else if (UInstancedSkinnedMeshComponent* SkinnedInstances = Cast<UInstancedSkinnedMeshComponent>(Mesh))
		{
			for (int32 LinkedIndex = 0; LinkedIndex < SkinnedInstances->GetInstanceCount(); ++LinkedIndex)
			{
				FTransform WorldTransform;
				if (SkinnedInstances->GetInstanceTransform(SkinnedInstances->GetInstanceId(LinkedIndex), WorldTransform, true))
				{
					LinkInstance(*SkinnedInstances, LinkedIndex, WorldTransform.GetLocation());
				}
			}
		}
	}

	TArray<int32> LinkedResources;
	NumLinkedResources = LinkedInstances.GetKeys(LinkedResources);
	if (LinkedInstances.IsEmpty())
	{
		UE_LOG(
			LogRpgHarvesting,
			Warning,
			TEXT("%s links presentation through tag %s, but no sibling instance shares a resource location."),
			*GetPathName(),
			*LinkedPresentationTag.ToString());
	}
}

void URpgHarvestableInstancesComponent::SetLinkedInstancesVisible(const int32 InstanceIndex, const bool bShow)
{
	for (auto It = LinkedInstances.CreateConstKeyIterator(InstanceIndex); It; ++It)
	{
		UMeshComponent* Component = It.Value().Component.Get();
		const int32 LinkedIndex = It.Value().InstanceIndex;
		if (!Component)
		{
			continue;
		}

		UInstancedStaticMeshComponent* StaticInstances = Cast<UInstancedStaticMeshComponent>(Component);
		UInstancedSkinnedMeshComponent* SkinnedInstances = Cast<UInstancedSkinnedMeshComponent>(Component);
		const TPair<TWeakObjectPtr<UMeshComponent>, int32> HiddenKey = MakeTuple(It.Value().Component, LinkedIndex);
		FTransform PresentedTransform;
		if (bShow)
		{
			FTransform AuthoredTransform;
			if (!HiddenLinkedTransforms.RemoveAndCopyValue(HiddenKey, AuthoredTransform))
			{
				continue;
			}
			PresentedTransform = AuthoredTransform;
		}
		else
		{
			if (HiddenLinkedTransforms.Contains(HiddenKey))
			{
				continue;
			}
			FTransform AuthoredTransform;
			const bool bHasTransform = StaticInstances
				? StaticInstances->GetInstanceTransform(LinkedIndex, AuthoredTransform, false)
				: SkinnedInstances && SkinnedInstances->GetInstanceTransform(SkinnedInstances->GetInstanceId(LinkedIndex), AuthoredTransform, false);
			if (!bHasTransform)
			{
				continue;
			}
			HiddenLinkedTransforms.Add(HiddenKey, AuthoredTransform);
			PresentedTransform = AuthoredTransform;
			PresentedTransform.SetScale3D(FVector::ZeroVector);
		}

		if (StaticInstances)
		{
			UpdatePresentedTransform(*StaticInstances, LinkedIndex, PresentedTransform);
		}
		else if (SkinnedInstances)
		{
			SkinnedInstances->SetInstanceTransform(SkinnedInstances->GetInstanceId(LinkedIndex), PresentedTransform, false);
		}
	}
}

void URpgHarvestableInstancesComponent::HandleStockChanged(const FIntVector& Key, const bool bInitialState)
{
	int32 InstanceIndex = INDEX_NONE;
	if (!FindInstanceByKey(Key, InstanceIndex))
	{
		return;
	}

	// A newer change supersedes a presentation that still waits.
	PendingPresentations.Remove(InstanceIndex);
	const URpgHarvestInstanceStockComponent* Stock = FindStock();
	const UWorld* World = GetWorld();
	const float Delay = !bInitialState && Stock ? Stock->GetRemainingPresentationDelay(Key) : 0.0f;
	if (World && Delay > UE_KINDA_SMALL_NUMBER)
	{
		PendingPresentations.Add(InstanceIndex, World->GetTimeSeconds() + Delay);
		ArmPresentationTimer();
		return;
	}
	PresentInstance(InstanceIndex, bInitialState);
}

void URpgHarvestableInstancesComponent::ArmPresentationTimer()
{
	UWorld* World = GetWorld();
	if (!World || PendingPresentations.IsEmpty())
	{
		return;
	}

	double EarliestDeadline = TNumericLimits<double>::Max();
	for (const TPair<int32, double>& Pending : PendingPresentations)
	{
		EarliestDeadline = FMath::Min(EarliestDeadline, Pending.Value);
	}
	const float Delay = static_cast<float>(FMath::Max(0.001, EarliestDeadline - World->GetTimeSeconds()));
	World->GetTimerManager().SetTimer(PresentationTimerHandle, this, &ThisClass::HandlePresentationTimer, Delay, false);
}

void URpgHarvestableInstancesComponent::HandlePresentationTimer()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	TArray<int32, TInlineAllocator<8>> DueInstances;
	for (const TPair<int32, double>& Pending : PendingPresentations)
	{
		if (Pending.Value <= Now + UE_KINDA_SMALL_NUMBER)
		{
			DueInstances.Add(Pending.Key);
		}
	}
	for (const int32 InstanceIndex : DueInstances)
	{
		PendingPresentations.Remove(InstanceIndex);
		PresentInstance(InstanceIndex, false);
	}
	ArmPresentationTimer();
}

void URpgHarvestableInstancesComponent::PresentInstance(const int32 InstanceIndex, const bool bInitialState)
{
	const FRpgHarvestStockSnapshot Stock = MakeStockSnapshot(InstanceIndex);
	if (bHideDepletedInstances)
	{
		SetInstancePresentationScale(InstanceIndex, Stock.bActive ? 1.0f : 0.0f);
		SetLinkedInstancesVisible(InstanceIndex, Stock.bActive);
	}
	OnInstanceStockChanged(InstanceIndex, Stock.GetRemainingSections(), GetSectionCount(), Stock.bActive, bInitialState);
}
