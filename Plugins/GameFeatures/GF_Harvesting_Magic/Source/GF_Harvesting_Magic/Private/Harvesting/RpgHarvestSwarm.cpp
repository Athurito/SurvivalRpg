#include "Harvesting/RpgHarvestSwarm.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Harvesting/RpgHarvestRewardService.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgHarvestSwarm)

namespace RpgHarvestSwarm
{
	/** Seconds the swarm actor stays after its last creature finished, so clients receive the final phases. */
	constexpr float FinishedLingerSeconds = 1.5f;

	/** Resources a sight line may pass before it counts as clear. */
	constexpr int32 MaxSightLineResources = 8;

	/** Shortest flight in seconds, so a creature never strikes in the frame it leaves. */
	constexpr double MinimumFlightSeconds = 0.05;

	/** Tolerance in seconds when deciding whether a scheduled departure or arrival is due. */
	constexpr double DueTolerance = 1.0e-4;

	/** Identity of one target: its receiver and, for instanced receivers, its instance. */
	TPair<const UObject*, int32> MakeTargetKey(const UObject* Receiver, const FHitResult& Hit)
	{
		return TPair<const UObject*, int32>(Receiver, Receiver && Receiver == Hit.GetComponent() ? Hit.Item : INDEX_NONE);
	}
}

void FRpgHarvestSwarmPlanner::Distribute(
	const TArray<FRpgHarvestTargetEvaluation>& Targets,
	const int32 CreatureCount,
	const int32 SectionsPerCreature,
	TArray<int32>& OutTargetIndices,
	TArray<int32>& OutSections)
{
	OutTargetIndices.Reset();
	OutSections.Reset();
	const int32 Creatures = FMath::Clamp(CreatureCount, 0, ARpgHarvestSwarm::MaxCreatures);
	const int32 PerCreature = FMath::Max(1, SectionsPerCreature);

	// Creatures cover the nearest resource's remaining stock before they move on to the next one, so a swarm empties
	// resources instead of leaving many of them half harvested.
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num() && OutTargetIndices.Num() < Creatures; ++TargetIndex)
	{
		const FRpgHarvestTargetEvaluation& Target = Targets[TargetIndex];
		int32 Unreserved = Target.WouldHarvest() ? GetAvailableSections(Target) : 0;
		while (Unreserved > 0 && OutTargetIndices.Num() < Creatures)
		{
			const int32 Sections = FMath::Min(PerCreature, Unreserved);
			Unreserved -= Sections;
			OutTargetIndices.Add(TargetIndex);
			OutSections.Add(Sections);
		}
	}
}

int32 FRpgHarvestSwarmPlanner::GetAvailableSections(const FRpgHarvestTargetEvaluation& Target)
{
	return FMath::Max(0, Target.Result.SectionsTaken + Target.Result.RemainingSections);
}

bool FRpgHarvestSwarmPlanner::HasLineOfSight(
	const UWorld& World,
	const FVector& Origin,
	const FVector& StrikePoint,
	const ECollisionChannel Channel,
	const FCollisionQueryParams& QueryParams)
{
	FCollisionQueryParams SightParams = QueryParams;
	for (int32 Pass = 0; Pass <= RpgHarvestSwarm::MaxSightLineResources; ++Pass)
	{
		FHitResult Hit;
		if (!World.LineTraceSingleByChannel(Hit, Origin, StrikePoint, Channel, SightParams))
		{
			return true;
		}

		// Creatures fly past other resources and pawns; anything else, such as a wall or a hill, blocks them.
		UPrimitiveComponent* Component = Hit.GetComponent();
		if (!Component || (!FRpgHarvestTargeting::FindReceiver(Hit) && !Cast<APawn>(Hit.GetActor())))
		{
			return false;
		}
		SightParams.AddIgnoredComponent(Component);
	}
	return true;
}

