#pragma once

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"

#include "RpgUIScreenPayload.generated.h"

class AActor;
class UActorComponent;
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
