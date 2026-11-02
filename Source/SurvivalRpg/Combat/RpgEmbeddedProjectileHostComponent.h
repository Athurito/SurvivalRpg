#pragma once

#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Templates/SubclassOf.h"

#include "RpgEmbeddedProjectileHostComponent.generated.h"

class ARpgEmbeddedProjectilePickup;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class URpgLootSourceComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FInventoryPickup;
struct FRpgProjectileRecoveryDefinition;

/** One visible shaft on a character that stands for Count recovered projectiles. Replicated presentation only. */
USTRUCT()
struct SURVIVALRPG_API FRpgEmbeddedProjectileGroup
{
	GENERATED_BODY()

	/** Server-assigned identity of the shaft. */
	UPROPERTY()
	int32 GroupId = 0;

	/** Bone of the character's skeletal mesh the shaft is attached to; None for the mesh itself. */
	UPROPERTY()
	FName Bone;

	/** Shaft location relative to the bone in cm. */
	UPROPERTY()
	FVector_NetQuantize10 RelativeLocation = FVector::ZeroVector;

	/** Shaft rotation relative to the bone. */
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** Pickup class whose display mesh the shaft shows. */
	UPROPERTY()
	TSubclassOf<ARpgEmbeddedProjectilePickup> VisualClass;

	/** Recovered projectiles this shaft stands for; the shaft disappears at zero. */
	UPROPERTY()
	int32 Count = 0;
};

/**
 * Recovered projectiles lodged in an AI character and its corpse (ranged combat roadmap RNG-03c).
 *
 * The server keeps one record per intact projectile, registered before its damage applies, so even a killing arrow
 * counts. Records stay pending entitlements while the character lives: they become corpse loot when the colocated
 * URpgLootSourceComponent populates, or join that loot at once for a later hit on the corpse. From then on the corpse
 * inventory is the only quantity owner. Recovered items are credited in order, and every decrease of their quantity in
 * the corpse inventory retires the oldest credits and their shafts; moves inside the corpse change nothing. A completed
 * or expiring corpse takes no further credits, and nothing ever drops into the world when the corpse goes.
 *
 * Shafts are presentation: at most MaxVisibleArrowsPerCharacter per character, after which further projectiles join the
 * nearest shaft. The groups replicate; every machine attaches its own shaft meshes to the bones of its own skeletal
 * mesh, so they follow animation and each local ragdoll, also for late joiners.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEmbeddedProjectileHostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgEmbeddedProjectileHostComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Server: whether the character can take a recovered projectile now: alive, or a corpse whose loot is still open. */
	bool CanRecover() const;

	/**
	 * Server: records one intact projectile of Ammunition that hit the character at Hit while flying along
	 * FlightDirection, and shows it on the nearest bone. Returns the record's id, or zero when nothing was recorded.
	 */
	int32 RecoverProjectile(
		const FHitResult& Hit,
		const FVector& FlightDirection,
		TSubclassOf<URpgInventoryItemDefinition> Ammunition,
		const FRpgProjectileRecoveryDefinition& Definition);

	/** Server: withdraws a record that has not reached the corpse loot yet, such as one whose impact turned out blocked. */
	void CancelRecord(int32 RecordId);

	/** The visible shafts; replicated. */
	const TArray<FRpgEmbeddedProjectileGroup>& GetGroups() const { return Groups; }

	/** Server: recovered projectiles still waiting for the corpse loot. */
	int32 GetPendingCount() const;

	/** Server: recovered projectiles in the corpse loot whose items are still there. */
	int32 GetCreditedCount() const;

	/** Shaft meshes this machine shows. */
	int32 GetVisibleShaftCount() const { return Visuals.Num(); }

	/** The shaft mesh this machine shows for GroupId, or null. */
	UStaticMeshComponent* GetShaftForTests(int32 GroupId) const;

	//~ UActorComponent
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class ERecordState : uint8
	{
		/** Lodged in the living character or a corpse whose loot is not built yet. */
		Pending,
		/** In the corpse loot payload that has not committed yet. */
		Queued,
		/** In the corpse inventory. */
		Credited,
		/** Taken from the corpse; its shaft share is gone. */
		Retired,
	};

	struct FRecord
	{
		int32 RecordId = 0;
		TSubclassOf<URpgInventoryItemDefinition> Ammunition;
		int32 GroupId = 0;
		ERecordState State = ERecordState::Pending;
	};

	void BindCorpseLoot();
	void HandleGatherLoot(URpgLootSourceComponent* Source, FInventoryPickup& Payload);
	void HandleLootPopulated(URpgLootSourceComponent* Source, bool bHasLoot);
	void HandleInventoryCommitted(URpgInventoryManagerComponent* Inventory);
	void OfferPendingToLoot();
	void ChangeGroupCount(int32 GroupId, int32 Delta);
	void MarkGroupsChanged();
	USkeletalMeshComponent* FindMesh() const;
	URpgLootSourceComponent* FindLootSource() const;
	URpgInventoryManagerComponent* FindCorpseInventory() const;

	UFUNCTION()
	void OnRep_Groups();
	void RefreshVisuals();

	/** Visible shafts, replicated to every machine that sees the character. */
	UPROPERTY(ReplicatedUsing = OnRep_Groups)
	TArray<FRpgEmbeddedProjectileGroup> Groups;

	/** This machine's shaft meshes by group id. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UStaticMeshComponent>> Visuals;

	/** Server: one record per recovered projectile, in credit order. */
	TArray<FRecord> Records;

	/** Server: the corpse inventory's last seen count of each recovered ammunition. */
	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> ObservedCorpseCounts;

	int32 LastRecordId = 0;
	int32 LastGroupId = 0;
	bool bCorpseLootBound = false;
};