ARpgHarvestSwarm::ARpgHarvestSwarm(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	bReplicates = true;
	SetReplicatingMovement(false);
	// Every change forces an update; the low rate only bounds idle checks.
	SetNetUpdateFrequency(10.0f);
	// Only the cosmetic creature actors tick, and only while they move.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

ARpgHarvestSwarm::~ARpgHarvestSwarm() = default;

bool ARpgHarvestSwarm::StartSwarm(
	AActor* InSummoner,
	const FRpgHarvestRequest& InRequestTemplate,
	const FRpgHarvestSwarmParams& InParams,
	const float InSearchRadius,
	const ECollisionChannel Channel,
	const TArray<FRpgHarvestTargetEvaluation>& InitialTargets)
{
	if (bStarted || !HasAuthority() || !GetWorld() || !IsValid(InSummoner))
	{
		return false;
	}
	bStarted = true;
	Summoner = InSummoner;
	SwarmParams = InParams;
	SwarmParams.CreatureCount = FMath::Clamp(InParams.CreatureCount, 1, MaxCreatures);
	SwarmParams.FlightSpeed = FMath::Max(50.0f, InParams.FlightSpeed);
	SearchRadius = FMath::Max(0.0f, InSearchRadius);
	SearchChannel = Channel;

	// The player state receives the rewards, so they arrive even when the summoner's pawn dies first. The swarm itself
	// strikes the resources.
	AActor* Beneficiary = FRpgHarvestRewardService::ResolveHarvesterPlayerState(InSummoner);
	if (!Beneficiary)
	{
		Beneficiary = InSummoner;
	}
	RequestTemplate = InRequestTemplate;
	RequestTemplate.Harvester = Beneficiary;
	RequestTemplate.PhysicalHarvester = this;
	RequestTemplate.RequestedSections = FMath::Max(1, InRequestTemplate.RequestedSections);
	RequestTemplate.bAreaHarvest = true;
	RequestTemplate.bCanHitWeakPoint = false;
	RequestTemplate.PresentationDelaySeconds = 0.0f;
	RewardBatch = MakeUnique<FRpgHarvestRewardBatch>(Beneficiary, false);

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InSummoner);
	if (!AbilitySystem)
	{
		AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Beneficiary);
	}
	if (AbilitySystem)
	{
		SummonerAbilitySystem = AbilitySystem;
		DeathTagHandle = AbilitySystem
			->RegisterGameplayTagEvent(RpgGameplayTags::Status_Death, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleSummonerDeathTagChanged);
	}

	const double Now = GetServerWorldTimeSeconds();
	SummonServerTime = Now;
	ExpireServerTime = Now + FMath::Max(0.5f, SwarmParams.MaxLifetimeSeconds);

	TArray<int32> TargetIndices;
	TArray<int32> Sections;
	FRpgHarvestSwarmPlanner::Distribute(
		InitialTargets,
		SwarmParams.CreatureCount,
		RequestTemplate.RequestedSections,
		TargetIndices,
		Sections);

	const FVector Center = GetActorLocation();
	const FVector Lift(0.0, 0.0, StrikeHeight);
	Creatures.SetNum(SwarmParams.CreatureCount);
	Assignments.SetNum(SwarmParams.CreatureCount);
	for (int32 CreatureIndex = 0; CreatureIndex < Creatures.Num(); ++CreatureIndex)
	{
		FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
		const double Angle = UE_TWO_PI * CreatureIndex / Creatures.Num();
		FVector RingDirection(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		Creature.LaunchServerTime =
			Now + FMath::Max(0.0f, SwarmParams.EmergeSeconds) + CreatureIndex * FMath::Max(0.0f, SwarmParams.LaunchIntervalSeconds);

		const FRpgHarvestTargetEvaluation* Target =
			TargetIndices.IsValidIndex(CreatureIndex) ? &InitialTargets[TargetIndices[CreatureIndex]] : nullptr;
		UObject* Receiver = Target ? Target->Receiver.Get() : nullptr;
		if (Receiver)
		{
			// A creature rises on the side of the ring that faces its resource, never beyond a resource close by.
			// Creatures that share a resource rise side by side instead of on one spot.
			const FVector StrikePoint = Target->Hit.ImpactPoint + Lift;
			const FVector TowardTarget = (StrikePoint - Center).GetSafeNormal2D();
			int32 SharedBefore = 0;
			for (int32 Earlier = 0; Earlier < CreatureIndex; ++Earlier)
			{
				SharedBefore += TargetIndices[Earlier] == TargetIndices[CreatureIndex] ? 1 : 0;
			}
			const double SpreadDegrees = SharedBefore == 0
				? 0.0
				: (SharedBefore % 2 == 1 ? 35.0 : -35.0) * ((SharedBefore + 1) / 2);
			RingDirection = TowardTarget.IsNearlyZero()
				? RingDirection
				: TowardTarget.RotateAngleAxis(SpreadDegrees, FVector::UpVector);
			const double RingDistance = FMath::Min<double>(EmergeRingRadius, 0.5 * FVector::Dist2D(Center, StrikePoint));
			Creature.From = Center + RingDirection * RingDistance + Lift;
			Creature.To = StrikePoint;
			Creature.State = ERpgHarvestSwarmCreatureState::Flying;
			Creature.ArrivalServerTime = Creature.LaunchServerTime +
				FMath::Max(RpgHarvestSwarm::MinimumFlightSeconds, FVector::Dist(Creature.From, Creature.To) / SwarmParams.FlightSpeed);

			FAssignment& Assignment = Assignments[CreatureIndex];
			Assignment.Receiver = Receiver;
			Assignment.Hit = Target->Hit;
			Assignment.ReservedSections = Sections[CreatureIndex];
			Assignment.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Receiver, Target->Hit);
		}
		else
		{
			// Creatures without a resource look again when they would leave.
			Creature.From = Center + RingDirection * EmergeRingRadius + Lift;
			Creature.To = Creature.From;
			Creature.ArrivalServerTime = Creature.LaunchServerTime;
			Creature.State = ERpgHarvestSwarmCreatureState::Searching;
		}
	}

	ReplicateCreatures();
	ScheduleStep();
	return true;
}

