// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Itemization/RpgItemizationTypes.h"
#include "RpgInventoryGraphTypes.h"
#include "SurvivalRpg/Systems/GameplayTagStack.h"
#include "Templates/SubclassOf.h"

#include "RpgInventoryItemInstance.generated.h"

class FLifetimeProperty;

class URpgInventoryManagerComponent;
class URpgInventoryItemDefinition;
class URpgInventoryItemFragment;
class URpgInventoryFragment_Itemization;
struct FFrame;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInventoryItemizationStateChanged,
	const FRpgItemizationState&,
	NewState);

/** Persistent lifecycle state of one concrete Rift-containment item instance. */
UENUM(BlueprintType)
enum class ERpgInventoryContainmentState : uint8
{
	/** Item has not completed its authored stabilization transaction. */
	Unstable,

	/** Item completed stabilization and may move to its definition-authored destination domains. */
	Stabilized
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInventoryContainmentStateChanged,
	ERpgInventoryContainmentState,
	NewState);

/**
 * Canonical, non-persisted value used for every stack-compatibility decision.
 *
 * The key contains only the static definition and canonical fragment runtime state. Persistent identity, entry
 * identity, quantity, placement, ownership, and UObject outer are deliberately excluded.
 */
struct SURVIVALRPG_API FRpgInventoryStackKey
{
public:
	/** Returns whether this key was built from a definition and strictly ordered, valid runtime-state payloads. */
	bool IsValid() const;

	/** Static item definition represented by this key. */
	TSubclassOf<URpgInventoryItemDefinition> GetItemDefinition() const { return ItemDefinition; }

	/** Canonically ordered, current-version runtime payloads represented by this key. */
	const TArray<FRpgInventoryFragmentStatePayload>& GetRuntimeState() const { return RuntimeState; }

	friend bool operator==(const FRpgInventoryStackKey& A, const FRpgInventoryStackKey& B);
	friend bool operator!=(const FRpgInventoryStackKey& A, const FRpgInventoryStackKey& B)
	{
		return !(A == B);
	}

private:
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
	TArray<FRpgInventoryFragmentStatePayload> RuntimeState;

	friend class URpgInventoryItemInstance;
};

