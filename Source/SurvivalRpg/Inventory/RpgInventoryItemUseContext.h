#pragma once

#include "UObject/Object.h"

#include "RpgInventoryItemUseContext.generated.h"

class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;

/**
 * Transient server-side source object passed into one-shot item-use abilities.
 *
 * The context carries the authoritative inventory stack that may be consumed by a configured use sequence step.
 * It is not replicated or saved; UI reads inventory replication after the server mutates the stack.
 */
UCLASS(BlueprintType, Transient)
class SURVIVALRPG_API URpgInventoryItemUseContext : public UObject
{
	GENERATED_BODY()

public:
	/** Initializes the context immediately before GAS activates the item-use ability. */
	void Initialize(URpgInventoryManagerComponent* InInventory, URpgInventoryItemInstance* InItemInstance, int32 InRequestedUseCount, int32 InConsumeCount);

	/** Attempts to consume the configured stack count once. Returns true for zero-consume items. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Use")
	bool TryConsume();

	/** Inventory that owns ItemInstance for this use request. Server-authoritative and runtime-only. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Use")
	TObjectPtr<URpgInventoryManagerComponent> Inventory;

	/** Item instance being used. Also passed as GameplayEventData.OptionalObject for Blueprint convenience. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Use")
	TObjectPtr<URpgInventoryItemInstance> ItemInstance;

	/** Requested use multiplier, usually 1 for V1 consumables. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Use")
	int32 RequestedUseCount = 1;

	/** Total item units this use should consume when a sequence step asks for consumption. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Use")
	int32 ConsumeCount = 0;

	/** True once TryConsume has succeeded or the context was configured with zero consumption. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Use")
	bool bConsumed = false;
};