bool ARpgHarvestSwarm::GetCreatureLocation(const int32 CreatureIndex, FVector& OutLocation) const
{
	if (!Creatures.IsValidIndex(CreatureIndex))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}
	OutLocation = GetCreatureLocationAt(CreatureIndex, GetServerWorldTimeSeconds());
	return true;
}

FVector ARpgHarvestSwarm::GetCreatureLocationAt(const int32 CreatureIndex, const double ServerTime) const
{
	if (!Creatures.IsValidIndex(CreatureIndex))
	{
		return FVector::ZeroVector;
	}

	const FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
	const FVector From = Creature.From;
	const FVector To = Creature.To;
	if (ServerTime <= Creature.LaunchServerTime)
	{
		if (Creature.Leg == 0 && Creature.LaunchServerTime > SummonServerTime)
		{
			// Before its first departure the creature rises out of the ground, slowing down toward its start.
			const double Rise = FMath::Clamp(
				(ServerTime - SummonServerTime) / (Creature.LaunchServerTime - SummonServerTime),
				0.0,
				1.0);
			return From - FVector(0.0, 0.0, EmergeDepth * FMath::Square(1.0 - Rise));
		}
		return From;
	}

	const double Duration = FMath::Max(UE_KINDA_SMALL_NUMBER, Creature.ArrivalServerTime - Creature.LaunchServerTime);
	const double Alpha = FMath::Clamp((ServerTime - Creature.LaunchServerTime) / Duration, 0.0, 1.0);
	return FMath::Lerp(From, To, Alpha) + FVector(0.0, 0.0, FlightArcHeight * 4.0 * Alpha * (1.0 - Alpha));
}

bool ARpgHarvestSwarm::IsFinished() const
{
	if (Creatures.IsEmpty())
	{
		return false;
	}
	for (const FRpgHarvestSwarmCreature& Creature : Creatures)
	{
		if (!Creature.IsFinished())
		{
			return false;
		}
	}
	return true;
}

double ARpgHarvestSwarm::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ARpgHarvestSwarm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Creatures);
	DOREPLIFETIME(ThisClass, SummonServerTime);
}

void ARpgHarvestSwarm::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown(EndPlayReason == EEndPlayReason::Destroyed);
	Super::EndPlay(EndPlayReason);
}

