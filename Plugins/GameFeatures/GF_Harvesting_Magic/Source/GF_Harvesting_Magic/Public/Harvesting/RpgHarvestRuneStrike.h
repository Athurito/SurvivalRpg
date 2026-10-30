#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Harvesting/RpgHarvestableTarget.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "Templates/Function.h"
#include "TimerManager.h"

#include "RpgHarvestRuneStrike.generated.h"

/**
 * Counts the accepted own hits that charge a rune's echo, such as the harvesting Echo rune: every HitsPerEcho own hits,
 * each within TimeoutSeconds of the one before, complete one charge. Server-side state of one harvest ability instance,
 * so it ends when the tool's abilities are removed.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestEchoCharge
{
	/**
	 * Counts one accepted own hit at world time Now. A hit after the deadline of the previous one starts the count
	 * over. Returns true when the hit completes the charge, which then starts over; HitsPerEcho below 1 never does.
	 */
	bool AddHit(double Now, int32 HitsPerEcho, float TimeoutSeconds);

	/** Forgets every counted hit. */
	void Reset();

	/** Hits counted towards the next echo. */
	int32 GetHits() const { return Hits; }

private:
	int32 Hits = 0;
	double Deadline = -1.0;
};

/** One harvestable resource as follow-up rules tell resources apart: instance Item of Receiver, or Receiver itself. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestResourceId
{
	const UObject* Receiver = nullptr;
	int32 Item = INDEX_NONE;

	/** Returns whether Target addresses this resource. */
	bool Matches(const FRpgHarvestTargetEvaluation& Target) const
	{
		return Target.Receiver.Get() == Receiver && Target.Hit.Item == Item;
	}
};

/** One planned leap strike of an Erntesprung: the resource it strikes and its step in the chain, from 0. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestLeapStrike
{
	FRpgHarvestTargetEvaluation Target;
	int32 Step = 0;
};

/**
 * Stateless rules of the harvesting runes that send follow-up work to resources: Echo and Kraftuebertrag (Runenfluss),
 * Erntesprung and Durchschlag (Ausbreitung). Shared by the harvest ability and its tests.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestRuneFlow
{
	/**
	 * Appends to OutTargets up to MaxTargets resources of Candidates that a follow-up strike would harvest, in the
	 * candidates' order (nearest first from FRpgHarvestTargeting::CollectAreaTargets), leaving out resource ExcludeItem
	 * of ExcludeReceiver: the resource the triggering hit struck.
	 */
	static void PickFollowUpTargets(
		const TArray<FRpgHarvestTargetEvaluation>& Candidates,
		const UObject* ExcludeReceiver,
		int32 ExcludeItem,
		int32 MaxTargets,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets);

	/** Like PickFollowUpTargets, leaving out every resource of Excluded. */
	static void PickFollowUpTargets(
		const TArray<FRpgHarvestTargetEvaluation>& Candidates,
		TConstArrayView<FRpgHarvestResourceId> Excluded,
		int32 MaxTargets,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets);

	/**
	 * Appends to OutTargets up to MaxTargets resources of Candidates behind Origin that a follow-up strike would
	 * harvest, nearest first: those whose location lies, seen from above, within ConeHalfAngleDegrees of Direction
	 * from Origin. Leaves out resource Excluded, the struck one. A Direction without horizontal length picks nothing.
	 */
	static void PickPierceTargets(
		const TArray<FRpgHarvestTargetEvaluation>& Candidates,
		const FRpgHarvestResourceId& Excluded,
		const FVector& Origin,
		const FVector& Direction,
		float ConeHalfAngleDegrees,
		int32 MaxTargets,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets);

	/**
	 * Plans the chain of an Erntesprung from resource Struck at StruckPoint: up to Jumps steps, each striking up to
	 * TargetsPerJump of the nearest resources that CollectCandidates finds around the previous step's first resource
	 * and that no step struck yet. The chain never strikes a resource twice, and it ends early when a step finds
	 * nothing. CollectCandidates appends the candidates around Center, nearest first, up to MaxTargets the strike would
	 * harvest.
	 */
	static void PlanLeaps(
		const FRpgHarvestResourceId& Struck,
		const FVector& StruckPoint,
		int32 Jumps,
		int32 TargetsPerJump,
		TFunctionRef<void(const FVector& Center, int32 MaxTargets, TArray<FRpgHarvestTargetEvaluation>& OutCandidates)> CollectCandidates,
		TArray<FRpgHarvestLeapStrike>& OutStrikes);

	/**
	 * Sections of work a Kraftuebertrag carries from a depleting hit: the actual overkill of Result times Fraction.
	 * Zero without overkill, so completing a resource exactly, or a finishing request, carries nothing.
	 */
	static float TransferSections(const FRpgHarvestResult& Result, float Fraction);
};

