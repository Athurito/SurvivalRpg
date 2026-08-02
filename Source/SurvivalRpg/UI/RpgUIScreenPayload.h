#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"

#include "RpgUIScreenPayload.generated.h"

class AActor;
class UActorComponent;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgCraftingStationComponent;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/**
 * Implemented by activatable screens that accept an optional UObject payload after they are pushed.
 */
UINTERFACE(BlueprintType)
class URpgUIScreenPayloadReceiver : public UInterface
{
	GENERATED_BODY()
};

class SURVIVALRPG_API IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	/** Called by URpgUIScreenSubsystem after a screen is created or an existing single-instance screen is reused. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Screens")
	void ReceiveScreenPayload(UObject* Payload);
};

/**
 * Base payload object for screen-opening context that should stay local to the owning player UI.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgUIScreenPayload : public UObject
{
	GENERATED_BODY()

public:
	/** Optional semantic screen id for debugging or generic Blueprint handling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	FGameplayTag ScreenTag;
};

/**
 * Payload used by inventory-like screens to bind one or two inventory manager components.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryScreenPayload : public URpgUIScreenPayload
{
	GENERATED_BODY()

public:
	/** Main inventory shown by the screen, usually the player inventory. UI reads it but does not own it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<URpgInventoryManagerComponent> PrimaryInventory = nullptr;

	/** Optional second inventory for storage, loot, crafting station, or base armory views. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<URpgInventoryManagerComponent> SecondaryInventory = nullptr;

	/** Optional actor that caused the screen to open, such as a chest, corpse, or crafting station. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<AActor> ContextActor = nullptr;

	/** Optional component context for interactions that are component-owned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UActorComponent> ContextComponent = nullptr;
};

/**
 * Payload used by base terminal and storage-unit screens to bind shared base resources and armory state.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgBaseStorageScreenPayload : public URpgInventoryScreenPayload
{
	GENERATED_BODY()

public:
	/** Player inventory shown for deposits, withdrawals, and upgrade material context. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	/** Shared material-count storage pool owned by the linked base camp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgBaseStorageComponent> BaseStorage = nullptr;

	/** Shared inventory for instance-based base storage such as weapons, armor, and tools. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgInventoryManagerComponent> ArmoryInventory = nullptr;

	/** Owner-relevant private inventory for this player's locker at the selected base. Never used by shared crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgInventoryManagerComponent> PersonalInventory = nullptr;

	/** Shared concrete inventory for unstable and stabilized Rift objects. Null while the selected access point has no linked base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgInventoryManagerComponent> RiftInventory = nullptr;

	/** Station or terminal that opened the screen and is used for server-side access validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TObjectPtr<URpgBaseStorageStationComponent> StationComponent = nullptr;

	/** Empty means full terminal access; otherwise the UI should show only these material definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Storage")
	TArray<TSubclassOf<URpgInventoryItemDefinition>> AllowedResources;
};

/**
 * Payload used by crafting station screens to bind recipe state, player context, and output inventory.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgCraftingStationScreenPayload : public URpgInventoryScreenPayload
{
	GENERATED_BODY()

public:
	/** Player inventory used for ingredient availability and inventory-side transfers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	/** Station that owns recipes, queue state, pause state, and output handling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<URpgCraftingStationComponent> CraftingStation = nullptr;

	/** Station output inventory shown beside active jobs and queue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<URpgInventoryManagerComponent> OutputInventory = nullptr;

	/** Local pawn or actor used by the ViewModel for access and resource availability checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<AActor> RequestingActor = nullptr;
};