void ARpgHarvestSwarm::Destroyed()
{
	const UWorld* World = GetWorld();
	Shutdown(World && !World->bIsTearingDown);
	Super::Destroyed();
}

void ARpgHarvestSwarm::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = GetServerWorldTimeSeconds();
	bool bAnyMoving = false;
	for (int32 CreatureIndex = 0; CreatureIndex < CreatureActors.Num(); ++CreatureIndex)
	{
		AActor* CreatureActor = CreatureActors[CreatureIndex];
		if (!IsValid(CreatureActor) || !Creatures.IsValidIndex(CreatureIndex) || Creatures[CreatureIndex].IsFinished())
		{
			continue;
		}

		const FVector Location = GetCreatureLocationAt(CreatureIndex, Now);
		const FVector Heading = GetCreatureLocationAt(CreatureIndex, Now + 0.05) - Location;
		CreatureActor->SetActorLocationAndRotation(
			Location,
			Heading.IsNearlyZero(0.1) ? CreatureActor->GetActorRotation() : Heading.Rotation());
		bAnyMoving = true;
	}
	if (!bAnyMoving)
	{
		SetActorTickEnabled(false);
	}
}

void ARpgHarvestSwarm::OnRep_Creatures()
{
	UpdatePresentation();
}

void ARpgHarvestSwarm::Step()
{
	if (!HasAuthority() || !bStarted || IsFinished())
	{
		return;
	}

	const double Now = GetServerWorldTimeSeconds();
	if (!IsSummonerAlive() || Now + RpgHarvestSwarm::DueTolerance >= ExpireServerTime)
	{
		DissipateAll();
		return;
	}

	bool bChanged = false;
	for (int32 CreatureIndex = 0; CreatureIndex < Creatures.Num(); ++CreatureIndex)
	{
		const FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
		if (Creature.State == ERpgHarvestSwarmCreatureState::Searching &&
			Now + RpgHarvestSwarm::DueTolerance >= Creature.LaunchServerTime)
		{
			if (!Reassign(CreatureIndex, Creature.From, Now))
			{
				Dissipate(CreatureIndex, Now);
			}
			bChanged = true;
		}
		else if (Creature.State == ERpgHarvestSwarmCreatureState::Flying &&
			Now + RpgHarvestSwarm::DueTolerance >= Creature.ArrivalServerTime)
		{
			Strike(CreatureIndex, Now);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		ReplicateCreatures();
	}
	FinishIfDone();
	ScheduleStep();
}

void ARpgHarvestSwarm::ScheduleStep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(StepTimerHandle);
	if (!HasAuthority() || !bStarted || IsFinished())
	{
		return;
	}

	double NextServerTime = ExpireServerTime;
	for (const FRpgHarvestSwarmCreature& Creature : Creatures)
	{
		if (Creature.State == ERpgHarvestSwarmCreatureState::Searching)
		{
			NextServerTime = FMath::Min(NextServerTime, Creature.LaunchServerTime);
		}
		else if (Creature.State == ERpgHarvestSwarmCreatureState::Flying)
		{
			NextServerTime = FMath::Min(NextServerTime, Creature.ArrivalServerTime);
		}
	}
	const float Delay = static_cast<float>(FMath::Max(0.001, NextServerTime - GetServerWorldTimeSeconds()));
	TimerManager.SetTimer(StepTimerHandle, this, &ThisClass::Step, Delay, false);
}

