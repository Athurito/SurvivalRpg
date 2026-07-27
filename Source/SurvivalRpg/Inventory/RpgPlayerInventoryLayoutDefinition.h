#pragma once

#include "Engine/DataAsset.h"
#include "RpgPlayerInventoryLayoutTypes.h"

#include "RpgPlayerInventoryLayoutDefinition.generated.h"

/**
 * One actionable failure found while validating a static player-inventory layout.
 *
 * The issue code is shared by editor validation and runtime filtering. GroupIndex identifies the
 * authored StaticSlotGroups entry; ConflictingGroupIndex identifies the earlier entry for duplicate
 * ids or roles.
 */
enum class ERpgInventoryStaticLayoutValidationIssue : uint8
{
	MissingContainerId,
	DuplicateContainerId,
	InvalidGridSize,
	InvalidGroupKind,
	ContentHasEquipmentSlotRole,
	CarryHasInvalidEquipmentSlotRole,
	GearHasInvalidEquipmentSlotRole,
	EquipmentGroupIsNotSingleCell,
	MissingActionbarCarrySemanticRole,
	SemanticRoleOutsideLayoutNamespace,
	DuplicateSemanticRole,
	DuplicateGearEquipmentSlotRole
};

/** Result of the shared static-layout validation pass used by editor and runtime consumers. */
struct SURVIVALRPG_API FRpgInventoryStaticLayoutValidationResult
{
	struct FIssue
	{
		ERpgInventoryStaticLayoutValidationIssue Type =
			ERpgInventoryStaticLayoutValidationIssue::MissingContainerId;
		int32 GroupIndex = INDEX_NONE;
		int32 ConflictingGroupIndex = INDEX_NONE;
	};

	/** True only when every authored static group satisfies the complete layout contract. */
	bool IsValid() const { return Issues.IsEmpty(); }

	/**
	 * Returns whether physical root and typed Gear classification are safe for authoritative preflights.
	 *
	 * Semantic-role and actionbar-only diagnostics do not block inventory reconciliation or death-drop;
	 * their dedicated runtime resolvers still fail closed when those semantics are requested.
	 */
	bool PassesPhysicalEquipmentPreflight() const;

	/**
	 * Returns whether the entry can safely become a root view.
	 *
	 * Ambiguous semantic or Gear roles remain materialized so unique runtime resolvers can detect
	 * the collision and fail closed. Physically malformed roots are rejected here.
	 */
	bool CanMaterializeGroup(int32 GroupIndex) const
	{
		return !NonMaterializableGroupIndices.Contains(GroupIndex);
	}

	/** Returns whether this group participates in the requested validation failure. */
	bool HasIssue(
		ERpgInventoryStaticLayoutValidationIssue Type,
		int32 GroupIndex) const;

	TArray<FIssue> Issues;
	TSet<int32> NonMaterializableGroupIndices;
};

/**
 * Immutable designer-authored root layout for a player inventory.
 *
 * PawnData owns this definition. Runtime item-provided containers are projected separately by the
 * controller-owned layout component and never mutate this asset.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPlayerInventoryLayoutDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Evaluates every static root through the shared editor/runtime contract.
	 *
	 * Container identity remains a root FRpgInventoryContainerHandle at runtime; this validation
	 * never aliases item-owned handles or derives gameplay semantics from physical names.
	 */
	FRpgInventoryStaticLayoutValidationResult ValidateStaticSlotGroups() const;

	/**
	 * Returns whether a semantic role is a concrete descendant of Rpg.Inventory.Layout.Role.
	 *
	 * Editor validation and runtime semantic resolvers share this predicate so invalid namespaces
	 * cannot become a second set of address keys.
	 */
	static bool IsConcreteLayoutSemanticRole(FGameplayTag SemanticRole);

	/** Returns whether GroupKind, typed equipment role, and grid size form a valid static group. */
	static bool IsStaticGroupEquipmentContractValid(
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole,
		const FRpgInventoryGridSize& GridSize);

#if WITH_EDITOR
	//~ UObject interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject interface
#endif

	/**
	 * Static gear, carry, and content roots in stable presentation order.
	 * This is cooked definition data: designers author it in the asset and runtime systems treat it as read-only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout", meta = (TitleProperty = "ContainerId"))
	TArray<FRpgInventorySlotGroupDefinition> StaticSlotGroups;
};
