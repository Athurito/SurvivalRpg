#include "RpgWorldSaveGame.h"

#include "Engine/AssetManager.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWorldSaveGame)

namespace RpgWorldSaveGame
{
bool ValidateGraphEnvelope(
	const FRpgInventoryGraphSaveData& Graph,
	TSet<FRpgInventoryItemId>& InOutGlobalItemIds,
	TSet<FRpgInventoryItemId>* OutGraphItemIds,
	FString& OutError)
{
	if (Graph.SchemaVersion != FRpgInventoryGraphSaveData::CurrentSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported inventory graph schema %d."), Graph.SchemaVersion);
		return false;
	}

	TSet<FRpgInventoryItemId> GraphItemIds;
	for (const FRpgInventorySavedItem& Item : Graph.Items)
	{
		if (!Item.ItemId.IsValid() ||
			GraphItemIds.Contains(Item.ItemId) ||
			InOutGlobalItemIds.Contains(Item.ItemId))
		{
			OutError = TEXT("Inventory graph contains an invalid, locally duplicate, or save-globally reused persistent item id.");
			return false;
		}
		if (Item.ItemDefinition.IsNull() || Item.StackCount <= 0 ||
			!Item.Container.IsValid() ||
			!Item.Placement.GetContainerHandle().IsValid() ||
			Item.Placement.GetContainerHandle() != Item.Container ||
			Item.Placement.X < 0 || Item.Placement.Y < 0)
		{
			OutError = TEXT("Inventory graph contains an invalid definition, stack, container handle, or placement.");
			return false;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
			Item.ItemDefinition.LoadSynchronous();
		if (!ItemDefinition || Item.StackCount >
			URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(ItemDefinition) ||
			!URpgInventoryItemInstance::ValidatePersistedRuntimeState(
				ItemDefinition,
				Item.RuntimeState))
		{
			OutError = TEXT("Inventory graph references a missing item definition, exceeds its current stack limit, or contains invalid fragment runtime state.");
			return false;
		}

		GraphItemIds.Add(Item.ItemId);
		InOutGlobalItemIds.Add(Item.ItemId);
	}

	for (const FRpgInventorySavedItem& Item : Graph.Items)
	{
		if (Item.Container.ItemOwnerId.IsValid() &&
			(!GraphItemIds.Contains(Item.Container.ItemOwnerId) ||
			Item.Container.ItemOwnerId == Item.ItemId))
		{
			OutError = TEXT("Inventory graph contains an orphaned or self-owned nested container handle.");
			return false;
		}
	}

	if (OutGraphItemIds)
	{
		*OutGraphItemIds = MoveTemp(GraphItemIds);
	}

	return true;
}

URpgBaseStorageUpgradeDefinition* ResolveUpgradeDefinition(
	const FPrimaryAssetId& UpgradeId)
{
	if (!UpgradeId.IsValid())
	{
		return nullptr;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	if (URpgBaseStorageUpgradeDefinition* Loaded =
			Cast<URpgBaseStorageUpgradeDefinition>(
				AssetManager.GetPrimaryAssetObject(UpgradeId)))
	{
		return Loaded;
	}
	const FSoftObjectPath Path =
		AssetManager.GetPrimaryAssetPath(UpgradeId);
	return Path.IsValid()
		? Cast<URpgBaseStorageUpgradeDefinition>(Path.TryLoad())
		: nullptr;
}
}

bool URpgWorldSaveGame::ValidateForLoad(FString& OutError) const
{
	OutError.Reset();
	if (SchemaVersion < MinimumSupportedSchemaVersion ||
		SchemaVersion > CurrentSchemaVersion)
	{
		OutError = FString::Printf(TEXT("Unsupported world-save schema %d."), SchemaVersion);
		return false;
	}
	if (SaveSequence < 0 || SaveSequence == MAX_int64)
	{
		OutError = TEXT("Save sequence must leave room for the next monotone snapshot id.");
		return false;
	}

	TSet<FRpgInventoryItemId> GlobalItemIds;
	for (const TPair<FString, FRpgPlayerSaveData>& Pair : Players)
	{
		if (Pair.Key.IsEmpty() || !Pair.Value.IsSchemaSupported())
		{
			OutError = FString::Printf(TEXT("Player profile '%s' has an invalid save envelope."), *Pair.Key);
			return false;
		}
		if (SchemaVersion >= 2 && !Pair.Value.bHasInventoryGraph)
		{
			OutError = FString::Printf(
				TEXT("Player profile '%s' is missing its required V2 inventory graph."),
				*Pair.Key);
			return false;
		}
		if (Pair.Value.bHasInventoryGraph &&
			!RpgWorldSaveGame::ValidateGraphEnvelope(
				Pair.Value.InventoryGraph,
				GlobalItemIds,
				nullptr,
				OutError))
		{
			OutError = FString::Printf(TEXT("Player profile '%s': %s"), *Pair.Key, *OutError);
			return false;
		}
	}

	for (const TPair<FName, FRpgWorldContainerSaveData>& Pair : WorldContainers)
	{
		if (Pair.Key.IsNone() || Pair.Value.PersistentContainerId != Pair.Key)
		{
			OutError = TEXT("World-container map contains a missing or mismatched persistent id.");
			return false;
		}
		if (!RpgWorldSaveGame::ValidateGraphEnvelope(
				Pair.Value.InventoryGraph,
				GlobalItemIds,
				nullptr,
				OutError))
		{
			OutError = FString::Printf(TEXT("World container '%s': %s"), *Pair.Key.ToString(), *OutError);
			return false;
		}
	}

	if (SchemaVersion < 2)
	{
		// V1 had no base-storage or storage-knowledge fields; default-empty values migrate safely.
		if (!BaseStorages.IsEmpty() || !StorageKnowledgeTags.IsEmpty())
		{
			OutError = TEXT("World-save schema V1 cannot claim V2 base-storage payloads.");
			return false;
		}
		return true;
	}

	FRpgWorldStorageKnowledgeSaveData KnowledgeSaveData;
	KnowledgeSaveData.KnowledgeTags = StorageKnowledgeTags;
	if (!URpgWorldStorageKnowledgeComponent::ValidateSaveData(
			KnowledgeSaveData, &OutError))
	{
		return false;
	}

	for (const TPair<FName, FRpgBaseStorageSaveData>& Pair : BaseStorages)
	{
		const FRpgBaseStorageSaveData& Base = Pair.Value;
		if (Pair.Key.IsNone() || Base.BaseId != Pair.Key ||
			Base.RiftStrain < 0 || Base.RiftStrain > 100)
		{
			OutError = TEXT("Base-storage map contains a missing/mismatched id or invalid Rift strain.");
			return false;
		}

		TSet<FSoftObjectPath> SeenBulkDefinitions;
		for (const FRpgBaseStorageBulkSaveEntry& Entry : Base.BulkEntries)
		{
			const FSoftObjectPath DefinitionPath = Entry.ItemDefinition.ToSoftObjectPath();
			const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
				Entry.ItemDefinition.LoadSynchronous();
			const URpgInventoryFragment_StorageProfile* StorageProfile =
				ItemDefinition
					? URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
						ItemDefinition)
					: nullptr;
			if (!DefinitionPath.IsValid() || Entry.Count <= 0 ||
				SeenBulkDefinitions.Contains(DefinitionPath) ||
				!ItemDefinition || !StorageProfile ||
				!StorageProfile->CanDepositAsBulk() ||
				StorageProfile->StorageDomainTag !=
					RpgGameplayTags::Storage_Domain_Materials)
			{
				OutError = FString::Printf(
					TEXT("Base storage '%s' contains an invalid or duplicate bulk row."),
					*Pair.Key.ToString());
				return false;
			}
			SeenBulkDefinitions.Add(DefinitionPath);
		}

		TSet<FPrimaryAssetId> SeenUpgradeIds;
		for (const FRpgBaseStorageInstalledUpgradeSaveData& Upgrade : Base.InstalledUpgrades)
		{
			const URpgBaseStorageUpgradeDefinition* Definition =
				RpgWorldSaveGame::ResolveUpgradeDefinition(
					Upgrade.UpgradeId);
			if (!Upgrade.UpgradeId.IsValid() || Upgrade.AnchorId.IsNone() ||
				SeenUpgradeIds.Contains(Upgrade.UpgradeId) || !Definition ||
				!Definition->TargetDomainTag.IsValid() ||
				Definition->TargetAnchorId != Upgrade.AnchorId)
			{
				OutError = FString::Printf(
					TEXT("Base storage '%s' contains an invalid, duplicate, or untargeted upgrade."),
					*Pair.Key.ToString());
				return false;
			}
			SeenUpgradeIds.Add(Upgrade.UpgradeId);
		}

		if (!Base.bHasArmoryGraph || !Base.bHasContainmentGraph)
		{
			OutError = FString::Printf(
				TEXT("Base storage '%s' is missing a required V2 Armory or Containment graph."),
				*Pair.Key.ToString());
			return false;
		}

		if (!RpgWorldSaveGame::ValidateGraphEnvelope(
				Base.ArmoryGraph,
				GlobalItemIds,
				nullptr,
				OutError))
		{
			OutError = FString::Printf(TEXT("Base storage '%s' armory: %s"), *Pair.Key.ToString(), *OutError);
			return false;
		}
		TSet<FRpgInventoryItemId> ContainmentItemIds;
		if (!RpgWorldSaveGame::ValidateGraphEnvelope(
				Base.ContainmentGraph,
				GlobalItemIds,
				&ContainmentItemIds,
				OutError))
		{
			OutError = FString::Printf(TEXT("Base storage '%s' containment: %s"), *Pair.Key.ToString(), *OutError);
			return false;
		}

		TSet<FRpgInventoryItemId> SeenContainmentStateIds;
		for (const FRpgBaseContainmentItemStateSaveData& State : Base.ContainmentStates)
		{
			if (!State.ItemId.IsValid() ||
				SeenContainmentStateIds.Contains(State.ItemId) ||
				!ContainmentItemIds.Contains(State.ItemId))
			{
				OutError = FString::Printf(
					TEXT("Base storage '%s' contains invalid or duplicate containment state."),
					*Pair.Key.ToString());
				return false;
			}
			SeenContainmentStateIds.Add(State.ItemId);
		}

		for (const TPair<FString, FRpgInventoryGraphSaveData>& Locker : Base.PersonalLockerGraphs)
		{
			if (Locker.Key.IsEmpty() ||
				!RpgWorldSaveGame::ValidateGraphEnvelope(
					Locker.Value,
					GlobalItemIds,
					nullptr,
					OutError))
			{
				OutError = FString::Printf(
					TEXT("Base storage '%s' personal locker '%s': %s"),
					*Pair.Key.ToString(), *Locker.Key, *OutError);
				return false;
			}
		}
	}

	return true;
}
