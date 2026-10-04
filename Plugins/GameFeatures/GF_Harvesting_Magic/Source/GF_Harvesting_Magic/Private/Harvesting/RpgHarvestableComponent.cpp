#include "Harvesting/RpgHarvestableComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestableComponent)

URpgHarvestableComponent::URpgHarvestableComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URpgHarvestableComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* OwningActor = GetOwner();
	if (OwningActor && OwningActor->HasAuthority())
	{
		if (!OwningActor->GetIsReplicated())
		{
			UE_LOG(
				LogRpgHarvesting,
				Warning,
				TEXT("%s is owned by a non-replicated actor; clients will not see its harvest state."),
				*GetPathName());
		}
		if (!HarvestProfile)
		{
			UE_LOG(
				LogRpgHarvesting,
				Warning,
				TEXT("%s has no harvest profile and rejects every harvest request."),
				*GetPathName());
		}
	}

	BroadcastStateChanged(true);
}

void URpgHarvestableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void URpgHarvestableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, HarvestState);
}

int32 URpgHarvestableComponent::GetHarvestRevision_Implementation(const FHitResult& Hit) const
{
	return GetOwner() && Hit.GetActor() == GetOwner()
		? HarvestState.Revision
		: INDEX_NONE;
}

FRpgHarvestResult URpgHarvestableComponent::EvaluateHarvest_Implementation(const FRpgHarvestRequest& Request) const
{
	const AActor* OwningActor = GetOwner();
	if (!OwningActor || !HarvestProfile || Request.Hit.GetActor() != OwningActor)
	{
		return FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Invalid, GetRemainingSections(), GetSectionCount());
	}

	FRpgHarvestWeakPoint WeakPoint;
	const bool bHasWeakPoint = GetActiveWeakPoint(WeakPoint.Location, WeakPoint.Radius);
	const FRpgHarvestResult Result = FRpgHarvestStockRules::Evaluate(
		HarvestProfile,
		Request,
		MakeStockSnapshot(),
		bHasWeakPoint ? &WeakPoint : nullptr);
	if (Result.IsSuccess() && bCommitInProgress)
	{
		return FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Stale, GetRemainingSections(), GetSectionCount());
	}
	return Result;
}

FRpgHarvestResult URpgHarvestableComponent::CommitHarvest_Implementation(const FRpgHarvestRequest& Request)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Invalid, GetRemainingSections(), GetSectionCount());
	}

	// Re-run the complete validation at the moment of extraction. Abilities may have spent time
	// on montages or costs since they selected this target, and other harvesters share the stock.
	FRpgHarvestResult Result = EvaluateHarvest_Implementation(Request);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TGuardValue<bool> CommitGuard(bCommitInProgress, true);

	FTransform DeliveryTransform = OwningActor->GetActorTransform();
	if (!Request.Hit.ImpactPoint.IsNearlyZero())
	{
		DeliveryTransform.SetLocation(Request.Hit.ImpactPoint);
	}

	FRpgHarvestRewardRequest RewardRequest;
	RewardRequest.SourceActor = OwningActor;
	RewardRequest.Harvester = Request.Harvester;
	RewardRequest.DeliveryTransform = DeliveryTransform;
	RewardRequest.HarvestPower = Request.HarvestPower;
	RewardRequest.RollCount = Result.SectionsTaken;
	RewardRequest.SeedSalt = HashCombine(
		GetTypeHash(HarvestState.Revision),
		GetTypeHash(HarvestState.HarvestedSections));
	const ERpgHarvestRewardDeliveryResult DeliveryResult =
		FRpgHarvestRewardService::DeliverReward(HarvestProfile, RewardRequest);
	if (DeliveryResult == ERpgHarvestRewardDeliveryResult::Failed)
	{
		// Nothing was granted, so the stock stays untouched and the request may be retried.
		return FRpgHarvestResult::MakeRejected(
			ERpgHarvestOutcome::DeliveryFailed,
			GetRemainingSections(),
			GetSectionCount());
	}

	FRpgHarvestNodeState NewState = HarvestState;
	NewState.HarvestedSections = static_cast<uint8>(FMath::Clamp(
		static_cast<int32>(HarvestState.HarvestedSections) + Result.SectionsTaken,
		0,
		URpgHarvestProfile::MaxSectionCount));
	Result.bDepleted = Result.RemainingSections <= 0;
	if (Result.bDepleted)
	{
		NewState.bActive = false;
		NewState.Revision = FMath::Max(1, HarvestState.Revision + 1);
	}
	SetAuthoritativeState(NewState);

	FRpgHarvestRewardService::AwardExperience(HarvestProfile, Request.Harvester, Result.SectionsTaken);
	if (Result.bDepleted)
	{
		ScheduleRespawn();
	}

	Result.Delivery = FRpgHarvestStockRules::ToDelivery(DeliveryResult);
	OnHarvested.Broadcast(Request, Result);
	return Result;
}

