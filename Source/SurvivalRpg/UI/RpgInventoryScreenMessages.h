#pragma once

#include "CoreMinimal.h"

#include "RpgInventoryScreenMessages.generated.h"

class APlayerController;

/**
 * Local gameplay message on Rpg.Inventory.Message.ScreenActivation, sent when an inventory, storage or crafting
 * screen opens or closes. HUD read models use it to tell item moves made in a screen from gains in the world.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryScreenActivationMessage
{
	GENERATED_BODY()

	/** Local player whose screen changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI")
	TObjectPtr<APlayerController> OwningPlayer = nullptr;

	/** True when the screen opened, false when it closed. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|UI")
	bool bActive = false;
};