void ARpgHarvestSwarm::Strike(const int32 CreatureIndex, const double Now)
{
	FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
	FAssignment& Assignment = Assignments[CreatureIndex];
	UObject* Receiver = Assignment.Receiver.Get();
	FRpgHarvestResult Result = FRpgHarvestResult::MakeRejected(ERpgHarvestOutcome::Invalid);
	if (Receiver && Assignment.ReservedSections > 0 && RewardBatch)
	{
		FRpgHarvestRequest Request = RequestTemplate;
		Request.Hit = Assignment.Hit;
		Request.TraceOrigin = Creature.From;
		Request.ExpectedRevision = Assignment.ExpectedRevision;
		Request.RequestedSections = Assignment.ReservedSections;

		// The reservation is no lock: the stock left when the creature arrives decides, and the reward joins the
		// swarm's single delivery.
		RewardBatch->Open();
		Result = IRpgHarvestableTarget::Execute_CommitHarvest(Receiver, Request);
		RewardBatch->Close();
	}
	Assignment.ReservedSections = 0;

	if (Result.IsSuccess())
	{
		Creature.State = ERpgHarvestSwarmCreatureState::Struck;
		Creature.SectionsTaken = static_cast<uint8>(FMath::Clamp(Result.SectionsTaken, 0, 255));
		HarvestedSections += Result.SectionsTaken;
		Assignment.Receiver.Reset();
		return;
	}

	// The resource was emptied, removed or protected before the creature arrived: look for another one from here.
	if (Assignment.Reassignments < SwarmParams.MaxReassignments && Reassign(CreatureIndex, Creature.To, Now))
	{
		++Assignments[CreatureIndex].Reassignments;
		return;
	}
	Dissipate(CreatureIndex, Now);
}

bool ARpgHarvestSwarm::Reassign(const int32 CreatureIndex, const FVector& FromLocation, const double Now)
{
	UWorld* World = GetWorld();
	if (!World || SearchRadius <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgHarvestSwarm), false, this);
	if (AActor* SummonerActor = Summoner.Get())
	{
		QueryParams.AddIgnoredActor(SummonerActor);
	}
	const FVector Center = GetActorLocation();
	const FVector Lift(0.0, 0.0, StrikeHeight);
	TArray<FRpgHarvestTargetEvaluation> Candidates;
	FRpgHarvestTargeting::CollectAreaTargets(
		*World,
		Center,
		SearchRadius,
		SearchChannel,
		QueryParams,
		Center,
		true,
		RequestTemplate,
		0,
		Candidates);

	// The nearest resource that still has stock no other creature reserved.
	const FRpgHarvestTargetEvaluation* Best = nullptr;
	int32 BestAvailable = 0;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (const FRpgHarvestTargetEvaluation& Candidate : Candidates)
	{
		UObject* Receiver = Candidate.Receiver.Get();
		if (!Receiver || !Candidate.WouldHarvest())
		{
			continue;
		}
		const TPair<const UObject*, int32> Key = RpgHarvestSwarm::MakeTargetKey(Receiver, Candidate.Hit);
		int32 Available = FRpgHarvestSwarmPlanner::GetAvailableSections(Candidate);
		for (int32 Other = 0; Other < Assignments.Num(); ++Other)
		{
			const FAssignment& OtherAssignment = Assignments[Other];
			if (Other != CreatureIndex &&
				Creatures[Other].State == ERpgHarvestSwarmCreatureState::Flying &&
				RpgHarvestSwarm::MakeTargetKey(OtherAssignment.Receiver.Get(), OtherAssignment.Hit) == Key)
			{
				Available -= OtherAssignment.ReservedSections;
			}
		}
		if (Available <= 0)
		{
			continue;
		}

		const FVector StrikePoint = Candidate.Hit.ImpactPoint + Lift;
		const double DistanceSquared = FVector::DistSquared(FromLocation, StrikePoint);
		if (DistanceSquared >= BestDistanceSquared ||
			(SwarmParams.bRequireLineOfSight &&
				!FRpgHarvestSwarmPlanner::HasLineOfSight(*World, Center + Lift, StrikePoint, SearchChannel, QueryParams)))
		{
			continue;
		}
		Best = &Candidate;
		BestAvailable = Available;
		BestDistanceSquared = DistanceSquared;
	}
	if (!Best)
	{
		return false;
	}

	UObject* Receiver = Best->Receiver.Get();
	FAssignment& Assignment = Assignments[CreatureIndex];
	Assignment.Receiver = Receiver;
	Assignment.Hit = Best->Hit;
	Assignment.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Receiver, Best->Hit);
	Assignment.ReservedSections = FMath::Min(RequestTemplate.RequestedSections, BestAvailable);

	FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
	Creature.From = FromLocation;
	Creature.To = Best->Hit.ImpactPoint + Lift;
	Creature.LaunchServerTime = Now;
	Creature.ArrivalServerTime = Now +
		FMath::Max(RpgHarvestSwarm::MinimumFlightSeconds, FVector::Dist(Creature.From, Creature.To) / SwarmParams.FlightSpeed);
	Creature.State = ERpgHarvestSwarmCreatureState::Flying;
	Creature.Leg = static_cast<uint8>(FMath::Min(255, Creature.Leg + 1));
	return true;
}