int32 URpgHarvestableComponent::GetSectionCount() const
{
	return HarvestProfile ? HarvestProfile->GetClampedSectionCount() : 0;
}

int32 URpgHarvestableComponent::GetRemainingSections() const
{
	return HarvestState.bActive
		? FMath::Max(0, GetSectionCount() - static_cast<int32>(HarvestState.HarvestedSections))
		: 0;
}

bool URpgHarvestableComponent::IsHarvestable() const
{
	return GetRemainingSections() > 0;
}

bool URpgHarvestableComponent::GetActiveWeakPoint(FVector& OutWorldLocation, float& OutRadius) const
{
	OutWorldLocation = FVector::ZeroVector;
	OutRadius = 0.0f;
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !HarvestProfile || HarvestProfile->GetClampedWeakPointBonusSections() <= 0 || WeakPointRadius <= 0.0f)
	{
		return false;
	}

	const int32 WeakPointIndex = FRpgHarvestStockRules::GetActiveWeakPointIndex(MakeStockSnapshot(), WeakPointLocations.Num());
	if (!WeakPointLocations.IsValidIndex(WeakPointIndex))
	{
		return false;
	}

	const USceneComponent* Frame = Cast<USceneComponent>(WeakPointFrame.GetComponent(OwningActor));
	const FTransform FrameTransform = Frame ? Frame->GetComponentTransform() : OwningActor->GetActorTransform();
	OutWorldLocation = FrameTransform.TransformPosition(WeakPointLocations[WeakPointIndex]);
	OutRadius = WeakPointRadius;
	return true;
}

bool URpgHarvestableComponent::RestoreHarvestStock()
{
	const AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || (HarvestState.bActive && HarvestState.HarvestedSections == 0))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	FRpgHarvestNodeState NewState = HarvestState;
	if (!NewState.bActive)
	{
		NewState.Revision = FMath::Max(1, HarvestState.Revision + 1);
	}
	NewState.bActive = true;
	NewState.HarvestedSections = 0;
	SetAuthoritativeState(NewState);
	return true;
}

FRpgHarvestStockSnapshot URpgHarvestableComponent::MakeStockSnapshot() const
{
	FRpgHarvestStockSnapshot Stock;
	Stock.Revision = HarvestState.Revision;
	Stock.SectionCount = GetSectionCount();
	Stock.HarvestedSections = HarvestState.HarvestedSections;
	Stock.bActive = HarvestState.bActive;
	return Stock;
}

void URpgHarvestableComponent::OnRep_HarvestState()
{
	// A change older than the live window was made before this client received the actor
	// (late join, relevancy, or a dormancy flush) and is presented as initial state.
	const bool bInitialState =
		GetServerWorldTimeSeconds() - HarvestState.LastChangeServerTime > LiveChangeWindowSeconds;
	BroadcastStateChanged(bInitialState);
}

void URpgHarvestableComponent::SetAuthoritativeState(const FRpgHarvestNodeState& NewState)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	OwningActor->FlushNetDormancy();
	HarvestState = NewState;
	HarvestState.LastChangeServerTime = GetServerWorldTimeSeconds();
	OwningActor->ForceNetUpdate();
	BroadcastStateChanged(false);
}

void URpgHarvestableComponent::BroadcastStateChanged(const bool bInitialState)
{
	OnHarvestStateChanged.Broadcast(
		this,
		GetRemainingSections(),
		GetSectionCount(),
		HarvestState.bActive,
		bInitialState);
}

float URpgHarvestableComponent::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void URpgHarvestableComponent::ScheduleRespawn()
{
	UWorld* World = GetWorld();
	if (!HarvestProfile || !World)
	{
		return;
	}

	const float MinimumDelay = FMath::Max(0.0f, HarvestProfile->MinimumRespawnSeconds);
	const float MaximumDelay = FMath::Max(MinimumDelay, HarvestProfile->MaximumRespawnSeconds);
	if (MaximumDelay <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::HandleRespawnTimer,
		FMath::Max(0.001f, FMath::FRandRange(MinimumDelay, MaximumDelay)),
		false);
}

void URpgHarvestableComponent::HandleRespawnTimer()
{
	const AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority() || HarvestState.bActive)
	{
		return;
	}

	FRpgHarvestNodeState NewState = HarvestState;
	NewState.Revision = FMath::Max(1, HarvestState.Revision + 1);
	NewState.HarvestedSections = 0;
	NewState.bActive = true;
	SetAuthoritativeState(NewState);
}
