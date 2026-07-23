// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RpgInventoryGraphTypes.h"

#include "RpgInventoryItemDefinition.generated.h"

template <typename T> class TSubclassOf;

class URpgInventoryFragment_SpatialItem;
class URpgInventoryItemInstance;
class FDataValidationContext;
struct FFrame;

//////////////////////////////////////////////////////////////////////

/**
 * One immutable modular data block on an item definition.
 *
 * Fragments may optionally provide a versioned runtime-state seam for durability, affixes, sockets, rolls, or other
 * instance-specific data. The item instance owns mutable state; definition fragments remain static CDO data.
 */
UCLASS(DefaultToInstanced, EditInlineNew, Abstract)
class SURVIVALRPG_API URpgInventoryItemFragment : public UObject
{
	GENERATED_BODY()

public:
	/** Called on authority after a new concrete item instance receives its definition and persistent identity. */
	virtual void OnInstanceCreated(URpgInventoryItemInstance* Instance) const {}

	/** Stable save-payload id owned by this fragment, or None when the fragment has no mutable runtime state. */
	virtual FName GetRuntimeStateIdentifier() const;

	/** Current schema version emitted by this fragment's runtime-state payload. */
	virtual int32 GetRuntimeStateVersion() const;

	/**
	 * Exports this fragment's complete stack-relevant mutable state from an item.
	 * The output must be deterministic and side-effect-free: semantically equal state must produce identical current-
	 * version bytes, excluding identity, placement, ownership, and replication bookkeeping.
	 */
	virtual bool ExportRuntimeState(
		const URpgInventoryItemInstance* Instance,
		FRpgInventoryFragmentStatePayload& OutPayload) const;

	/** Validates an imported payload without mutating the item. A successful validation must make import infallible. */
	virtual bool ValidateRuntimeState(
		const URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const;

	/** Applies a previously validated payload to an authority-owned item instance. */
	virtual bool ImportRuntimeState(
		URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const;

	/** Copies this fragment's mutable runtime state for split or cross-inventory reconstruction. */
	virtual void CopyRuntimeState(
		const URpgInventoryItemInstance* Source,
		URpgInventoryItemInstance* Target) const;
};

//////////////////////////////////////////////////////////////////////

/** Static fragment-composed definition shared by all concrete instances of one item type. */
UCLASS(Blueprintable, Const, Abstract)
class URpgInventoryItemDefinition : public UObject
{
	GENERATED_BODY()

public:
	explicit URpgInventoryItemDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Player-facing item name shown in inventory and pickup UI. Static definition data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display)
	FText DisplayName;

	/** Modular static behavior/data, including equipment, item-owned containers, UI, stack rules, and runtime-state seams. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Display, Instanced)
	TArray<TObjectPtr<URpgInventoryItemFragment>> Fragments;

public:
	/** Finds the first fragment compatible with FragmentClass. */
	const URpgInventoryItemFragment* FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const;

	/**
	 * Returns the sole valid SpatialItem fragment that defines this item's unrotated grid footprint.
	 * Missing, duplicate, or non-positive SpatialItem data fails closed so runtime placement never invents 1x1 metadata.
	 */
	const URpgInventoryFragment_SpatialItem* FindValidSpatialItemFragment() const;

	/** Resolves the valid SpatialItem contract from an item-definition class default object, or null on malformed data. */
	static const URpgInventoryFragment_SpatialItem* ResolveValidSpatialItemFragment(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);

#if WITH_EDITOR
	/**
	 * Reports malformed static fragment contracts to the editor.
	 * Runtime placement remains authoritative and consumes the same fail-closed SpatialItem resolver.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

//@TODO: Make into a subsystem instead?
UCLASS()
class URpgInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Finds one static fragment on an item definition without constructing a runtime item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Definition", meta=(DeterminesOutputType=FragmentClass))
	static const URpgInventoryItemFragment* FindItemDefinitionFragment(TSubclassOf<URpgInventoryItemDefinition> ItemDef, TSubclassOf<URpgInventoryItemFragment> FragmentClass);
};