void ARpgHarvestSwarm::Dissipate(const int32 CreatureIndex, const double Now)
{
	// The creature stops where it is and fades there.
	FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
	const FVector Location = GetCreatureLocationAt(CreatureIndex, Now);
	Creature.From = Location;
	Creature.To = Location;
	Creature.LaunchServerTime = FMath::Min(Creature.LaunchServerTime, Now);
	Creature.ArrivalServerTime = Creature.LaunchServerTime;
	Creature.State = ERpgHarvestSwarmCreatureState::Dissipated;

	FAssignment& Assignment = Assignments[CreatureIndex];
	Assignment.Receiver.Reset();
	Assignment.ReservedSections = 0;
}

void ARpgHarvestSwarm::DissipateAll()
{
	const double Now = GetServerWorldTimeSeconds();
	for (int32 CreatureIndex = 0; CreatureIndex < Creatures.Num(); ++CreatureIndex)
	{
		if (!Creatures[CreatureIndex].IsFinished())
		{
			Dissipate(CreatureIndex, Now);
		}
	}
	ReplicateCreatures();
	FinishIfDone();
}

void ARpgHarvestSwarm::FinishIfDone()
{
	if (!HasAuthority() || bDelivered || !IsFinished())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimerHandle);
	}
	ReleaseSummoner();
	DeliverRewards();
	SetLifeSpan(RpgHarvestSwarm::FinishedLingerSeconds);
}

bool ARpgHarvestSwarm::IsSummonerAlive() const
{
	const AActor* SummonerActor = Summoner.Get();
	if (!IsValid(SummonerActor) || SummonerActor->IsActorBeingDestroyed())
	{
		return false;
	}
	const UAbilitySystemComponent* AbilitySystem = SummonerAbilitySystem.Get();
	return !AbilitySystem || !AbilitySystem->HasMatchingGameplayTag(RpgGameplayTags::Status_Death);
}

void ARpgHarvestSwarm::HandleSummonerDeathTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	(void)Tag;
	// A dead summoner's creatures dissipate at once; nothing they had not struck yet yields loot.
	if (NewCount > 0 && HasAuthority() && bStarted && !IsFinished())
	{
		DissipateAll();
	}
}

void ARpgHarvestSwarm::ReplicateCreatures()
{
	ForceNetUpdate();
	UpdatePresentation();
}

void ARpgHarvestSwarm::ReleaseSummoner()
{
	if (UAbilitySystemComponent* AbilitySystem = SummonerAbilitySystem.Get(); AbilitySystem && DeathTagHandle.IsValid())
	{
		AbilitySystem->RegisterGameplayTagEvent(RpgGameplayTags::Status_Death, EGameplayTagEventType::NewOrRemoved)
			.Remove(DeathTagHandle);
	}
	DeathTagHandle.Reset();
	SummonerAbilitySystem.Reset();
}

void ARpgHarvestSwarm::DeliverRewards()
{
	if (bDelivered || !RewardBatch)
	{
		return;
	}
	bDelivered = true;

	// Overflow lands at the summoner's feet like any area harvest, or where the swarm was if the pawn is gone.
	FTransform DropTransform = GetActorTransform();
	if (const AActor* SummonerActor = Summoner.Get(); IsValid(SummonerActor) && !SummonerActor->IsActorBeingDestroyed())
	{
		DropTransform = SummonerActor->GetActorTransform();
	}
	const ERpgHarvestDelivery BatchDelivery = FRpgHarvestStockRules::ToDelivery(RewardBatch->Deliver(DropTransform));
	Delivery = HarvestedSections > 0 ? BatchDelivery : ERpgHarvestDelivery::None;
}

