#include "Harvesting/RpgHarvestRuneStrike.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestRuneStrike)

bool FRpgHarvestEchoCharge::AddHit(const double Now, const int32 HitsPerEcho, const float TimeoutSeconds)
{
	if (HitsPerEcho < 1)
	{
		Reset();
		return false;
	}
	if (Hits > 0 && Now > Deadline)
	{
		Hits = 0;
	}
	++Hits;
	Deadline = Now + FMath::Max(0.0f, TimeoutSeconds);
	if (Hits < HitsPerEcho)
	{
		return false;
	}
	Reset();
	return true;
}

void FRpgHarvestEchoCharge::Reset()
{
	Hits = 0;
	Deadline = -1.0;
}

void FRpgHarvestRuneFlow::PickFollowUpTargets(
	const TArray<FRpgHarvestTargetEvaluation>& Candidates,
	const UObject* ExcludeReceiver,
	const int32 ExcludeItem,
	const int32 MaxTargets,
	TArray<FRpgHarvestTargetEvaluation>& OutTargets)
{
	const FRpgHarvestResourceId Excluded{ExcludeReceiver, ExcludeItem};
	PickFollowUpTargets(Candidates, MakeArrayView(&Excluded, 1), MaxTargets, OutTargets);
}

void FRpgHarvestRuneFlow::PickFollowUpTargets(
	const TArray<FRpgHarvestTargetEvaluation>& Candidates,
	const TConstArrayView<FRpgHarvestResourceId> Excluded,
	const int32 MaxTargets,
	TArray<FRpgHarvestTargetEvaluation>& OutTargets)
{
	int32 Picked = 0;
	for (const FRpgHarvestTargetEvaluation& Candidate : Candidates)
	{
		if (Picked >= MaxTargets)
		{
			break;
		}
		if (!Candidate.WouldHarvest() || !Candidate.Receiver.IsValid() ||
			Excluded.ContainsByPredicate([&Candidate](const FRpgHarvestResourceId& Resource) { return Resource.Matches(Candidate); }))
		{
			continue;
		}
		OutTargets.Add(Candidate);
		++Picked;
	}
}

void FRpgHarvestRuneFlow::PickPierceTargets(
	const TArray<FRpgHarvestTargetEvaluation>& Candidates,
	const FRpgHarvestResourceId& Excluded,
	const FVector& Origin,
	const FVector& Direction,
	const float ConeHalfAngleDegrees,
	const int32 MaxTargets,
	TArray<FRpgHarvestTargetEvaluation>& OutTargets)
{
	const FVector Forward = Direction.GetSafeNormal2D();
	if (Forward.IsNearlyZero() || MaxTargets <= 0)
	{
		return;
	}
	const double MinCos = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ConeHalfAngleDegrees, 0.0f, 89.0f)));
	int32 Picked = 0;
	for (const FRpgHarvestTargetEvaluation& Candidate : Candidates)
	{
		if (Picked >= MaxTargets)
		{
			break;
		}
		if (!Candidate.WouldHarvest() || !Candidate.Receiver.IsValid() || Excluded.Matches(Candidate))
		{
			continue;
		}
		// Seen from above, the resource has to lie ahead of the struck one, inside the cone around the hit's direction.
		const FVector ToCandidate = (Candidate.Hit.ImpactPoint - Origin) * FVector(1.0, 1.0, 0.0);
		const double Distance = ToCandidate.Size();
		if (Distance < UE_KINDA_SMALL_NUMBER || FVector::DotProduct(ToCandidate / Distance, Forward) < MinCos)
		{
			continue;
		}
		OutTargets.Add(Candidate);
		++Picked;
	}
}

void FRpgHarvestRuneFlow::PlanLeaps(
	const FRpgHarvestResourceId& Struck,
	const FVector& StruckPoint,
	const int32 Jumps,
	const int32 TargetsPerJump,
	const TFunctionRef<void(const FVector& Center, int32 MaxTargets, TArray<FRpgHarvestTargetEvaluation>& OutCandidates)> CollectCandidates,
	TArray<FRpgHarvestLeapStrike>& OutStrikes)
{
	if (Jumps <= 0 || TargetsPerJump <= 0)
	{
		return;
	}

	// Every struck resource stays excluded, so the chain only ever moves on and ends after Jumps steps at the latest.
	TArray<FRpgHarvestResourceId, TInlineAllocator<16>> Visited;
	Visited.Add(Struck);
	FVector From = StruckPoint;
	TArray<FRpgHarvestTargetEvaluation> Candidates;
	TArray<FRpgHarvestTargetEvaluation> Picked;
	for (int32 Step = 0; Step < Jumps; ++Step)
	{
		Candidates.Reset();
		Picked.Reset();
		CollectCandidates(From, Visited.Num() + TargetsPerJump, Candidates);
		PickFollowUpTargets(Candidates, Visited, TargetsPerJump, Picked);
		if (Picked.IsEmpty())
		{
			break;
		}
		for (FRpgHarvestTargetEvaluation& Target : Picked)
		{
			Visited.Add(FRpgHarvestResourceId{Target.Receiver.Get(), Target.Hit.Item});
			OutStrikes.Add(FRpgHarvestLeapStrike{Target, Step});
		}
		From = Picked[0].Hit.ImpactPoint;
	}
}

