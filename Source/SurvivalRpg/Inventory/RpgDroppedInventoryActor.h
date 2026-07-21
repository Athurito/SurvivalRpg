#pragma once

#include "SurvivalRpg/Core/RpgWorldCollectable.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgDroppedInventoryActor.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryContainerComponent;
class URpgInventoryManagerComponent;

/**
 * Simple world pickup actor used for death drops, loot bundles, and scripted inventory piles.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgDroppedInventoryActor : public ARpgWorldCollectable
{
	GENERATED_BODY()

public:
	ARpgDroppedInventoryActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual FInventoryPickup GetPickupInventory() const override;

	/** Sets the pickup contents before players interact with this actor. Server-authoritative runtime state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	void SetPickupInventory(const FInventoryPickup& NewPickupInventory);

	/** Inventory shown when the player cannot auto-take the whole drop. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Drop")
	URpgInventoryManagerComponent* GetLootInventoryManager() const { return LootInventoryComponent; }

	/** Returns whether the replicated loot manager, rather than the pre-initialization pickup payload, is canonical. */
	bool IsLootInventoryCanonical() const { return bLootInventoryInitialized; }

	/**
	 * Moves one exact source-entry snapshot into this actor's authoritative loot inventory.
	 * A full container provider keeps its persistent identity, runtime state, placements, and complete descendant subtree.
	 * Set bPreventStackMerge for death/save semantics that must retain each source ItemId as a separate concrete stack.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	FRpgInventoryMutationResult TransferItemFromInventoryByIntent(
		URpgInventoryManagerComponent* SourceInventory,
		FRpgInventoryTransferIntent Intent,
		bool bPreventStackMerge = false);

	/** Legacy server-side adapter that snapshots the current entry before forwarding to the typed drop seam. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop", meta = (DeprecatedFunction, DeprecationMessage = "Capture FRpgInventoryTransferIntent from the source entry and use TransferItemFromInventoryByIntent."))
	FRpgInventoryMutationResult TransferItemFromInventory(
		URpgInventoryManagerComponent* SourceInventory,
		FRpgInventoryItemId ItemId,
		int32 StackCount,
		FGuid RequestId,
		bool bPreventStackMerge = false);

	/** Adds count to an existing template pickup with the same definition, or creates one. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Drop")
	bool MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount);

	/** Returns true when this drop can merge another stack of the given item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Drop")
	bool CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

private:
	void EnsureDefaultPickupInteractionOption();
	bool PopulateLootInventoryFromPickup(const FInventoryPickup& PickupInventory);
	FInventoryPickup BuildPickupInventoryFromLootInventory() const;
	struct FRecentDropTransferResult
	{
		TWeakObjectPtr<URpgInventoryManagerComponent> SourceInventory;
		bool bHadSourceInventory = false;
		uint64 SourceMutationEpoch = 0;
		TWeakObjectPtr<URpgInventoryManagerComponent> TargetInventory;
		bool bHadTargetInventory = false;
		uint64 TargetMutationEpoch = 0;
		FRpgInventoryTransferIntent Intent;
		bool bPreventStackMerge = false;
		FRpgInventoryMutationResult Result;
	};
	static bool AreDropTransferIntentsEquivalent(
		const FRpgInventoryTransferIntent& A,
		const FRpgInventoryTransferIntent& B);
	bool TryReplayRecentDropTransfer(
		URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryTransferIntent& Intent,
		bool bPreventStackMerge,
		FRpgInventoryMutationResult& OutResult) const;
	FRpgInventoryMutationResult CacheRecentDropTransfer(
		URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryTransferIntent& Intent,
		bool bPreventStackMerge,
		FRpgInventoryMutationResult Result);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryManagerComponent> LootInventoryComponent;

	/** Access/relevancy boundary used by the same server-side transfer validation as world storage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory|Drop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryContainerComponent> ContainerComponent;

	/** True after PostInitializeComponents makes the runtime manager canonical on this local role. */
	UPROPERTY(Transient)
	bool bLootInventoryInitialized = false;

	/** Server-local replay cache for physical drop commands whose target placement is derived once. */
	TMap<FGuid, FRecentDropTransferResult> RecentDropTransferResults;
	TArray<FGuid> RecentDropTransferOrder;
	static constexpr int32 MaxRecentDropTransferResults = 64;
};
