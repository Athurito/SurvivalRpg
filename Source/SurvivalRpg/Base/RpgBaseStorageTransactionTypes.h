#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgBaseStorageTransactionTypes.generated.h"

class APlayerController;
class URpgBaseStorageUpgradeDefinition;
class URpgInventoryItemDefinition;

/** Stable outcome code for one server-authoritative base-storage command. */
UENUM(BlueprintType)
enum class ERpgBaseStorageResultCode : uint8
{
	Success,
	Partial,
	InvalidRequest,
	ConfirmationRequired,
	NoAccess,
	MissingItem,
	Stale,
	CapacityFull,
	NoPlacement,
	UnsupportedMode,
	CapabilityLocked,
	KnowledgeMissing,
	MissingCosts,
	StrainBlocked,
	Conflict,
	InternalRollback
};

/** Shared optimistic-concurrency envelope supplied with every modern storage command. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageRequestContext
{
	GENERATED_BODY()

	/** Caller-generated id used by the bounded server replay cache and owning-client feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FGuid RequestId;

	/** Stable target base id; requests never select a base only through a live station pointer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FName BaseId = NAME_None;

	/** Replicated revision observed by the caller; public modern RPCs reject INDEX_NONE, which is reserved for trusted compatibility code. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	int64 ExpectedNetworkRevision = INDEX_NONE;

	void EnsureRequestId()
	{
		if (!RequestId.IsValid())
		{
			RequestId = FGuid::NewGuid();
		}
	}
};

/** Per-stack outcome returned by Smart Deposit so UI can explain every item that stayed or moved. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageResourceCommandOutcome
{
	GENERATED_BODY()

	/** Exact source item identity when the outcome originated from a concrete player stack. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	FRpgInventoryItemId ItemId;

	/** Resource definition represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Units inspected in the source stack. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	int32 RequestedCount = 0;

	/** Units committed to or from the network for this row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	int32 AppliedCount = 0;

	/** Specific reason for complete, partial, or rejected handling of this row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	ERpgBaseStorageResultCode Code = ERpgBaseStorageResultCode::InvalidRequest;
};

/** Request-correlated result retained by the server replay window and delivered to the owning client. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageCommandResult
{
	GENERATED_BODY()

	/** Correlation id copied unchanged from the admitted request. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	FGuid RequestId;

	/** Stable base id evaluated by the authoritative storage component. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	FName BaseId = NAME_None;

	/** Semantic command outcome suitable for localized terminal feedback. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	ERpgBaseStorageResultCode Code = ERpgBaseStorageResultCode::InvalidRequest;

	/** Requested unit count, or zero for non-count commands. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	int32 RequestedCount = 0;

	/** Units actually committed by the command. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	int32 AppliedCount = 0;

	/** Authoritative network revision after completion or replay. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	int64 NetworkRevision = 0;

	/** Per-definition requested/applied deltas for deposit, costs, or refunds; preserved verbatim by idempotent replay. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Transaction")
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;

	/** Deterministic extraction output definition shown during confirmation and reported after commit. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	TSubclassOf<URpgInventoryItemDefinition> RiftOutputItemDefinition;

	/** Previewed or actually granted extraction output units; zero when the command has no Rift output. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	int32 RiftOutputCount = 0;

	/** Authoritative strain before the evaluated Rift operation, or -1 when strain was not evaluated. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	int32 RiftStrainBefore = INDEX_NONE;

	/** Previewed or committed strain after the evaluated Rift operation, or -1 when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	int32 RiftStrainAfter = INDEX_NONE;

	/** True when the result contains an authoritative before/after stabilization snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	bool bHasStabilizationState = false;

	/** Stabilization state before the evaluated operation when bHasStabilizationState is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	bool bWasStabilized = false;

	/** Previewed or committed stabilization state after the evaluated operation. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Rift")
	bool bIsStabilized = false;

	bool IsSuccess() const
	{
		return Code == ERpgBaseStorageResultCode::Success || Code == ERpgBaseStorageResultCode::Partial;
	}
};

/** Stable exact-item deposit command replacing the legacy live item-pointer RPC. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageDepositRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope captured by the requesting client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgBaseStorageRequestContext Context;

	/** Persistent identity of the concrete player-owned item instance to collapse into bulk credits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgInventoryItemId ItemId;

	/** Replicated entry identity displayed when the client selected the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FGuid ExpectedEntryId;

	/** Exact source placement displayed when the client selected the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgInventoryGridPlacement ExpectedSourcePlacement;

	/** Player-inventory revision displayed when the exact entry snapshot was captured. Modern deposits require it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	int32 ExpectedInventoryRevision = INDEX_NONE;

	/** Exact stack quantity displayed for the selected entry; the server rejects a changed stack as stale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction", meta = (ClampMin = "1", UIMin = "1"))
	int32 ExpectedSourceQuantity = 0;

	/** Number of units requested from the exact stack; server-authoritative and never silently exceeds the snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequestedCount = 0;
};

/** Request-correlated one-action unload of every eligible BulkResource stack. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageSmartDepositRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope for one partial-capable smart unload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgBaseStorageRequestContext Context;
};

/** Stable material withdrawal from shared bulk state into the requesting player's concrete inventory. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageWithdrawRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope captured by the requesting client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgBaseStorageRequestContext Context;

	/** Exact fungible BulkResource definition requested from the shared Materials domain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of physical units to grant to the owning player's inventory after the base debit commits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequestedCount = 1;
};

/** Stable PrimaryAsset-based install or decommission request for one unique domain anchor. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageUpgradeRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope captured by the requesting client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FRpgBaseStorageRequestContext Context;

	/** Stable server-resolved PrimaryAssetId of the install or decommission definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FPrimaryAssetId UpgradeId;

	/** Exact authored domain-anchor id expected on the resolved upgrade definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Transaction")
	FName ExpectedAnchorId = NAME_None;
};

/** Stable exact-item request shared by stabilization and destructive Rift extraction. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageRiftItemRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope captured by the requesting client. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	FRpgBaseStorageRequestContext Context;

	/** Persistent identity of the exact concrete item in the base containment graph. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	FRpgInventoryItemId ItemId;

	/** Containment inventory revision displayed by the caller when the action was selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	int32 ExpectedContainmentRevision = INDEX_NONE;

	/** Stabilization state displayed for the exact item; server rejects a changed state as stale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	bool bExpectedStabilized = false;

	/** Required only for destructive extraction; stabilization rejects a populated confirmation flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	bool bConfirmed = false;
};

/** Request-correlated cleanse operation using the base's designer-authored early-plus-Rift material costs. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageCleanseRequest
{
	GENERATED_BODY()

	/** Replay, target-base, and optimistic-concurrency envelope for one designer-authored cleanse operation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage|Rift")
	FRpgBaseStorageRequestContext Context;
};

/** Owning-client gameplay-message payload for one typed base-storage command result. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageCommandFeedbackMessage
{
	GENERATED_BODY()

	/** Owning controller filled locally after the reliable client RPC; never trusted as request input. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Feedback")
	TObjectPtr<APlayerController> Recipient = nullptr;

	/** Typed storage action that produced the result, used to route presentation. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Feedback")
	FGameplayTag ActionTag;

	/** Immutable server-authored command result, including correlation id and final network revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Feedback")
	FRpgBaseStorageCommandResult Result;

	/** Exact item identity relevant to the command, or invalid for definition-only operations. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Feedback")
	FRpgInventoryItemId ItemId;

	/** Relevant item definition when disclosure is allowed; access-denied feedback deliberately clears it. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Feedback")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
};