/** Server-authored concrete item state with persistent identity independent of placement and replicated entry ids. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	explicit URpgInventoryItemInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UObject interface

	/** Adds an instance stat-tag count on authority; ignored for non-authority replicated instances. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** Removes an instance stat-tag count on authority; ignored for non-authority replicated instances. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category= Inventory)
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	/** Returns the instance-specific count for a stat tag. */
	UFUNCTION(BlueprintCallable, Category=Inventory)
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	/** Returns whether this instance has at least one count of the stat tag. */
	UFUNCTION(BlueprintCallable, Category=Inventory)
	bool HasStatTag(FGameplayTag Tag) const;

	/** Returns a copy of the replicated, save-backed generated item state for read-only UI/gameplay use. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	FRpgItemizationState GetItemizationState() const { return ItemizationState; }

	/** Returns the generated item state without a copy for native equipment and ability integration. */
	const FRpgItemizationState& GetItemizationStateRef() const { return ItemizationState; }

	/** Returns whether this concrete instance was explicitly generated from an itemization profile. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	bool HasGeneratedItemization() const { return ItemizationState.bGenerated; }

	/** Applies one complete server-authored state after validation by the definition's Itemization fragment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Itemization")
	bool ApplyItemizationState(const FRpgItemizationState& NewState);

	/** Fired locally on authority mutation and on clients after replicated itemization changes. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Itemization")
	FRpgInventoryItemizationStateChanged OnItemizationStateChanged;

	/** Returns the replicated, graph-save-backed lifecycle state of this concrete contained item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containment")
	ERpgInventoryContainmentState GetContainmentState() const
	{
		return ContainmentState;
	}

	/** True only after this exact item instance completed stabilization. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Containment")
	bool IsContainmentStabilized() const
	{
		return ContainmentState ==
			ERpgInventoryContainmentState::Stabilized;
	}

	/** Changes lifecycle state on authority; accepted only for definitions with one Containment Profile fragment. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Containment")
	bool SetContainmentState(ERpgInventoryContainmentState NewState);

	/** Fired locally on authority mutation and on clients after replicated containment-state changes. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Containment")
	FRpgInventoryContainmentStateChanged OnContainmentStateChanged;

	/** Returns the persistent item identity used by graph handles, transactions, saves, and quick-access bindings. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Identity")
	FRpgInventoryItemId GetItemId() const { return ItemId; }

	/** Generates a persistent id when absent. Only an authority-owned or transient test instance may initialize it. */
	bool InitializePersistentId();

	/** Restores a validated saved/transferred id before graph insertion. Rejects invalid ids and non-authority callers. */
	bool RestoreItemId(const FRpgInventoryItemId& InItemId);

	/**
	 * Copies all stack-relevant mutable state from Source.
	 * Preserve identity for cross-inventory reconstruction; leave it false for split stacks so the new item keeps its id.
	 */
	bool CopyRuntimeStateFrom(const URpgInventoryItemInstance* Source, bool bPreserveItemId);

	/** Builds the canonical, non-persisted definition/runtime-state key used by every stack merge decision. */
	bool TryBuildStackKey(FRpgInventoryStackKey& OutKey) const;

	/** Returns whether two distinct items have exactly the same canonical stack key. */
	bool IsStackCompatibleWith(const URpgInventoryItemInstance* Other) const;

	/**
	 * Returns whether this concrete item can be losslessly represented by a definition/count resource credit.
	 * Projection fails closed for container providers, semantic StatTags, or fragment-owned runtime payloads because a
	 * definition/count pool cannot preserve owned subtrees or rehydrate mutable instance state.
	 */
	bool CanCollapseIntoDefinitionCount() const;

	/** Exports core StatTags plus every fragment-owned versioned payload for atomic graph persistence. */
	bool ExportRuntimeState(TArray<FRpgInventoryFragmentStatePayload>& OutPayloads) const;

	/** Validates all payloads first, then restores core and fragment-owned state on authority. */
	bool ImportRuntimeState(const TArray<FRpgInventoryFragmentStatePayload>& Payloads);

	/** Reconstructs a transient staging instance and validates/imports saved payloads without touching live inventory state. */
	static bool ValidatePersistedRuntimeState(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		const TArray<FRpgInventoryFragmentStatePayload>& Payloads);

	TSubclassOf<URpgInventoryItemDefinition> GetItemDef() const
	{
		return ItemDef;
	}

	/** Finds a static fragment on this instance's item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category = "Inventory|Definition", meta=(DeterminesOutputType=FragmentClass))
	const URpgInventoryItemFragment* FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const;

	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return static_cast<const ResultClass*>(FindFragmentByClass(ResultClass::StaticClass()));
	}

	/** Register all replication fragments */
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
private:
	UFUNCTION()
	void OnRep_ItemizationState();

	UFUNCTION()
	void OnRep_ContainmentState();

	/** Restores an already validated historical roll without comparing it to rebalanced generation ranges. */
	bool RestorePersistedItemizationState(const FRpgItemizationState& NewState);
	bool CommitItemizationState(const FRpgItemizationState& NewState);
	void SetItemDef(TSubclassOf<URpgInventoryItemDefinition> InDef);
	bool HasAuthorityForMutation() const;

	friend struct FRpgInventoryList;
	friend struct FRpgLootResolver;
	friend class URpgInventoryFragment_Itemization;
	friend class URpgInventoryManagerComponent;

private:
	/** Persistent server-authored identity. Replicated to every connection allowed to receive this inventory instance. */
	UPROPERTY(Replicated, SaveGame)
	FRpgInventoryItemId ItemId;

	/** Mutable instance tags used by affixes/stats; replicated and serialized through the core runtime-state payload. */
	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

	/** Server-generated numeric rolls, replicated directly and serialized by the Itemization definition fragment. */
	UPROPERTY(ReplicatedUsing = OnRep_ItemizationState)
	FRpgItemizationState ItemizationState;

	/**
	 * Concrete containment lifecycle state. Replicated with the item and serialized by Containment Profile runtime
	 * payloads, so transfers, disconnects, and graph restores preserve stabilization without a vault-side shadow map.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ContainmentState)
	ERpgInventoryContainmentState ContainmentState =
		ERpgInventoryContainmentState::Unstable;

	/** Static fragment-composed definition shared by every instance of this item type. */
	UPROPERTY(Replicated)
	TSubclassOf<URpgInventoryItemDefinition> ItemDef;
};