/**
 * One delayed follow-up hit of a harvesting rune on one resource, such as an Echo repeating a hit on its resource or a
 * Kraftuebertrag carrying a depleting hit's overkill to the next resource.
 *
 * Server-authoritative: DelaySeconds after Launch the strike commits secondary work of the triggering action on its
 * resource once, with the triggering player as harvester, through the resource's normal commit. The work only depletes
 * the stock faster; every section it completes rewards that player and awards XP once. It never counts as an own hit,
 * so it charges no echo, lights no burn and carries no overkill on. A resource that was depleted or respawned meanwhile
 * takes nothing. The actor replicates without movement for its presentation, such as a Niagara flare of the Blueprint
 * subclass, and removes itself LingerSeconds after the strike.
 */
UCLASS(Blueprintable)
class GF_HARVESTING_MAGIC_API ARpgHarvestRuneStrike : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestRuneStrike(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Authority: spawns a strike of StrikeClass at TargetHit that works resource TargetHit.Item of Receiver with
	 * WorkSections sections of work for DirectRequest's player, as follow-up work of DirectRequest's action. A strike on
	 * another resource than the one DirectRequest struck (bOtherResource) is an area harvest, so protected resources
	 * refuse it. ExtraDelaySeconds, 0 to 8, waits on top of the class's DelaySeconds, for example for the later steps of
	 * a leap. Returns the strike, or null when there is nothing to strike.
	 */
	static ARpgHarvestRuneStrike* Launch(
		UObject* Receiver,
		const FHitResult& TargetHit,
		const FRpgHarvestRequest& DirectRequest,
		TSubclassOf<ARpgHarvestRuneStrike> StrikeClass,
		float WorkSections,
		bool bOtherResource,
		float ExtraDelaySeconds = 0.0f);

	/** Authority: the player state (or harvester) that receives the strike's rewards. */
	AActor* GetBeneficiary() const { return Request.Harvester; }

	/** Authority: the resource the strike works. */
	UObject* GetReceiver() const { return Receiver.Get(); }

	/** Authority: sections of work the strike brings. */
	float GetWorkSections() const { return Request.WorkScale; }

	/** The effect this strike's work names in its provenance, for example Rune.Echo. */
	FGameplayTag GetProcTag() const { return ProcTag; }

	/** Authority: whether the strike has committed its work. */
	bool HasStruck() const { return bStruck; }

	/** Authority: the result of the strike's commit, a rejected Invalid result before it. */
	const FRpgHarvestResult& GetLastResult() const { return LastResult; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Seconds from the triggering hit until the strike works its resource, 0 to 2, for example 0.3 for Echo.
	 * Designer-tuned on the Blueprint subclass.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Rune Strike", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "1.0", Units = "s"))
	float DelaySeconds = 0.3f;

	/**
	 * Seconds the actor, and with it its presentation, stays after the strike. Cosmetic; designer-tuned on the
	 * Blueprint subclass.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Rune Strike", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0", Units = "s"))
	float LingerSeconds = 1.0f;

	/**
	 * Effect the strike's work names in its provenance, for example Rune.Echo or Rune.Kraftuebertrag, so later effects
	 * can tell it from other follow-up work. Designer-set on the Blueprint subclass.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Rune Strike", meta = (Categories = "Rune"))
	FGameplayTag ProcTag;

private:
	bool Start(
		UObject* InReceiver,
		AActor* Beneficiary,
		const FHitResult& TargetHit,
		const FRpgHarvestRequest& DirectRequest,
		float WorkSections,
		bool bOtherResource,
		float ExtraDelaySeconds);
	void Strike();

	/** The request the strike commits: secondary work of the triggering action, for the beneficiary. Authority. */
	UPROPERTY(Transient)
	FRpgHarvestRequest Request;

	UPROPERTY(Transient)
	FRpgHarvestResult LastResult;

	TWeakObjectPtr<UObject> Receiver;
	FTimerHandle StrikeTimerHandle;
	bool bStruck = false;
};
