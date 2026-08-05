#pragma once

#include "GameplayTagContainer.h"
#include "RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_StorageProfile.generated.h"

/** Determines how one item definition is represented after entering a local base storage network. */
UENUM(BlueprintType)
enum class ERpgInventoryStorageMode : uint8
{
	/** Preserve every concrete item instance, including identity, runtime state, and contained items. */
	GridItem,

	/** Convert only lossless, fungible instances into an abstract definition/count ledger. */
	BulkResource,

	/** Preserve the concrete instance and require a compatible containment domain before storage. */
	SpecialContainedItem
};

/**
 * Static base-storage routing contract for an item definition.
 *
 * This fragment never owns or mutates items. Inventory item instances remain authoritative until a server-side
 * storage transaction explicitly converts an eligible BulkResource into definition/count state.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Storage Profile"))
class SURVIVALRPG_API URpgInventoryFragment_StorageProfile final : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Representation used by authoritative base-storage transactions. Static item-definition data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage")
	ERpgInventoryStorageMode StorageMode = ERpgInventoryStorageMode::GridItem;

	/** Logical local-network destination. Must be a strict child of Storage.Domain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage", meta = (Categories = "Storage.Domain"))
	FGameplayTag StorageDomainTag;

	/**
	 * Capacity points consumed by one deposited BulkResource unit. Ignored by instance-preserving modes.
	 * The storage-network authority applies this value; UI may only display the derived result.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Bulk", meta = (EditCondition = "StorageMode == ERpgInventoryStorageMode::BulkResource", EditConditionHides, ClampMin = "1", UIMin = "1"))
	int32 BulkCapacityCost = 1;

	/**
	 * Allows eligible lossless instances to participate in return/deposit-all routing.
	 * This is only valid for BulkResource and never bypasses the instance collapse-safety check.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Bulk", meta = (EditCondition = "StorageMode == ERpgInventoryStorageMode::BulkResource", EditConditionHides))
	bool bCanAutoDeposit = false;

	/**
	 * Allows definition/count consumption from the local network without selecting a concrete instance.
	 * This is only valid for fungible BulkResource definitions.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Bulk", meta = (EditCondition = "StorageMode == ERpgInventoryStorageMode::BulkResource", EditConditionHides))
	bool bCanCraftFromNetwork = false;

	/** Functional storage capabilities required before this definition can use its configured network route. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Requirements", meta = (Categories = "Storage.Capability"))
	FGameplayTagContainer RequiredStorageCapabilityTags;

	/** Returns whether this definition is explicitly eligible for lossless bulk conversion. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool IsBulkResource() const
	{
		return StorageMode == ERpgInventoryStorageMode::BulkResource;
	}

	/** Returns whether the configured storage mode preserves a normal concrete grid-item instance. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool IsGridItem() const
	{
		return StorageMode == ERpgInventoryStorageMode::GridItem;
	}

	/** Returns whether this definition requires a concrete contained-item route. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool RequiresContainment() const
	{
		return StorageMode == ERpgInventoryStorageMode::SpecialContainedItem;
	}

	/**
	 * Returns static eligibility for a manual bulk-deposit request.
	 * The authority must still validate the concrete instance's lossless-collapse contract, access, and capacity.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool CanDepositAsBulk() const;

	/** Returns manual bulk-deposit eligibility after applying this profile's general storage capability requirements. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool CanDepositAsBulkWithCapabilities(const FGameplayTagContainer& NetworkCapabilities) const;

	/**
	 * Returns static eligibility for smart/deposit-all routing in a network with the supplied capabilities.
	 * Auto-deposit always requires explicit item opt-in and Storage.Capability.AutoDepositBulk; manual deposit does not.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	bool CanAutoDeposit(const FGameplayTagContainer& NetworkCapabilities) const;

	/** Resolves the effective first Storage Profile on an item definition, or null when none is authored. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	static const URpgInventoryFragment_StorageProfile* ResolveStorageProfile(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);

	/** Returns whether the definition has no item-owned containers or fragment runtime payloads that a count ledger would discard. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	static bool IsDefinitionIntrinsicallyCollapsible(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);

	/** Validates the profile's local scalar and gameplay-tag contract without inspecting runtime inventory state. */
	bool IsStructurallyValid() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
