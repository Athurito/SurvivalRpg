#pragma once

#include "Components/ActorComponent.h"

#include "RpgCraftingStationComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class ARpgBaseCampActor;
class URpgBaseStorageStationComponent;
class URpgBaseStorageComponent;

/** Resource source order used by a crafting station when a recipe consumes materials. */
UENUM(BlueprintType)
enum class ERpgCraftingResourceConsumeOrder : uint8
{
	/** Consume from linked base storage first, then player/allowed inventory sources. */
	BaseThenPlayer,

	/** Consume from player/allowed inventory sources first, then linked base storage. */
	PlayerThenBase,

	/** Only consume from linked base storage. */
	BaseOnly,

	/** Only consume from player/allowed inventory sources. */
	PlayerOnly
};

/** One resource requirement consumed by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingResourceCost
{
	GENERATED_BODY()

	/** Item definition required by the recipe. Static recipe data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items to consume across player inventory and linked storage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** One item stack created by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingOutputItem
{
	GENERATED_BODY()

	/** Item definition produced by the recipe. Instance data is created through the inventory manager. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items produced. Stackable definitions may merge; equipment definitions create entries as needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/**
 * V1 crafting station helper that consumes resources and stores outputs in a replicated inventory.
 */
UCLASS(Blueprintable, ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCraftingStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/** Returns player inventory plus crafting-accessible containers in range or in the same storage group. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	TArray<URpgInventoryManagerComponent*> GetResourceInventories(AActor* RequestingActor) const;

	/** Returns total available count across all resource inventories for one item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	int32 GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Consumes resources across player inventory and nearby/same-group storage after verifying the full cost is available. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems);

	/** Returns true when the requesting actor may use this station's output inventory. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Replicated inventory where crafted outputs wait when they are not auto-deposited into the base. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	URpgInventoryManagerComponent* GetOutputInventory() const { return OutputInventoryComponent; }

	/** Assigns the output inventory component, usually from a native or Blueprint crafting-station actor constructor. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Output")
	void SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory);

	/** Returns true when every output can either auto-deposit or fit into the output inventory. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool CanAcceptCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems) const;

	/** Adds already-crafted outputs to base storage/armory and the output inventory. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool AddCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Convenience V1 craft path: verifies output room, consumes costs, then stores outputs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Attempts to move current output inventory contents into linked base storage/armory. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool FlushOutputToBaseStorage();

	/** Returns true when this station should push crafted outputs into the linked base before using output slots. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool ShouldAutoDepositCraftingOutputs() const;

protected:
	/** Shared storage group this station belongs to. Empty means radius-only shared-container lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName StorageGroupId;

	/** Whether old shared containers in range/storage group are included as recipe input sources. Disabled by default for V1 basislager flow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	bool bUseNearbyCraftingContainers = false;

	/** Radius in centimeters for including nearby shared containers as crafting resource sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (EditCondition = "bUseNearbyCraftingContainers", ClampMin = "0", UIMin = "0", Units = "cm"))
	float StorageSearchRadius = 1200.0f;

	/** Maximum direct distance in centimeters for taking outputs from this station. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Optional base camp resource pool this station may pull material counts from. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Whether recipe checks and consumption include the linked base camp's material-count pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	bool bUseLinkedBaseStorage = true;

	/** Resource source order used by recipe cost checks and consumption. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	ERpgCraftingResourceConsumeOrder ResourceConsumeOrder = ERpgCraftingResourceConsumeOrder::BaseThenPlayer;

	/** Station/terminal whose installed upgrades unlock output auto-deposit for this crafting station. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgBaseStorageStationComponent> OutputAutoDepositUpgradeProvider;

	/** Debug/prototype override that enables auto-deposit without requiring the upgrade provider tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAlwaysAutoDepositCraftingOutputs = false;

	/** Whether instance-based outputs may go directly to the linked base armory when auto-deposit is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAutoDepositInstanceOutputsToArmory = true;

	/** Output inventory used when auto-deposit is disabled or linked storage is full. Usually a fixed 4-slot component. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgInventoryManagerComponent> OutputInventoryComponent;

	/** Fixed slot count configured on output inventories assigned through SetOutputInventoryManager. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output", meta = (ClampMin = "0", UIMin = "0"))
	int32 OutputSlotCount = 4;

private:
	URpgBaseStorageComponent* GetLinkedBaseStorage() const;
	URpgInventoryManagerComponent* GetLinkedArmoryInventory() const;
	int32 GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const;
	bool ConsumeInventoryResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const;
	bool ConsumeBaseResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;
};
