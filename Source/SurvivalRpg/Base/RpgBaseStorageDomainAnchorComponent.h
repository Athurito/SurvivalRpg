#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgBaseStorageDomainAnchorComponent.generated.h"

/** Coarse derived presentation state for one physical storage-domain anchor. */
UENUM(BlueprintType)
enum class ERpgBaseStorageDomainAnchorVisualStatus : uint8
{
	/** The anchor is not currently backed by an available local storage network. */
	Offline,

	/** The anchor is available and operating below warning thresholds. */
	Ready,

	/** The anchor is actively representing stored contents without unsafe strain. */
	Active,

	/** Capacity or containment strain is approaching an authored warning threshold. */
	Strained,

	/** The anchor is at a critical limit and should present an urgent warning. */
	Critical,

	/** The anchor represents isolated or quarantined contained items. */
	Quarantined
};

/**
 * Replicated, read-only presentation snapshot for a storage-domain anchor.
 *
 * Values are derived by server-owned storage gameplay and are never storage authority themselves. Blueprint visuals
 * may read this struct but must not infer inventory mutations from it.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageDomainAnchorVisualState
{
	GENERATED_BODY()

	/** Coarse visual status selected by the authoritative storage network. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Domain Anchor")
	ERpgBaseStorageDomainAnchorVisualStatus Status =
		ERpgBaseStorageDomainAnchorVisualStatus::Offline;

	/** Normalized occupied-capacity presentation value in the inclusive range 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Domain Anchor")
	float FillRatio = 0.0f;

	/** Normalized containment or magical strain presentation value in the inclusive range 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Domain Anchor")
	float StrainRatio = 0.0f;

	/** Returns whether the replicated presentation ratios are finite and normalized. */
	bool IsValid() const;

	bool operator==(const FRpgBaseStorageDomainAnchorVisualState& Other) const;
	bool operator!=(const FRpgBaseStorageDomainAnchorVisualState& Other) const
	{
		return !(*this == Other);
	}
};

class URpgBaseStorageDomainAnchorComponent;

/** Broadcast after the authority changes visual state or a client receives a replicated state update. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgBaseStorageDomainAnchorVisualStateChanged,
	URpgBaseStorageDomainAnchorComponent*,
	Anchor,
	const FRpgBaseStorageDomainAnchorVisualState&,
	VisualState);

/**
 * Stable physical presentation anchor for one logical domain in a local base storage network.
 *
 * The component owns no items, capacity, or containment truth. A server-authoritative network coordinator may publish
 * derived visual state here so Blueprint actors can update shelves, seals, lights, and warnings for late joiners.
 */
UCLASS(
	BlueprintType,
	ClassGroup = (Base),
	meta = (BlueprintSpawnableComponent, DisplayName = "RPG Base Storage Domain Anchor"))
class SURVIVALRPG_API URpgBaseStorageDomainAnchorComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgBaseStorageDomainAnchorComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns the stable registry id used to target this physical anchor within its local base network. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Domain Anchor")
	FName GetAnchorId() const { return AnchorId; }

	/** Returns the logical storage domain represented by this physical anchor. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Domain Anchor")
	FGameplayTag GetDomainTag() const { return DomainTag; }

	/**
	 * Configures the stable identity of a native default anchor.
	 * Runtime calls are authority-only; invalid ids or non-domain tags are rejected without mutation.
	 */
	bool ConfigureAnchor(FName NewAnchorId, FGameplayTag NewDomainTag);

	/** Returns the replicated cosmetic snapshot currently driving this anchor's visuals. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Domain Anchor")
	FRpgBaseStorageDomainAnchorVisualState GetVisualState() const
	{
		return VisualState;
	}

	/**
	 * Publishes a validated cosmetic snapshot on the server.
	 * Returns false for client calls, invalid normalized values, or an unchanged snapshot.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Domain Anchor")
	bool SetVisualState(const FRpgBaseStorageDomainAnchorVisualState& NewVisualState);

	/** Local presentation signal fired on server changes, replication updates, and initial BeginPlay state. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Domain Anchor")
	FRpgBaseStorageDomainAnchorVisualStateChanged OnVisualStateChanged;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * Stable authored id, replicated for runtime actors. It must be unique within one local storage network;
	 * editor validation catches same-actor duplicates and the future network registry must reject cross-actor repeats.
	 */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Base Storage|Domain Anchor")
	FName AnchorId = TEXT("Default");

	/** Logical domain represented by this anchor. Must be a strict Storage.Domain child and unique among anchors on the same owner. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Base Storage|Domain Anchor", meta = (Categories = "Storage.Domain"))
	FGameplayTag DomainTag;

	/** Server-derived cosmetic state replicated for Blueprint presentation and join-in-progress reconstruction. */
	UPROPERTY(ReplicatedUsing = OnRep_VisualState, BlueprintReadOnly, Category = "Base Storage|Domain Anchor")
	FRpgBaseStorageDomainAnchorVisualState VisualState;

	UFUNCTION()
	void OnRep_VisualState();

private:
	void BroadcastVisualState();
};
