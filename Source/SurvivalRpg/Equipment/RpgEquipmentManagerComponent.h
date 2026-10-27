#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentManagerComponent.generated.h"

class URpgAbilitySystemComponent;
class URpgEquipmentDefinition;
class URpgEquipmentInstance;
class URpgInventoryManagerComponent;
class URpgPawnExtensionComponent;
class UAnimInstance;
class USkeletalMeshComponent;
struct FNetDeltaSerializeInfo;

/**
 * Runtime handles for one AbilitySet source on an equipped item.
 *
 * The source key is stable within the equipment definition, allowing unchanged grants to survive
 * loadout changes without briefly removing persistent GameplayEffects.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgAppliedEquipmentAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const URpgAbilitySet> AbilitySet = nullptr;

	UPROPERTY()
	FRpgAbilitySet_GrantedHandles GrantedHandles;
};

/**
 * Runtime grants of one learned skill tree node, or of one rune socketed into the item, while the equipped item is in
 * use. Server only; rebuilt whenever equipment, the player's skill tree or the item's sockets change.
 */
USTRUCT()
struct SURVIVALRPG_API FRpgAppliedItemGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const URpgAbilitySet> AbilitySet = nullptr;

	UPROPERTY()
	FRpgAbilitySet_GrantedHandles GrantedHandles;

	/** Loose replicated tags added for the node or rune; removed with the same count. */
	UPROPERTY()
	FGameplayTagContainer GrantedTags;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FString GetDebugString() const;

private:
	friend struct FRpgEquipmentList;
	friend class URpgEquipmentManagerComponent;

	UPROPERTY()
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition;

	UPROPERTY()
	ERpgEquipmentSlot EquippedSlot = ERpgEquipmentSlot::None;

	UPROPERTY()
	TObjectPtr<URpgEquipmentInstance> Instance = nullptr;

	UPROPERTY(NotReplicated)
	TMap<int32, FRpgAppliedEquipmentAbilityGrant> AbilitySetGrants;

	/** Server-only grants of the learned nodes of the item's skill tree, keyed by node tag. */
	UPROPERTY(NotReplicated)
	TMap<FGameplayTag, FRpgAppliedItemGrant> SkillNodeGrants;

	/** Server-only grants of the runes socketed into the item, keyed by the rune item's persistent id. */
	UPROPERTY(NotReplicated)
	TMap<FRpgInventoryItemId, FRpgAppliedItemGrant> SocketGrants;

	/** Server-only handle for the concrete inventory item's generated global-stat effect. */
	FActiveGameplayEffectHandle ItemizationEffectHandle;

	/** Last generated state represented by ItemizationEffectHandle; never replicated or saved. */
	UPROPERTY(NotReplicated)
	FRpgItemizationState AppliedItemizationState;

	/** Distinguishes an intentionally cached legacy/empty state from an entry that has never been evaluated. */
	UPROPERTY(NotReplicated)
	bool bHasAppliedItemizationState = false;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgEquipmentList() = default;
	explicit FRpgEquipmentList(UActorComponent* InOwnerComponent) : OwnerComponent(InOwnerComponent) {}

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgAppliedEquipmentEntry, FRpgEquipmentList>(Entries, DeltaParms, *this);
	}

	URpgEquipmentInstance* AddEntry(
		TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition,
		ERpgEquipmentSlot EquippedSlot,
		UObject* SourceItemInstigator);
	void RemoveEntry(URpgEquipmentInstance* Instance);

