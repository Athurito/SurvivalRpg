#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Harvesting/RpgHarvestableTarget.h"
#include "TimerManager.h"

#include "RpgHarvestBurn.generated.h"

/** What one direct hit asks of the burn on its resource, from the tunings of the hitting tool. Server-side. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestBurnParams
{
	/** Sections of work the burn does per second; above zero to light or refresh a burn. */
	float WorkPerSecond = 0.0f;

	/** Burn time in seconds of a new or refreshed burn; above zero to light or refresh a burn. */
	float DurationSeconds = 0.0f;

	/** Whether a further hit restarts the burn time of the player's running burn on the resource. */
	bool bRefresh = false;

	/**
	 * Above zero, a further hit releases the work the player's running burn would still do, times this value, at once,
	 * and puts the burn out instead of refreshing it.
	 */
	float DischargeMultiplier = 0.0f;

	/**
	 * Above zero, a new burn moves on when its resource runs empty while it still has burn time: to the nearest other
	 * resource within this radius in centimeters that the hit's tool can work and that does not burn for the player yet.
	 * It burns there for its remaining time. Zero puts it out with its resource.
	 */
	float WanderRadius = 0.0f;

	/** Collision channel on which a wandering burn finds the next resource, like the hit's trace channel. */
	TEnumAsByte<ECollisionChannel> WanderChannel = ECC_Visibility;
};

/**
 * One burn on one harvestable resource, lit by a socketed rune such as Brand when its harvester's own hit works the
 * resource.
 *
 * Server-authoritative: every TickIntervalSeconds the burn commits secondary work of the igniting action on the
 * resource, with the igniting player as harvester, until its ticks are used up, the resource is empty, respawned or
 * protected, or the player leaves. The work only depletes the stock faster; every section it completes rewards the
 * igniting player and awards XP once, through the resource's normal commit. Each player keeps at most one burn per
 * resource, so co-op partners can burn the same resource side by side. The actor replicates without movement, so every
 * player sees its presentation, such as a fire Niagara component of the Blueprint subclass, for as long as it burns.
 *
 * A burn lit with a wander radius, from a talent such as Wandering Effect, moves on when its resource runs empty: a new
 * burn of the same class takes over its remaining ticks on the nearest other resource, so the total burn time never
 * grows and the wandering always ends.
 */
UCLASS(Blueprintable)
class GF_HARVESTING_MAGIC_API ARpgHarvestBurn : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestBurn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Authority: applies a direct hit to the burn of DirectRequest's player on the resource it struck on Receiver.
	 * Without a running burn, lights one of BurnClass. With one, releases it when Params asks for a discharge, restarts
	 * its time when Params asks for a refresh, and otherwise keeps it unchanged. Returns the burn that keeps burning,
	 * or null when none does.
	 */
	static ARpgHarvestBurn* Light(
		UObject* Receiver,
		const FRpgHarvestRequest& DirectRequest,
		TSubclassOf<ARpgHarvestBurn> BurnClass,
		const FRpgHarvestBurnParams& Params);

	/** Authority: the burn that Beneficiary keeps on resource Item of Receiver, or null. */
	static ARpgHarvestBurn* FindBurn(const UObject* Receiver, int32 Item, const AActor* Beneficiary);

	/** Authority: the player state (or harvester) that lit this burn and receives its rewards. */
	AActor* GetBeneficiary() const { return RequestTemplate.Harvester; }

	/** Authority: sections of work the burn does per second. */
	float GetWorkPerSecond() const { return WorkPerSecond; }

	/** Authority: commits of burn work still to come. */
	int32 GetRemainingTicks() const { return RemainingTicks; }

	/** Authority: the resource the burn works. */
	UObject* GetReceiver() const { return Receiver.Get(); }

	/** Authority: the burn that took over this one's remaining time on another resource, or null. */
	ARpgHarvestBurn* GetWanderedTo() const { return WanderedTo.Get(); }

	/** The effect this burn's work names in its provenance, for example Rune.Brand. */
	FGameplayTag GetProcTag() const { return ProcTag; }

	/** Authority: the result of the burn's last commit, a rejected Invalid result before the first one. */
	const FRpgHarvestResult& GetLastResult() const { return LastResult; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds between two commits of burn work, at least 0.1. Designer-tuned on the Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Burn", meta = (ClampMin = "0.1", UIMin = "0.25", UIMax = "2.0", Units = "s"))
	float TickIntervalSeconds = 1.0f;

	/**
	 * Effect the burn's work names in its provenance, for example Rune.Brand, so later effects can tell it from other
	 * follow-up work. Designer-set on the Blueprint subclass.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Burn", meta = (Categories = "Rune"))
	FGameplayTag ProcTag;

private:
	bool Start(UObject* InReceiver, AActor* Beneficiary, const FRpgHarvestRequest& DirectRequest, const FRpgHarvestBurnParams& Params);

	/**
	 * Moves the remaining ticks on to the nearest other resource around this burn that its tool can work and that does
	 * not burn for its player yet. Returns whether a burn took over.
	 */
	bool Wander();
	int32 TicksFor(float DurationSeconds) const;
	void Refresh(float DurationSeconds);
	void Discharge(float Multiplier);
	void Burn();
	void Extinguish();

	/** The request every commit sends: secondary work of the igniting action, for the beneficiary. Authority. */
	UPROPERTY(Transient)
	FRpgHarvestRequest RequestTemplate;

	UPROPERTY(Transient)
	FRpgHarvestResult LastResult;

	TWeakObjectPtr<UObject> Receiver;
	int32 Item = INDEX_NONE;
	float WorkPerSecond = 0.0f;
	int32 RemainingTicks = 0;
	float WanderRadius = 0.0f;
	TEnumAsByte<ECollisionChannel> WanderChannel = ECC_Visibility;
	TWeakObjectPtr<ARpgHarvestBurn> WanderedTo;
	FTimerHandle BurnTimerHandle;
};
