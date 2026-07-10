#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgPlayerSaveData.generated.h"

/**
 * Versioned host-owned persistence payload for one player profile.
 *
 * Runtime UObject pointers are deliberately excluded. The inventory graph is restored first so quick-access and
 * equipment selections can resolve their persistent item ids against newly reconstructed runtime instances.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPlayerSaveData
{
	GENERATED_BODY()

	/** Current per-player schema emitted by this build. */
	static constexpr int32 CurrentSchemaVersion = 1;

	/** Selects the migration/validation path before any runtime player state is changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save", meta = (ClampMin = "1", UIMin = "1"))
	int32 SchemaVersion = CurrentSchemaVersion;

	/** Last server-authored checkpoint used for respawn after reconnect or host restart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	FTransform CheckpointTransform = FTransform::Identity;

	/** Whether CheckpointTransform contains an authored checkpoint rather than the map's default spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	bool bHasCheckpoint = false;

	/** True only when InventoryGraph contains a captured authoritative profile inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	bool bHasInventoryGraph = false;

	/** Complete flattened inventory graph; import is validated and committed atomically by the inventory manager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	FRpgInventoryGraphSaveData InventoryGraph;

	/** Shared keyboard/radial bindings. Successful captures always store exactly eight pointer-free entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	TArray<FRpgQuickAccessBinding> QuickAccessBindings;

	/** Active hand ids and remembered shield pairings resolved only after InventoryGraph imports successfully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Rpg|Save")
	FRpgEquipmentSelectionSaveData EquipmentSelection;

	/** Lightweight envelope validation; the inventory manager performs the authoritative deep graph validation. */
	bool IsSchemaSupported() const
	{
		return SchemaVersion == CurrentSchemaVersion &&
			(!bHasInventoryGraph || InventoryGraph.SchemaVersion == FRpgInventoryGraphSaveData::CurrentSchemaVersion) &&
			(QuickAccessBindings.IsEmpty() || QuickAccessBindings.Num() == 8);
	}
};
