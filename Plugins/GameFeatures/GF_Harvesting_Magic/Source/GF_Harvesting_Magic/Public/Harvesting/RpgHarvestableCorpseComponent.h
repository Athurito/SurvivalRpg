#pragma once

#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgHarvestableCorpseComponent.generated.h"

class URpgCorpseHarvestProfile;
class URpgCorpseLifecycleComponent;
class URpgHarvestableCorpseComponent;
class URpgInventoryManagerComponent;
class FLifetimeProperty;
struct FInteractionQuery;
struct FInteractionOption;

/** Replicated reservation/completion state; corpse availability itself remains owned by the core lifecycle. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgCorpseHarvestState
{
	GENERATED_BODY()

	/** Monotonic server revision used to reject stale interaction and montage commits. */
	UPROPERTY(BlueprintReadOnly, Category = "Corpse Harvesting")
	int32 Revision = 0;

	/** True while exactly one server-validated harvester owns the timed reservation. */
	UPROPERTY(BlueprintReadOnly, Category = "Corpse Harvesting")
	bool bReserved = false;

	/** True once reward delivery is accepted; XP and lifecycle completion finish in the same server commit. */
	UPROPERTY(BlueprintReadOnly, Category = "Corpse Harvesting")
	bool bCompleted = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgCorpseHarvestStateChangedEvent,
	const FRpgCorpseHarvestState&, State,
	bool, bIsInteractable);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgCorpseHarvestCompletedEvent,
	AActor*, Harvester,
	FRpgInventoryItemId, ToolItemId);

/** Server-local cancellation signal used by the active ability to release movement/cue state immediately. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FRpgCorpseHarvestReservationEndedNative,
	URpgHarvestableCorpseComponent*,
	AActor*,
	int32);

/**
 * Server-authoritative Interaction target for processing a settled corpse.
 *
 * The core corpse lifecycle owns availability and despawn. This component owns only harvest revision,
 * one timed reservation, tool/skill validation, atomic rewards, and the external completion signal.
 */
UCLASS(BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvestable Corpse"))
class GF_HARVESTING_MAGIC_API URpgHarvestableCorpseComponent final
	: public UActorComponent
	, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgHarvestableCorpseComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IInteractableTarget interface
	virtual void GatherInteractionOptions(
		const FInteractionQuery& InteractQuery,
		FInteractionOptionBuilder& OptionBuilder) override;
	//~ End IInteractableTarget interface

	/** Current server-authored reservation/completion revision. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	int32 GetHarvestRevision() const { return HarvestState.Revision; }

	/** Replicated read-only reservation/completion snapshot for UI and diagnostics. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	const FRpgCorpseHarvestState& GetHarvestState() const { return HarvestState; }

	/** True when the core corpse is available and this target is neither reserved nor completed. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	bool IsHarvestAvailable() const;

	/** Static designer-authored tool, animation, interaction, and reward profile. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	const URpgCorpseHarvestProfile* GetHarvestProfile() const { return HarvestProfile; }

	/** Core bone-following interaction anchor used for prompt, distance, and overflow-drop placement. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	URpgCorpseLifecycleComponent* GetCorpseLifecycle() const { return CorpseLifecycle; }

	/** Read-only preflight for an exact interaction revision and exact still-owned tool. */
	bool CanBeginHarvest(
		AActor* Harvester,
		int32 ExpectedRevision,
		FRpgInventoryItemId ToolItemId) const;

	/** Atomically reserves this revision for one authority-side ability and returns the new reservation revision. */
	bool TryReserveHarvest(
		AActor* Harvester,
		int32 ExpectedRevision,
		FRpgInventoryItemId ToolItemId,
		int32& OutReservationRevision);

	/** Revalidates corpse, range, skill, and tool at the montage notify, then delivers reward exactly once. */
	bool TryCommitReservedHarvest(
		AActor* Harvester,
		int32 ReservationRevision,
		FRpgInventoryItemId ToolItemId);

	/** Releases only the matching current reservation; stale ability cleanup cannot unlock a newer harvester. */
	void CancelHarvestReservation(AActor* Harvester, int32 ReservationRevision);

	/** Native abort signal fired for timeout, lifecycle expiry, explicit cancel, or component teardown. */
	FRpgCorpseHarvestReservationEndedNative& OnHarvestReservationEndedNative()
	{
		return HarvestReservationEndedNative;
	}

	/** Presentation/read-only state signal emitted on authority changes and client replication. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Corpse Harvesting")
	FRpgCorpseHarvestStateChangedEvent OnHarvestStateChanged;

	/** Server telemetry emitted after reward, XP, and local completion state are final. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Corpse Harvesting")
	FRpgCorpseHarvestCompletedEvent OnCorpseHarvestCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Designer-authored interaction, tool, montage, progression, and reward rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Corpse Harvesting")
	TObjectPtr<URpgCorpseHarvestProfile> HarvestProfile;

private:
	UFUNCTION()
	void OnRep_HarvestState();

	void HandleCorpseAvailabilityChanged(URpgCorpseLifecycleComponent* Component, bool bIsAvailable);
	void HandleReservationTimeout();
	void SetHarvestState(bool bReserved, bool bCompleted, bool bBroadcast = true);
	bool IsMatchingReservation(AActor* Harvester, int32 ReservationRevision) const;
	bool IsHarvesterInRange(AActor* Harvester) const;
	URpgInventoryManagerComponent* ResolveHarvesterInventory(AActor* Harvester) const;
	void BroadcastStateChanged();

	/** Compact replicated state used by clients and stale-request validation. */
	UPROPERTY(ReplicatedUsing = OnRep_HarvestState)
	FRpgCorpseHarvestState HarvestState;

	/** Runtime core lifecycle adapter; availability is queried rather than duplicated. */
	UPROPERTY(Transient)
	TObjectPtr<URpgCorpseLifecycleComponent> CorpseLifecycle;

	/** Server-only reservation owner. Clients need only the replicated bReserved presentation bit. */
	TWeakObjectPtr<AActor> ReservedHarvester;

	/** Exact tool selected at reservation; prevents ability payload substitution at commit. */
	FRpgInventoryItemId ReservedToolItemId;

	/** Server-local guard preventing inventory post-commit callbacks from reentering or cancelling reward delivery. */
	bool bCommitInProgress = false;

	FTimerHandle ReservationTimeoutHandle;
	FRpgCorpseHarvestReservationEndedNative HarvestReservationEndedNative;
};
