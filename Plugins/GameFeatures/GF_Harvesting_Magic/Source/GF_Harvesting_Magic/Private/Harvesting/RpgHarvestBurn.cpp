#include "Harvesting/RpgHarvestBurn.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestBurn)

ARpgHarvestBurn::ARpgHarvestBurn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	bReplicates = true;
	SetReplicatingMovement(false);
	// The burn never changes replicated state after it spawned; clients only need to know it exists.
	SetNetUpdateFrequency(1.0f);
	PrimaryActorTick.bCanEverTick = false;
	LastResult = FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Invalid);
}

ARpgHarvestBurn* ARpgHarvestBurn::Light(
	UObject* Receiver,
	const FRpgHarvestRequest& DirectRequest,
	const TSubclassOf<ARpgHarvestBurn> BurnClass,
	const FRpgHarvestBurnParams& Params)
{
	UWorld* World = Receiver ? Receiver->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || !BurnClass || !IsValid(DirectRequest.Harvester) ||
		!FMath::IsFinite(Params.WorkPerSecond) || Params.WorkPerSecond <= 0.0f ||
		!FMath::IsFinite(Params.DurationSeconds) || Params.DurationSeconds <= 0.0f)
	{
		return nullptr;
	}

	// The player state receives the rewards, so they arrive even when the igniting pawn dies meanwhile.
	AActor* Beneficiary = FRpgHarvestRewardService::ResolveHarvesterPlayerState(DirectRequest.Harvester);
	if (!Beneficiary)
	{
		Beneficiary = DirectRequest.Harvester;
	}
	if (ARpgHarvestBurn* Existing = FindBurn(Receiver, DirectRequest.Hit.Item, Beneficiary))
	{
		if (FMath::IsFinite(Params.DischargeMultiplier) && Params.DischargeMultiplier > 0.0f)
		{
			Existing->Discharge(Params.DischargeMultiplier);
			return nullptr;
		}
		if (Params.bRefresh)
		{
			Existing->Refresh(Params.DurationSeconds);
		}
		return Existing;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgHarvestBurn* Burn = World->SpawnActor<ARpgHarvestBurn>(
		BurnClass,
		FTransform(DirectRequest.Hit.ImpactPoint),
		SpawnParameters);
	if (Burn && !Burn->Start(Receiver, Beneficiary, DirectRequest, Params))
	{
		Burn->Destroy();
		Burn = nullptr;
	}
	return Burn;
}

ARpgHarvestBurn* ARpgHarvestBurn::FindBurn(const UObject* Receiver, const int32 Item, const AActor* Beneficiary)
{
	UWorld* World = Receiver ? Receiver->GetWorld() : nullptr;
	if (!World || !Beneficiary)
	{
		return nullptr;
	}

	for (ARpgHarvestBurn* Burn : TActorRange<ARpgHarvestBurn>(World))
	{
		if (IsValid(Burn) && !Burn->IsActorBeingDestroyed() && Burn->Receiver.Get() == Receiver && Burn->Item == Item &&
			Burn->GetBeneficiary() == Beneficiary)
		{
			return Burn;
		}
	}
	return nullptr;
}

void ARpgHarvestBurn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool ARpgHarvestBurn::Start(
	UObject* InReceiver,
	AActor* Beneficiary,
	const FRpgHarvestRequest& DirectRequest,
	const FRpgHarvestBurnParams& Params)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !InReceiver || !Beneficiary)
	{
		return false;
	}

	const float Interval = FMath::Max(0.1f, TickIntervalSeconds);
	Receiver = InReceiver;
	Item = DirectRequest.Hit.Item;
	WorkPerSecond = Params.WorkPerSecond;
	RemainingTicks = TicksFor(Params.DurationSeconds);
	WanderRadius = FMath::IsFinite(Params.WanderRadius) ? FMath::Max(0.0f, Params.WanderRadius) : 0.0f;
	WanderChannel = Params.WanderChannel;

	// Every commit is follow-up work of the igniting action, for its player. The resource it worked has to stay the same
	// stock: a depletion or respawn changes the revision, and the next commit then puts the burn out.
	RequestTemplate = DirectRequest;
	RequestTemplate.Harvester = Beneficiary;
	RequestTemplate.PhysicalHarvester = this;
	RequestTemplate.RequestedSections = 1;
	RequestTemplate.WorkScale = FMath::Clamp(WorkPerSecond * Interval, 0.01f, RpgHarvestWork::MaxWorkScale);
	RequestTemplate.bCanHitWeakPoint = false;
	RequestTemplate.bFinishResource = false;
	RequestTemplate.PresentationDelaySeconds = 0.0f;
	RequestTemplate.Provenance = DirectRequest.Provenance.MakeSecondary(ProcTag);
	RequestTemplate.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(InReceiver, DirectRequest.Hit);

	World->GetTimerManager().SetTimer(BurnTimerHandle, this, &ThisClass::Burn, Interval, true);
	return true;
}