void ARpgHarvestSwarm::Shutdown(const bool bDeliverRewards)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimerHandle);
	}
	ReleaseSummoner();

	// What the creatures already harvested is never lost when the swarm is removed early; only a world that ends
	// takes it along.
	if (bDeliverRewards && bStarted && HasAuthority())
	{
		DeliverRewards();
	}
	if (RewardBatch)
	{
		if (RewardBatch->HasPendingRewards())
		{
			UE_LOG(LogRpgHarvesting, Warning, TEXT("%s ended with the world before it delivered its rewards."), *GetName());
		}
		RewardBatch->Discard();
		RewardBatch.Reset();
	}

	for (AActor* CreatureActor : CreatureActors)
	{
		if (IsValid(CreatureActor))
		{
			CreatureActor->Destroy();
		}
	}
	CreatureActors.Reset();
}

bool ARpgHarvestSwarm::ShouldPresent() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void ARpgHarvestSwarm::UpdatePresentation()
{
	UWorld* World = GetWorld();
	if (!World || !ShouldPresent())
	{
		return;
	}

	const double Now = GetServerWorldTimeSeconds();
	const int32 PreviouslyPresented = PresentedStates.Num();
	CreatureActors.SetNum(Creatures.Num());
	PresentedLegs.SetNum(Creatures.Num());
	PresentedStates.SetNum(Creatures.Num());

	bool bAnyMoving = false;
	for (int32 CreatureIndex = 0; CreatureIndex < Creatures.Num(); ++CreatureIndex)
	{
		const FRpgHarvestSwarmCreature& Creature = Creatures[CreatureIndex];
		const bool bNew = CreatureIndex >= PreviouslyPresented;
		if (bNew)
		{
			PresentedStates[CreatureIndex] = ERpgHarvestSwarmCreatureState::Searching;
			PresentedLegs[CreatureIndex] = 0;
			// A machine that receives the swarm late does not replay creatures that already finished.
			if (Creature.IsFinished())
			{
				PresentedStates[CreatureIndex] = Creature.State;
				PresentedLegs[CreatureIndex] = Creature.Leg;
				continue;
			}
			if (CreatureClass)
			{
				const FTransform SpawnTransform(GetCreatureLocationAt(CreatureIndex, Now));
				AActor* CreatureActor = World->SpawnActorDeferred<AActor>(
					CreatureClass,
					SpawnTransform,
					this,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (CreatureActor)
				{
					// Each machine moves its own creatures from the replicated flights.
					CreatureActor->SetReplicates(false);
					CreatureActor->FinishSpawning(SpawnTransform);
				}
				CreatureActors[CreatureIndex] = CreatureActor;
			}
		}

		const ERpgHarvestSwarmCreatureState PresentedState = PresentedStates[CreatureIndex];
		if (PresentedState == ERpgHarvestSwarmCreatureState::Struck ||
			PresentedState == ERpgHarvestSwarmCreatureState::Dissipated)
		{
			continue;
		}

		AActor* CreatureActor = CreatureActors[CreatureIndex];
		if (Creature.State == ERpgHarvestSwarmCreatureState::Flying &&
			(PresentedState != ERpgHarvestSwarmCreatureState::Flying || PresentedLegs[CreatureIndex] != Creature.Leg))
		{
			K2_OnCreatureLaunched(CreatureIndex, CreatureActor, Creature.From, Creature.To);
		}
		if (Creature.IsFinished())
		{
			if (IsValid(CreatureActor))
			{
				CreatureActor->SetActorLocation(Creature.To);
				CreatureActor->SetLifeSpan(FMath::Max(0.01f, CreatureLingerSeconds));
			}
			K2_OnCreatureFinished(
				CreatureIndex,
				CreatureActor,
				Creature.To,
				Creature.State == ERpgHarvestSwarmCreatureState::Struck);
		}
		else if (IsValid(CreatureActor))
		{
			bAnyMoving = true;
		}
		PresentedStates[CreatureIndex] = Creature.State;
		PresentedLegs[CreatureIndex] = Creature.Leg;
	}

	if (bAnyMoving)
	{
		SetActorTickEnabled(true);
	}
}
