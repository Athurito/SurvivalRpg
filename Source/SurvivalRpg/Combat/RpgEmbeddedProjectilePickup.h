#pragma once

#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#include "RpgEmbeddedProjectilePickup.generated.h"

class UPrimitiveComponent;
struct FRpgProjectileRecoveryDefinition;

/** Where the server places recovered ammunition. */
struct FRpgProjectileRecoveryPlacement
{
	/** World transform of the pickup: at the contact along the flight when embedded, on the ground when dropped. */
	FTransform Transform;
	/** The component the ammunition stuck in or lies on; it groups pickups and is followed when it moves. */
	TWeakObjectPtr<UPrimitiveComponent> HostComponent;
	/** Bone of a skeletal host the pickup attaches to; None for its root. */
	FName HostBone;
	/** Embedded in the host rather than dropped on the ground. */
	bool bEmbedded = false;
};

/**
 * Temporary world pickup of recovered projectile ammunition, such as intact arrows (ranged combat roadmap RNG-03b).
 *
 * Its replicated loot inventory is the only quantity owner: players collect it through the ordinary Collect
 * interaction and the atomic dropped-inventory transfer, a full inventory leaves the rest, and the pickup goes once it is
 * empty. Its container is withdraw-only, closed to crafting scans and never registered for world saves, so recovered
 * ammunition persists only after a player collected it.
 *
 * The server groups pickups per hit component and caps them per world (URpgEmbeddedProjectileSubsystem); grouping only
 * changes the actor count, never the quantity. Every pickup expires at its recovery deadline with what is left in it.
 * On a moving host it attaches to the hit component and bone, which every machine reconstructs from the replicated
 * attachment, also after a late join; when the host goes, it stays at its last world transform until its deadline.
 *
 * Blueprint subclasses only add presentation: the ammunition mesh on DisplayMesh, with its tip at the actor origin
 * pointing along +X.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgEmbeddedProjectilePickup : public ARpgDroppedInventoryActor
{
	GENERATED_BODY()

public:
	ARpgEmbeddedProjectilePickup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Server: places one recovered item of Ammunition. Once Placement's host holds the definition's maximum of separate
	 * pickups, it joins the nearest pickup of the same ammunition on that host within GroupRadius; otherwise it spawns
	 * a new pickup of PickupClass. Returns the pickup that holds the item, or null when it could not be placed.
	 */
	static ARpgEmbeddedProjectilePickup* RecoverAmmunition(
		UWorld* World,
		TSubclassOf<ARpgEmbeddedProjectilePickup> PickupClass,
		TSubclassOf<URpgInventoryItemDefinition> Ammunition,
		const FRpgProjectileRecoveryDefinition& Definition,
		const FRpgProjectileRecoveryPlacement& Placement);

	/**
	 * Whether a projectile can embed in Host or lie on it as its host: no other collectable, whose pickup would leave
	 * the arrow floating, and on a moving host only one that every client can resolve, so the attachment replicates.
	 * Otherwise an embed drops, and a dropped pickup stays where it fell without a host.
	 */
	static bool CanEmbedIn(const UPrimitiveComponent* Host);

	/** Server: adds Count items of its ammunition and keeps the later of its own and Deadline (world seconds). */
	bool AddRecoveredAmmunition(int32 Count, double Deadline);

	/** Server: ends the recovery early or at its deadline; the remaining ammunition is gone. */
	void ExpireRecovery();

	/** The ammunition this pickup holds; replicated once. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Recovery")
	TSubclassOf<URpgInventoryItemDefinition> GetAmmunitionDefinition() const { return AmmunitionDefinition; }

	/** Items of its ammunition left in the pickup, read from the replicated loot inventory. UI read-only. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Recovery")
	int32 GetAmmunitionCount() const;

	/** Whether the ammunition is embedded where it hit rather than lying on the ground; replicated once. Presentation. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Recovery")
	bool IsEmbedded() const { return bEmbedded; }

	/** Server: world time in seconds at which the pickup expires. */
	double GetRecoveryDeadline() const { return RecoveryDeadline; }

	/** Server: the component the ammunition stuck in or lies on, or null once it is gone. */
	UPrimitiveComponent* GetHostComponent() const { return HostComponent.Get(); }

	/** The display mesh; on a class's defaults, the mesh and offset that shafts on characters reuse. */
	UStaticMeshComponent* GetDisplayMeshComponent() const { return DisplayMesh; }

	/** Depth in cm an embedded projectile of this class sits inside what it hit. */
	float GetEmbedDepth() const { return EmbedDepth; }

	//~ AActor
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FInteractionOption BuildCollectInteractionOption(const FInteractionQuery& InteractQuery) const override;

	/** Depth in cm an embedded pickup sits inside the surface along its flight, so the tip disappears. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float EmbedDepth = 4.0f;

private:
	void Initialize(
		TSubclassOf<URpgInventoryItemDefinition> Ammunition,
		const FRpgProjectileRecoveryPlacement& Placement,
		double Deadline);
	void ScheduleExpiry();

	UPROPERTY(Replicated)
	TSubclassOf<URpgInventoryItemDefinition> AmmunitionDefinition;

	UPROPERTY(Replicated)
	bool bEmbedded = false;

	double RecoveryDeadline = 0.0;
	TWeakObjectPtr<UPrimitiveComponent> HostComponent;
	FTimerHandle ExpiryTimer;
};
