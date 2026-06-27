#pragma once

#include "Components/ControllerComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgStarterInventoryComponent.generated.h"

class ARpgPlayerController;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgEquipmentLoadoutComponent;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStarterInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starter Inventory", meta = (AssetBundles = "Server"))
	TSoftClassPtr<URpgInventoryItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starter Inventory", meta = (ClampMin = "1", UIMin = "1"))
	int32 StackCount = 1;

	/** If true, the granted item is assigned to EquipmentSlot after being added to the player's inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starter Inventory")
	bool bAssignToEquipment = true;

	/** Equipment slot used when bAssignToEquipment is true. MainHand, OffHand, and armor slots are supported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Starter Inventory", meta = (EditCondition = "bAssignToEquipment"))
	ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::MainHand;
};

/**
 * GameFeature-addable starter inventory grant.
 *
 * The component is intentionally generic: feature assets decide which item
 * definitions are granted, while runtime ownership still flows through
 * Inventory -> EquipmentLoadout -> runtime EquipmentManager.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgStarterInventoryComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgStarterInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Inventory")
	TArray<FRpgStarterInventoryEntry> StarterInventory;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Inventory")
	bool bGrantOnlyIfMissing = true;

	/** If true, starter equipment waits until the possessed pawn has an equipment manager ready to apply the loadout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Inventory")
	bool bWaitForPawnBeforeAssigningEquipment = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Inventory", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float RetryInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Inventory", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxGrantAttempts = 40;

private:
	void TryGrantStarterInventory();
	void ScheduleRetry();
	bool ShouldWaitForPawn(const ARpgPlayerController* PlayerController) const;
	static bool EquipmentLoadoutContainsItem(const URpgEquipmentLoadoutComponent* EquipmentLoadout, const URpgInventoryItemInstance* ItemInstance);

	UPROPERTY(Transient)
	bool bHasTriedGrant = false;

	UPROPERTY(Transient)
	int32 GrantAttempts = 0;

	FTimerHandle RetryTimerHandle;
};