int32 ARpgHarvestBurn::TicksFor(const float DurationSeconds) const
{
	return FMath::Max(1, FMath::RoundToInt(DurationSeconds / FMath::Max(0.1f, TickIntervalSeconds)));
}

void ARpgHarvestBurn::Refresh(const float DurationSeconds)
{
	RemainingTicks = FMath::Max(RemainingTicks, TicksFor(DurationSeconds));
}

void ARpgHarvestBurn::Discharge(const float Multiplier)
{
	UObject* Target = Receiver.Get();
	if (Target && IsValid(RequestTemplate.Harvester) && RemainingTicks > 0)
	{
		// The work of every tick still to come, released in one commit.
		FRpgHarvestRequest Request = RequestTemplate;
		Request.WorkScale = FMath::Clamp(
			RequestTemplate.WorkScale * RemainingTicks * Multiplier,
			0.01f,
			RpgHarvestWork::MaxWorkScale);
		LastResult = IRpgHarvestableTarget::Execute_CommitHarvest(Target, Request);
	}
	Extinguish();
}

void ARpgHarvestBurn::Burn()
{
	if (!IsValid(RequestTemplate.Harvester))
	{
		Extinguish();
		return;
	}

	// A resource that is gone, emptied or respawned, by this burn or by any hit, ends the burn here; a wandering burn
	// carries its remaining ticks on. Every tick counts, so the wandering always ends.
	UObject* Target = Receiver.Get();
	LastResult = Target
		? IRpgHarvestableTarget::Execute_CommitHarvest(Target, RequestTemplate)
		: FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Depleted);
	--RemainingTicks;
	const bool bEmptied = LastResult.IsSuccess()
		? LastResult.RemainingWork <= 0
		: LastResult.Outcome == ERpgHarvestOutcome::Depleted || LastResult.Outcome == ERpgHarvestOutcome::Stale;
	if (bEmptied && RemainingTicks > 0 && WanderRadius > 0.0f && Wander())
	{
		Extinguish();
		return;
	}
	if (!LastResult.IsSuccess() || LastResult.RemainingWork <= 0 || RemainingTicks <= 0)
	{
		Extinguish();
	}
}

bool ARpgHarvestBurn::Wander()
{
	UWorld* World = GetWorld();
	AActor* Beneficiary = RequestTemplate.Harvester;
	if (!World || !Beneficiary)
	{
		return false;
	}

	// The next resource is one the hit's tool could work there: an area request, so protected resources stay out.
	FRpgHarvestRequest CandidateTemplate = RequestTemplate;
	CandidateTemplate.WorkScale = 1.0f;
	CandidateTemplate.bAreaHarvest = true;
	const FVector Origin = GetActorLocation();
	TArray<FRpgHarvestTargetEvaluation> Candidates;
	FRpgHarvestTargeting::CollectAreaTargets(
		*World,
		Origin,
		WanderRadius,
		WanderChannel,
		FCollisionQueryParams(SCENE_QUERY_STAT(RpgHarvestBurnWander), false),
		Origin,
		true,
		CandidateTemplate,
		0,
		Candidates);
	for (const FRpgHarvestTargetEvaluation& Candidate : Candidates)
	{
		UObject* NextReceiver = Candidate.Receiver.Get();
		if (!Candidate.WouldHarvest() || !NextReceiver || (NextReceiver == Receiver.Get() && Candidate.Hit.Item == Item) ||
			FindBurn(NextReceiver, Candidate.Hit.Item, Beneficiary))
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgHarvestBurn* Next = World->SpawnActor<ARpgHarvestBurn>(GetClass(), FTransform(Candidate.Hit.ImpactPoint), SpawnParameters);
		if (!Next)
		{
			return false;
		}
		FRpgHarvestRequest NextRequest = RequestTemplate;
		NextRequest.Hit = Candidate.Hit;
		NextRequest.WorkScale = 1.0f;
		FRpgHarvestBurnParams Params;
		Params.WorkPerSecond = WorkPerSecond;
		Params.DurationSeconds = RemainingTicks * FMath::Max(0.1f, TickIntervalSeconds);
		Params.WanderRadius = WanderRadius;
		Params.WanderChannel = WanderChannel;
		if (!Next->Start(NextReceiver, Beneficiary, NextRequest, Params))
		{
			Next->Destroy();
			return false;
		}
		WanderedTo = Next;
		return true;
	}
	return false;
}

void ARpgHarvestBurn::Extinguish()
{
	RemainingTicks = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurnTimerHandle);
	}
	Destroy();
}
