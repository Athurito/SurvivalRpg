#pragma once

#include "Components/ActorComponent.h"
#include "RpgPhysicalStorageTypes.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgInventoryContainerComponent.generated.h"

class URpgInventoryManagerComponent;
class URpgBaseBuildableDefinition;
class USceneComponent;
class FDataValidationContext;
class URpgInventoryContainerComponent;

/** Emitted after authoritative settings change and after replicated settings arrive. */
DECLARE_MULTICAST_DELEGATE_OneParam(FRpgPhysicalStorageSettingsChanged, URpgInventoryContainerComponent*);

/** Direction contract enforced for transfers involving an interactable world container. */
UENUM(BlueprintType)
enum class ERpgInventoryContainerTransferPolicy : uint8
{
	/** Players may deposit into and withdraw from this container. */
	Bidirectional,

	/** External inventories may only withdraw; authoritative deposits are rejected. */
	WithdrawOnly
};

/**
 * Marks an actor inventory as an interactable loot or shared-storage container.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgInventoryContainerComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FRpgPhysicalStorageSettingsChanged OnPhysicalStorageSettingsChanged;

	/** Current physical settings snapshot; the inventory graph is exported separately. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	FRpgPhysicalStorageMetadata ExportPhysicalStorageMetadata() const;

	/** Restores validated world-save settings before restoring inventory items. Authority only. */
	bool RestorePhysicalStorageMetadata(const FRpgPhysicalStorageMetadata& Metadata);

	/** Creates a stable save key once for this world chest on authority. */
	void EnsurePersistentContainerId();

	/** Changes the server-resolved spatial membership; never accepts client-supplied membership. */
	void SetResolvedBaseId(FName NewBaseId);

	/** Flags whether world restore must reconstruct this actor. Authority only. */
	void SetRuntimeBuilt(bool bNewRuntimeBuilt);

	/** Sets the zero-based tier after its capacity/cost transaction has passed validation. */
	void SetUpgradeTier(int32 NewTier);

	/** Advances settings revision after an authoritative move, invalidating older placement requests. */
	void MarkPhysicalStorageMoved();

	/** Applies an authority-approved transform while blocking nested chest actions from transform callbacks. */
	bool TryRelocatePhysicalStorage(const FTransform& Transform, int32 ExpectedRevision);

	/** Stages a newly constructed actor before costs commit; pending actors cannot interact, supply resources or save. */
	void SetConstructionPending(bool bPending);

	/** Server-only construction staging state; never persisted as a completed chest. */
	bool IsConstructionPending() const { return bConstructionPending; }

	/** Replaces destination rules, reusing unchanged rules and assigning new orders on authority. */
	bool SetAssignments(const TArray<FRpgStorageAssignment>& NewAssignments, int32 ExpectedRevision = INDEX_NONE);

	/** Zero exact item, one category, two existing-stock general chest; INDEX_NONE has no destination. */
	int32 GetAssignmentRank(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int64& OutOrder) const;

	/** Replicated and saved destination rules; callers must use SetAssignments for changes. */
	const TArray<FRpgStorageAssignment>& GetAssignments() const;

	/** Current authoritative settings revision for stale request rejection. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	int32 GetSettingsRevision() const;

	/** Current resolved base membership, or None outside any base. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	FName GetBaseId() const;

	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Designer-owned construction and upgrade tuning for this chest. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	URpgBaseBuildableDefinition* GetBuildableDefinition() const { return BuildableDefinition; }

	/** Inventory manager on the same actor that stores this container's replicated item entries. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	URpgInventoryManagerComponent* GetInventoryManager() const;

	/** Returns true when the actor may currently open or transfer with this container. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory|Container")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Runtime toggle for corpses, locked chests, or scripted storage state. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Container")
	void SetContainerAccessible(bool bNewAccessible);

	/** Sets the maximum direct-access distance in centimeters. Runtime changes are server-authoritative and replicated. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Container")
	void SetInteractionRadius(float NewInteractionRadius);

	/**
	 * Selects the same-owner scene component used for interaction prompts and authoritative range checks.
	 * The reference is local runtime wiring and should be assigned on every role, normally by the owning actor/component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Container|Interaction")
	void SetInteractionAnchor(USceneComponent* NewInteractionAnchor);

	/** Returns the current anchor location, falling back to the owning actor's location. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container|Interaction")
	FVector GetInteractionWorldLocation() const;

	/** Changes whether external inventories may deposit into this container. Server-authoritative and replicated. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Container")
	void SetTransferPolicy(ERpgInventoryContainerTransferPolicy NewTransferPolicy);

	/**
	 * Configures this native default subobject as latent death loot.
	 * Death loot starts inaccessible and emits no interaction option until the server unlocks it.
	 */
	void ConfigureAsDeathLootContainer();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	bool IsContainerAccessible() const { return bAccessible && !bConstructionPending && !bPhysicalMoveInProgress; }

	/** Returns the transfer direction policy used by UI prediction and authoritative inventory commits. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	ERpgInventoryContainerTransferPolicy GetTransferPolicy() const { return TransferPolicy; }

	/** Returns whether SourceInventory may transfer into the inventory represented by this container. */
	bool CanReceiveTransferFrom(const URpgInventoryManagerComponent* SourceInventory) const;

	/** Identifier shared by nearby crafting stations and storage containers that belong to the same base area. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	FName GetStorageGroupId() const { return StorageGroupId; }

	/** Stable save/export id for this world container. Leave None for non-persistent loot proxies. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	FName GetPersistentContainerId() const { return PersistentContainerId; }

	/** True when crafting/resource scans may pull items from this container. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	bool AllowsCraftingAccess() const { return bAllowCraftingAccess && !bConstructionPending; }

	/** Maximum distance in centimeters for direct player transfer access. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory|Container")
	float GetInteractionRadius() const { return InteractionRadius; }

protected:
	/** Construction and tier data; concrete chest Blueprints choose this asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage")
	TObjectPtr<URpgBaseBuildableDefinition> BuildableDefinition;

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
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Inventory|Container", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/**
	 * Controls transfer direction for this world container. Runtime state is server-owned and replicated for UI preflight.
	 * Internal moves within the represented inventory are always permitted by this policy.
	 */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Inventory|Container")
	ERpgInventoryContainerTransferPolicy TransferPolicy = ERpgInventoryContainerTransferPolicy::Bidirectional;

	/** Whether this container currently accepts player interaction. Runtime mutable on the server and read by UI. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Inventory|Container")
	bool bAccessible = true;

	/**
	 * Whether an inaccessible container is completely absent from interaction scans instead of showing Blocked.
	 * Enable for latent targets such as living enemies whose corpse loot becomes valid only after death.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container|Interaction")
	bool bHideInteractionWhenInaccessible = false;

	/** Whether crafting stations may pull resources from this container. Use this for shared base chests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Container")
	bool bAllowCraftingAccess = true;

private:
	/** Authority-only synchronous guard; contents stay on this actor throughout relocation. */
	bool bPhysicalMoveInProgress = false;

	/** Temporary authority-only admission gate while a construction transaction stages this actor. */
	bool bConstructionPending = false;

	/** Saved server-owned settings replicated as one coherent value. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_PhysicalStorageMetadata, Category = "Inventory|Storage")
	FRpgPhysicalStorageMetadata PhysicalStorageMetadata;

	UFUNCTION()
	void OnRep_PhysicalStorageMetadata();
	void NotifyPhysicalStorageSettingsChanged();

	/** Local same-owner prompt/range anchor; runtime wiring is reconstructed instead of replicated as an object reference. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> InteractionAnchor;
};
