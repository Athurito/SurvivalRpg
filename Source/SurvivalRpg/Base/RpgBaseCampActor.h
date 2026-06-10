#pragma once

#include "GameFramework/Actor.h"

#include "RpgBaseCampActor.generated.h"

class URpgBaseStorageComponent;
class URpgInventoryManagerComponent;
class USceneComponent;

/**
 * Replicated authority actor for one player/base storage hub.
 *
 * Resource materials are stored as counts in BaseStorageComponent. Instance-based gear, weapons,
 * shields, and durability-bearing items remain item instances in ArmoryInventoryComponent.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgBaseCampActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgBaseCampActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Shared resource pool for material counts owned by this base camp. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp")
	URpgBaseStorageComponent* GetBaseStorageComponent() const { return BaseStorageComponent; }

	/** Inventory for instance-based base storage such as weapons, shields, armor, and tools. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp")
	URpgInventoryManagerComponent* GetArmoryInventoryComponent() const { return ArmoryInventoryComponent; }

protected:
	/** Simple replicated actor root so Blueprint children can attach base visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Server-authoritative material pool shared by linked stations and crafting stations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<URpgBaseStorageComponent> BaseStorageComponent;

	/** Server-authoritative instance inventory for stored equipment and non-material items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<URpgInventoryManagerComponent> ArmoryInventoryComponent;
};
