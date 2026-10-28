#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Harvesting/RpgHarvestableTarget.h"
#include "TimerManager.h"

#include "RpgHarvestBurn.generated.h"

/**
 * One burn on one harvestable resource, lit by a socketed rune such as Brand when its harvester's own hit works the
 * resource.
 *
 * Server-authoritative: every TickIntervalSeconds the burn commits secondary work of the igniting action on the
 * resource, with the igniting player as harvester, until its time is up, the resource is empty, respawned or protected,
 * or the player leaves. The work only depletes the stock faster; every section it completes rewards the igniting player
 * and awards XP once, through the resource's normal commit. Each player keeps at most one burn per resource, so co-op
 * partners can burn the same resource side by side. The actor replicates without movement, so every player sees its
 * presentation, such as a fire Niagara component of the Blueprint subclass, for as long as it burns.
 */
UCLASS(Blueprintable)
class GF_HARVESTING_MAGIC_API ARpgHarvestBurn : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestBurn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Authority: lights a burn of BurnClass on the resource that DirectRequest struck on Receiver, for the player of
	 * DirectRequest's harvester. WorkPerSecond is in sections of work per second and DurationSeconds the burn time, both
	 * above zero. When that player already burns the resource, returns that burn unchanged. Returns null when nothing
	 * burns.
	 */
	static ARpgHarvestBurn* Light(
		UObject* Receiver,
		const FRpgHarvestRequest& DirectRequest,
		TSubclassOf<ARpgHarvestBurn> BurnClass,
		float WorkPerSecond,
		float DurationSeconds);

	/** Authority: the burn that Beneficiary keeps on resource Item of Receiver, or null. */
	static ARpgHarvestBurn* FindBurn(const UObject* Receiver, int32 Item, const AActor* Beneficiary);

	/** Authority: the player state (or harvester) that lit this burn and receives its rewards. */
	AActor* GetBeneficiary() const { return RequestTemplate.Harvester; }

	/** Authority: sections of work the burn does per second. */
	float GetWorkPerSecond() const { return WorkPerSecond; }

	/** Authority: seconds of burn time left. */
	float GetRemainingSeconds() const;

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
	bool Start(UObject* InReceiver, AActor* Beneficiary, const FRpgHarvestRequest& DirectRequest, float InWorkPerSecond, float DurationSeconds);
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
	double EndTime = 0.0;
	FTimerHandle BurnTimerHandle;
};
