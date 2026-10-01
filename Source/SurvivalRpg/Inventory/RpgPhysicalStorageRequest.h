#pragma once

#include "CoreMinimal.h"
#include "RpgPhysicalStorageTypes.h"
#include "RpgPhysicalStorageRequest.generated.h"

class URpgBaseBuildableDefinition;
class URpgInventoryManagerComponent;

/** Physical chest commands accepted only through the requesting player's controller. */
UENUM(BlueprintType)
enum class ERpgPhysicalStorageCommand : uint8
{
	SetAssignments,
	DepositMaterials,
	Upgrade,
	Relocate,
	Build,
	/** Authorizes one move session while the requester can directly reach the chest. */
	BeginRelocate,
	/** Releases the requester's matching move session without changing the chest. */
	CancelRelocate
};

/** Client intent; all identities, costs, settings and placement are revalidated on authority. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPhysicalStorageRequest
{
	GENERATED_BODY()

	/** Fresh correlation ID; retries must retain exactly the same payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FGuid RequestId;

	/** Requested operation; the client never supplies a resulting item state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	ERpgPhysicalStorageCommand Command = ERpgPhysicalStorageCommand::DepositMaterials;

	/** Stable target chest identity; empty only for construction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FName ContainerId;

	/** Observed settings revision required for every existing-chest command. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	int32 ExpectedSettingsRevision = INDEX_NONE;

	/** BeginRelocate request ID required for Relocate/CancelRelocate; scoped to this controller and pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FGuid RelocationSessionId;

	/** Desired assignment slots. New rule ordering is always allocated by the server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	TArray<FRpgStorageAssignment> Assignments;

	/** Proposed placement for Build/Relocate; validated against the server's base and collision scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	FTransform Transform = FTransform::Identity;

	/** Authored construction definition; valid only for Build. Costs are read from this asset by authority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storage")
	TObjectPtr<URpgBaseBuildableDefinition> BuildableDefinition;
};

/** Controller-local replay record; reflected references retain request assets during the bounded replay window. */
USTRUCT()
struct FRpgPhysicalStorageCommandRecord
{
	GENERATED_BODY()
	UPROPERTY(Transient)
	FRpgPhysicalStorageRequest Request;
	UPROPERTY(Transient)
	FText Message;
	TWeakObjectPtr<URpgInventoryManagerComponent> PlayerInventory;
	uint64 PlayerMutationEpoch = 0;
	bool bInFlight = true;
	bool bSucceeded = false;
};

/** Owning-client presentation result. The replicated inventories remain gameplay truth. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRpgPhysicalStorageCommandCompleted,
	FGuid, RequestId, bool, bSucceeded, FText, Message);