float FRpgHarvestRuneFlow::TransferSections(const FRpgHarvestResult& Result, const float Fraction)
{
	if (!Result.IsSuccess() || Result.OverkillWork <= 0 || !FMath::IsFinite(Fraction) || Fraction <= 0.0f)
	{
		return 0.0f;
	}
	return static_cast<float>(Result.OverkillWork) / static_cast<float>(RpgHarvestWork::UnitsPerSection) * Fraction;
}

ARpgHarvestRuneStrike::ARpgHarvestRuneStrike(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	bReplicates = true;
	SetReplicatingMovement(false);
	// The strike never changes replicated state after it spawned; clients only need to know it exists.
	SetNetUpdateFrequency(1.0f);
	PrimaryActorTick.bCanEverTick = false;
	LastResult = FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Invalid);
}

ARpgHarvestRuneStrike* ARpgHarvestRuneStrike::Launch(
	UObject* Receiver,
	const FHitResult& TargetHit,
	const FRpgHarvestRequest& DirectRequest,
	const TSubclassOf<ARpgHarvestRuneStrike> StrikeClass,
	const float WorkSections,
	const bool bOtherResource,
	const float ExtraDelaySeconds)
{
	UWorld* World = Receiver ? Receiver->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !StrikeClass || !IsValid(DirectRequest.Harvester) ||
		!FMath::IsFinite(WorkSections) || WorkSections < 0.01f)
	{
		return nullptr;
	}

	// The player state receives the rewards, so they arrive even when the striking pawn dies meanwhile.
	AActor* Beneficiary = FRpgHarvestRewardService::ResolveHarvesterPlayerState(DirectRequest.Harvester);
	if (!Beneficiary)
	{
		Beneficiary = DirectRequest.Harvester;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgHarvestRuneStrike* RuneStrike = World->SpawnActor<ARpgHarvestRuneStrike>(
		StrikeClass,
		FTransform(TargetHit.ImpactPoint),
		SpawnParameters);
	if (RuneStrike && !RuneStrike->Start(Receiver, Beneficiary, TargetHit, DirectRequest, WorkSections, bOtherResource, ExtraDelaySeconds))
	{
		RuneStrike->Destroy();
		RuneStrike = nullptr;
	}
	return RuneStrike;
}

void ARpgHarvestRuneStrike::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StrikeTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool ARpgHarvestRuneStrike::Start(
	UObject* InReceiver,
	AActor* Beneficiary,
	const FHitResult& TargetHit,
	const FRpgHarvestRequest& DirectRequest,
	const float WorkSections,
	const bool bOtherResource,
	const float ExtraDelaySeconds)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !InReceiver || !Beneficiary)
	{
		return false;
	}

	// The strike is follow-up work of the triggering action, for its player. The resource has to keep its stock until
	// the strike lands: a depletion or respawn changes the revision, and the commit then takes nothing.
	Receiver = InReceiver;
	Request = DirectRequest;
	Request.Harvester = Beneficiary;
	Request.PhysicalHarvester = this;
	Request.Hit = TargetHit;
	Request.TraceOrigin = DirectRequest.Hit.ImpactPoint;
	Request.RequestedSections = 1;
	Request.WorkScale = FMath::Clamp(WorkSections, 0.01f, RpgHarvestWork::MaxWorkScale);
	Request.bCanHitWeakPoint = false;
	Request.bFinishResource = false;
	Request.bAreaHarvest = DirectRequest.bAreaHarvest || bOtherResource;
	Request.PresentationDelaySeconds = 0.0f;
	Request.Provenance = DirectRequest.Provenance.MakeSecondary(ProcTag);
	Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(InReceiver, TargetHit);

	const float Extra = FMath::IsFinite(ExtraDelaySeconds) ? FMath::Clamp(ExtraDelaySeconds, 0.0f, 8.0f) : 0.0f;
	World->GetTimerManager().SetTimer(StrikeTimerHandle, this, &ThisClass::Strike, FMath::Max(0.01f, DelaySeconds + Extra), false);
	return true;
}

void ARpgHarvestRuneStrike::Strike()
{
	UObject* Target = Receiver.Get();
	if (Target && IsValid(Request.Harvester))
	{
		LastResult = IRpgHarvestableTarget::Execute_CommitHarvest(Target, Request);
	}
	bStruck = true;
	if (LingerSeconds > 0.0f)
	{
		SetLifeSpan(LingerSeconds);
	}
	else
	{
		Destroy();
	}
}
