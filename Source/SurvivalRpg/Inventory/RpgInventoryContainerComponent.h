#pragma once

#include "Components/ActorComponent.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgInventoryContainerComponent.generated.h"

class URpgInventoryManagerComponent;

/**
 * Marks an actor inventory as an interactable loot or shared-storage container.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgInventoryContainerComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Inventory manager on the same actor that stores this container's replicated item entries. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	URpgInventoryManagerComponent* GetInventoryManager() const;

	/** Returns true when the actor may currently open or transfer with this container. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Runtime toggle for corpses, locked chests, or scripted storage state. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Container")
	void SetContainerAccessible(bool bNewAccessible);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	bool IsContainerAccessible() const { return bAccessible; }

	/** Identifier shared by nearby crafting stations and storage containers that belong to the same base area. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	FName GetStorageGroupId() const { return StorageGroupId; }

	/** Stable save/export id for this world container. Leave None for non-persistent loot proxies. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	FName GetPersistentContainerId() const { return PersistentContainerId; }

	/** True when crafting/resource scans may pull items from this container. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	bool AllowsCraftingAccess() const { return bAllowCraftingAccess; }

	/** Maximum distance in centimeters for direct player transfer access. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	float GetInteractionRadius() const { return InteractionRadius; }

protected:
	/** Interaction option shown by Lyra-style interaction UI when this container is accessible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	FInteractionOption OpenContainerOption;

	/** Storage/base group used by crafting stations to include same-base containers even outside direct interaction range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	FName StorageGroupId;

	/** Stable id used by inventory snapshots and future world-save data for this container. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Inventory|Container")
	FName PersistentContainerId;

	/** Maximum distance in centimeters at which a player may transfer items with this container directly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Whether this container currently accepts player interaction. Runtime mutable on the server and read by UI. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Inventory|Container")
	bool bAccessible = true;

	/** Whether crafting stations may pull resources from this container. Use this for shared base chests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	bool bAllowCraftingAccess = true;
};