private:
	URpgAbilitySystemComponent* GetAbilitySystemComponent() const;

	friend class URpgEquipmentManagerComponent;

	UPROPERTY()
	TArray<FRpgAppliedEquipmentEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FRpgEquipmentList> : public TStructOpsTypeTraitsBase2<FRpgEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(BlueprintType, Const, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URpgEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Native authority seam that equips the definition into its default slot. */
	URpgEquipmentInstance* EquipItem(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition);

	/** Native authority seam that equips the definition into one explicit slot. */
	URpgEquipmentInstance* EquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot);

	/**
	 * Creates runtime equipment with its inventory source assigned before actors or GAS grants are built.
	 * Inventory/loadout reconciliation should use this path so ability SourceObject is never temporarily null.
	 */
	URpgEquipmentInstance* EquipItemInSlotWithInstigator(
		TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition,
		ERpgEquipmentSlot Slot,
		UObject* SourceItemInstigator);

	/** Native authority seam that removes one exact runtime equipment instance. */
	void UnequipItem(URpgEquipmentInstance* ItemInstance);

	/** Native authority seam that removes the runtime equipment occupying Slot. */
	void UnequipItemInSlot(ERpgEquipmentSlot Slot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	URpgEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	TArray<URpgEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<URpgEquipmentInstance> InstanceType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	URpgEquipmentInstance* GetEquipmentInstanceInSlot(ERpgEquipmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsEquipmentSlotBlocked(ERpgEquipmentSlot Slot) const;

	/**
	 * Replicated read model: true while an applied equipment instance occupies the main or off hand, weapon or tool.
	 * Authority, owner, simulated proxies and late joiners read the same replicated equipment list.
	 */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool HasEquipmentInHands() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment")
	bool IsEquipmentInstanceActiveForInputTag(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const;

	/** Active blocking item using the same offhand-first rule as GAS grants; null while its replicated instance is unresolved. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Block")
	URpgEquipmentInstance* GetActiveBlockSource() const;

	/** Current equipment-owned linked instance on the gameplay mesh, or null until local presentation is ready. Game thread only. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Block")
	UAnimInstance* GetBlockLocomotionLayerInstance() const;

	/**
	 * Hold layer instance linked on the gameplay mesh for the item in a hand slot, or null while that hand is empty, its
	 * definition has no hold presentation for the mesh's skeleton, or local presentation is not ready. Cosmetic; game thread only.
	 */
	UFUNCTION(BlueprintPure, Category = "Equipment|Animation")
	UAnimInstance* GetWeaponHoldLayerInstance(ERpgEquipmentSlot Slot) const;

	/**
	 * Authority: re-grants the learned skill tree nodes of every equipped item after the player's skill tree changed,
	 * then re-resolves the owning controller's Q/E/R and quick-access bindings.
	 */
	void RefreshSkillTreeGrants();

	template <typename T>
	T* GetFirstInstanceOfType() const
	{
		return Cast<T>(GetFirstInstanceOfType(T::StaticClass()));
	}

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void ReadyForReplication() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend struct FRpgEquipmentList;
	void RefreshBlockLocomotionLayer();
	void ClearBlockLocomotionLayer();
	void HandleBlockAvatarInitialized();
	void HandleBlockAvatarUninitialized();
	UFUNCTION()
	void HandleBlockAnimationInitialized();
	TWeakObjectPtr<URpgPawnExtensionComponent> BlockPawnExtension;
	TWeakObjectPtr<USkeletalMeshComponent> BlockGameplayMesh;
	TWeakObjectPtr<UAnimInstance> BlockMainAnimInstance;
	TWeakObjectPtr<URpgEquipmentInstance> BlockLayerSource;
	TWeakObjectPtr<UClass> BlockLayerClass;
	bool bRefreshingBlockLayer = false;
	bool bBlockLayerShuttingDown = false;

	/** One hold layer linked for an equipped hand; local presentation state, never replicated. */
	struct FWeaponHoldBinding
	{
		TWeakObjectPtr<UClass> LayerClass;
		ERpgEquipmentSlot Slot = ERpgEquipmentSlot::None;
		bool operator==(const FWeaponHoldBinding& Other) const { return LayerClass == Other.LayerClass && Slot == Other.Slot; }
	};
	/** Links the hold layers of the items in hand on MainInstance; Skeleton is the gameplay mesh's skeleton. */
	void RefreshWeaponHoldLayers(UAnimInstance* MainInstance, const USkeleton* Skeleton);
	void ClearWeaponHoldLayers();
	TWeakObjectPtr<UAnimInstance> HoldMainAnimInstance;
	TArray<FWeaponHoldBinding, TInlineAllocator<2>> HoldBindings;

	bool CanEquipItemInSlot(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot) const;
	void UnequipConflictingItems(TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition, ERpgEquipmentSlot Slot);
	bool DoesEquipmentOccupySlot(const FRpgAppliedEquipmentEntry& Entry, ERpgEquipmentSlot Slot) const;
	bool CanEquipmentBlock(const URpgEquipmentInstance* EquipmentInstance) const;
	bool ShouldGrantSlotAbilitySet(const FRpgAppliedEquipmentEntry& Entry, const FRpgEquipmentSlotAbilitySet& SlotAbilitySet, const URpgEquipmentInstance* ActiveBlockSource) const;

	UFUNCTION()
	void HandleEquippedItemizationStateChanged(const FRpgItemizationState& NewState);

	void RefreshEquipmentItemizationEffect(FRpgAppliedEquipmentEntry& Entry);
	void RebuildEquipmentItemizationEffects();
	void RebuildEquipmentAbilityGrants();

	/** Authority: follows the commits of exactly the inventories that hold equipped items, so socket changes regrant. */
	void WatchSourceInventories(const TArray<URpgInventoryManagerComponent*>& SourceInventories);
	void StopWatchingSourceInventories();
	void HandleWatchedInventoryCommitted(URpgInventoryManagerComponent* Inventory);

	/** Inventories whose commits rebuild the grants; server only, never replicated or saved. */
	TArray<TWeakObjectPtr<URpgInventoryManagerComponent>> WatchedInventories;

	/** Re-resolves the owning controller's runtime ability bindings on the next tick after equipment grants changed. Authority only. */
	void RefreshOwnerAbilityBindings() const;

	UPROPERTY(Replicated)
	FRpgEquipmentList EquipmentList;
};
