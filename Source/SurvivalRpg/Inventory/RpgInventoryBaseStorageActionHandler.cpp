#include "RpgInventoryUiActionDomainHandlers.h"

#include "Engine/AssetManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "NativeGameplayTags.h"
#include "RpgInventoryFragment_ContainmentProfile.h"
#include "RpgInventoryFragment_StorageProfile.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_SmartDeposit,
	"Rpg.BaseStorage.Action.SmartDeposit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_Deposit,
	"Rpg.BaseStorage.Action.Deposit");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_Withdraw,
	"Rpg.BaseStorage.Action.Withdraw");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_InstallUpgrade,
	"Rpg.BaseStorage.Action.InstallUpgrade");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
	"Rpg.BaseStorage.Action.DecommissionUpgrade");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_Stabilize,
	"Rpg.BaseStorage.Action.Stabilize");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_Extract,
	"Rpg.BaseStorage.Action.Extract");
UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_Rpg_BaseStorage_Action_Cleanse,
	"Rpg.BaseStorage.Action.Cleanse");

namespace
{
	enum class ERpgStorageCommandHashSalt : uint32
	{
		SmartDeposit = 0x11A5A001u,
		Deposit = 0x11A5A002u,
		Withdraw = 0x11A5A003u,
		Install = 0x11A5A004u,
		Decommission = 0x11A5A005u,
		Stabilize = 0x11A5A006u,
		Extract = 0x11A5A007u,
		Cleanse = 0x11A5A008u
	};

	uint32 HashStorageContext(
		const FRpgBaseStorageRequestContext& Context,
		ERpgStorageCommandHashSalt Salt)
	{
		uint32 Hash = GetTypeHash(static_cast<uint32>(Salt));
		Hash = HashCombine(Hash, GetTypeHash(Context.BaseId));
		return HashCombine(
			Hash,
			GetTypeHash(Context.ExpectedNetworkRevision));
	}

	uint32 HashPlacement(
		uint32 Seed,
		const FRpgInventoryGridPlacement& Placement)
	{
		Seed = HashCombine(Seed, GetTypeHash(Placement.ContainerHandle));
		Seed = HashCombine(Seed, GetTypeHash(Placement.X));
		Seed = HashCombine(Seed, GetTypeHash(Placement.Y));
		Seed = HashCombine(Seed, GetTypeHash(Placement.Width));
		Seed = HashCombine(Seed, GetTypeHash(Placement.Height));
		return HashCombine(Seed, GetTypeHash(Placement.bRotated));
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageSmartDepositRequest& Request)
	{
		return HashStorageContext(
			Request.Context,
			ERpgStorageCommandHashSalt::SmartDeposit);
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageDepositRequest& Request)
	{
		uint32 Hash = HashStorageContext(
			Request.Context,
			ERpgStorageCommandHashSalt::Deposit);
		Hash = HashCombine(Hash, GetTypeHash(Request.ItemId));
		Hash = HashCombine(Hash, GetTypeHash(Request.ExpectedEntryId));
		Hash = HashPlacement(Hash, Request.ExpectedSourcePlacement);
		Hash = HashCombine(Hash, GetTypeHash(Request.ExpectedInventoryRevision));
		Hash = HashCombine(Hash, GetTypeHash(Request.ExpectedSourceQuantity));
		return HashCombine(Hash, GetTypeHash(Request.RequestedCount));
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageWithdrawRequest& Request)
	{
		uint32 Hash = HashStorageContext(
			Request.Context,
			ERpgStorageCommandHashSalt::Withdraw);
		Hash = HashCombine(
			Hash,
			GetTypeHash(Request.ItemDefinition.Get()));
		return HashCombine(Hash, GetTypeHash(Request.RequestedCount));
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageUpgradeRequest& Request,
		ERpgStorageCommandHashSalt Salt)
	{
		uint32 Hash = HashStorageContext(Request.Context, Salt);
		Hash = HashCombine(Hash, GetTypeHash(Request.UpgradeId));
		return HashCombine(Hash, GetTypeHash(Request.ExpectedAnchorId));
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageRiftItemRequest& Request,
		ERpgStorageCommandHashSalt Salt)
	{
		uint32 Hash = HashStorageContext(Request.Context, Salt);
		Hash = HashCombine(Hash, GetTypeHash(Request.ItemId));
		Hash = HashCombine(
			Hash,
			GetTypeHash(Request.ExpectedContainmentRevision));
		Hash = HashCombine(
			Hash,
			GetTypeHash(Request.bExpectedStabilized));
		return HashCombine(Hash, GetTypeHash(Request.bConfirmed));
	}

	uint32 GetStoragePayloadHash(
		const FRpgBaseStorageCleanseRequest& Request)
	{
		return HashStorageContext(
			Request.Context,
			ERpgStorageCommandHashSalt::Cleanse);
	}

	FRpgBaseStorageCommandResult MakeRejectedStorageResult(
		const FRpgBaseStorageRequestContext& Context,
		ERpgBaseStorageResultCode Code,
		int32 RequestedCount = 0)
	{
		FRpgBaseStorageCommandResult Result;
		Result.RequestId = Context.RequestId;
		Result.BaseId = Context.BaseId;
		Result.Code = Code;
		Result.RequestedCount = RequestedCount;
		return Result;
	}

	ARpgBaseCampActor* ResolveUniqueBaseCamp(
		UWorld* World,
		FName BaseId)
	{
		if (!World || BaseId.IsNone())
		{
			return nullptr;
		}

		ARpgBaseCampActor* Match = nullptr;
		for (TActorIterator<ARpgBaseCampActor> It(World); It; ++It)
		{
			ARpgBaseCampActor* Candidate = *It;
			if (!IsValid(Candidate) || Candidate->GetBaseId() != BaseId)
			{
				continue;
			}
			if (Match)
			{
				UE_LOG(
					LogRpgInventoryUiActions,
					Error,
					TEXT("Storage command rejected because BaseId '%s' is not unique."),
					*BaseId.ToString());
				return nullptr;
			}
			Match = Candidate;
		}
		return Match;
	}

	bool IsStationCompatibleWithUpgrade(
		const URpgBaseStorageStationComponent* Station,
		const URpgBaseStorageUpgradeDefinition* Upgrade)
	{
		return Station && Upgrade &&
			(Upgrade->AllowedStationTags.IsEmpty() ||
			 Station->GetStationTags().HasAny(Upgrade->AllowedStationTags));
	}

	URpgBaseStorageStationComponent* FindAccessibleBaseStation(
		ARpgBaseCampActor* BaseCamp,
		const AActor* RequestingActor,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = nullptr,
		const URpgBaseStorageUpgradeDefinition* Upgrade = nullptr)
	{
		if (!BaseCamp || !RequestingActor)
		{
			return nullptr;
		}

		TArray<URpgBaseStorageStationComponent*> Candidates;
		for (URpgBaseStorageStationComponent* Station :
			BaseCamp->GetStorageStations())
		{
			if (!IsValid(Station) || !Station->CanActorAccess(RequestingActor) ||
				(ItemDefinition &&
				 !Station->AllowsResourceDefinition(ItemDefinition)) ||
				(Upgrade &&
				 !IsStationCompatibleWithUpgrade(Station, Upgrade)))
			{
				continue;
			}
			Candidates.Add(Station);
		}
		Candidates.Sort(
			[](const URpgBaseStorageStationComponent& A,
			   const URpgBaseStorageStationComponent& B)
			{
				return A.GetPathName() < B.GetPathName();
			});
		return Candidates.IsEmpty() ? nullptr : Candidates[0];
	}

	const FRpgInventoryEntryView* FindEntryByItemId(
		const TArray<FRpgInventoryEntryView>& Entries,
		FRpgInventoryItemId ItemId)
	{
		return Entries.FindByPredicate(
			[ItemId](const FRpgInventoryEntryView& Entry)
			{
				return Entry.ItemId == ItemId;
			});
	}

	URpgBaseStorageUpgradeDefinition* ResolveStorageUpgrade(
		const FPrimaryAssetId& UpgradeId)
	{
		if (!UpgradeId.IsValid())
		{
			return nullptr;
		}

		UAssetManager& AssetManager = UAssetManager::Get();
		URpgBaseStorageUpgradeDefinition* Upgrade =
			Cast<URpgBaseStorageUpgradeDefinition>(
				AssetManager.GetPrimaryAssetObject(UpgradeId));
		if (!Upgrade)
		{
			const FSoftObjectPath AssetPath =
				AssetManager.GetPrimaryAssetPath(UpgradeId);
			Upgrade = AssetPath.IsValid()
				? Cast<URpgBaseStorageUpgradeDefinition>(
					AssetPath.TryLoad())
				: nullptr;
		}
		return Upgrade && Upgrade->GetPrimaryAssetId() == UpgradeId
			? Upgrade
			: nullptr;
	}

	bool CaptureInventoryCheckpoint(
		const URpgInventoryManagerComponent* Inventory,
		FRpgInventoryGraphSaveData& OutCheckpoint)
	{
		if (!Inventory)
		{
			return false;
		}
		OutCheckpoint = Inventory->ExportInventoryGraph();
		return OutCheckpoint.Items.Num() ==
			Inventory->GetAllEntries().Num();
	}

	bool RestoreInventoryCheckpoint(
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryGraphSaveData& Checkpoint)
	{
		FRpgInventoryMutationResult Result;
		return Inventory &&
			Inventory->RestoreRuntimeCheckpoint(Checkpoint, Result) &&
			Result.IsSuccess();
	}

	template <typename CostType>
	bool AggregateBulkCosts(
		const TArray<CostType>& Costs,
		const URpgBaseStorageComponent* BaseStorage,
		TMap<TSubclassOf<URpgInventoryItemDefinition>, int32>& OutCosts)
	{
		OutCosts.Reset();
		for (const CostType& Cost : Costs)
		{
			if (!Cost.ItemDefinition || Cost.Count <= 0 || !BaseStorage ||
				BaseStorage->GetBulkCapacityCost(Cost.ItemDefinition) <= 0)
			{
				return false;
			}
			const int64 NewCount =
				static_cast<int64>(OutCosts.FindRef(Cost.ItemDefinition)) +
				Cost.Count;
			if (NewCount > MAX_int32)
			{
				return false;
			}
			OutCosts.Add(Cost.ItemDefinition, static_cast<int32>(NewCount));
		}
		return true;
	}

	using FBaseResourceCheckpointMap = TMap<
		TSubclassOf<URpgInventoryItemDefinition>,
		FRpgBaseResourceMutationCheckpoint>;

	bool CaptureBaseResourceCheckpoints(
		URpgBaseStorageComponent* BaseStorage,
		const TMap<TSubclassOf<URpgInventoryItemDefinition>, int32>& Amounts,
		FBaseResourceCheckpointMap& OutCheckpoints)
	{
		OutCheckpoints.Reset();
		if (!BaseStorage)
		{
			return false;
		}
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Amount :
			Amounts)
		{
			FRpgBaseResourceMutationCheckpoint Checkpoint;
			if (!Amount.Key || Amount.Value <= 0 ||
				!BaseStorage->CaptureResourceMutationCheckpoint(
					Amount.Key,
					Checkpoint))
			{
				OutCheckpoints.Reset();
				return false;
			}
			OutCheckpoints.Add(Amount.Key, MoveTemp(Checkpoint));
		}
		return true;
	}

	bool RestoreBaseResourceCheckpoints(
		URpgBaseStorageComponent* BaseStorage,
		const FBaseResourceCheckpointMap& Checkpoints)
	{
		if (!BaseStorage)
		{
			return false;
		}
		bool bRestored = true;
		for (const TPair<
				TSubclassOf<URpgInventoryItemDefinition>,
				FRpgBaseResourceMutationCheckpoint>& Pair : Checkpoints)
		{
			bRestored = BaseStorage->RestoreResourceMutationCheckpoint(
				Pair.Value) && bRestored;
		}
		return bRestored;
	}

	bool ConsumeBaseResourceCosts(
		URpgBaseStorageComponent* BaseStorage,
		const TMap<TSubclassOf<URpgInventoryItemDefinition>, int32>& Costs,
		FBaseResourceCheckpointMap& OutCheckpoints)
	{
		OutCheckpoints.Reset();
		if (!BaseStorage)
		{
			return false;
		}
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
			Costs)
		{
			if (BaseStorage->GetResourceCount(Cost.Key) < Cost.Value)
			{
				return false;
			}
		}
		if (!CaptureBaseResourceCheckpoints(
				BaseStorage,
				Costs,
				OutCheckpoints))
		{
			return false;
		}
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
			Costs)
		{
			if (!BaseStorage->WithdrawResource(Cost.Key, Cost.Value))
			{
				return false;
			}
		}
		return true;
	}

	TArray<FRpgBaseStorageResourceCommandOutcome>
	BuildDefinitionResourceOutcomes(
		const TMap<TSubclassOf<URpgInventoryItemDefinition>, int32>& Amounts,
		ERpgBaseStorageResultCode Code,
		bool bApplied)
	{
		TArray<TSubclassOf<URpgInventoryItemDefinition>> Definitions;
		Amounts.GetKeys(Definitions);
		Definitions.Sort(
			[](const TSubclassOf<URpgInventoryItemDefinition>& A,
			   const TSubclassOf<URpgInventoryItemDefinition>& B)
			{
				return GetPathNameSafe(A.Get()) <
					GetPathNameSafe(B.Get());
			});

		TArray<FRpgBaseStorageResourceCommandOutcome> Outcomes;
		Outcomes.Reserve(Definitions.Num());
		for (const TSubclassOf<URpgInventoryItemDefinition> Definition :
			Definitions)
		{
			FRpgBaseStorageResourceCommandOutcome& Outcome =
				Outcomes.AddDefaulted_GetRef();
			Outcome.ItemDefinition = Definition;
			Outcome.RequestedCount = Amounts.FindRef(Definition);
			Outcome.AppliedCount = bApplied
				? Outcome.RequestedCount
				: 0;
			Outcome.Code = Code;
		}
		return Outcomes;
	}
	bool IsMaterialItem(const URpgInventoryItemInstance* Item)
	{
		const URpgInventoryFragment_StorageProfile* Profile =
			Item
				? URpgInventoryFragment_StorageProfile::
					ResolveStorageProfile(Item->GetItemDef())
				: nullptr;
		return Item && Item->CanCollapseIntoDefinitionCount() &&
			Profile && Profile->CanDepositAsBulk();
	}

	bool IsMaterialItemDefinition(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_StorageProfile* Profile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				ItemDefinition);
		return Profile && Profile->CanDepositAsBulk();
	}

	bool HasEquivalentRuntimeState(
		const TArray<FRpgInventoryFragmentStatePayload>& A,
		const TArray<FRpgInventoryFragmentStatePayload>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].FragmentId != B[Index].FragmentId ||
				A[Index].Version != B[Index].Version ||
				A[Index].Payload != B[Index].Payload)
			{
				return false;
			}
		}
		return true;
	}

	int32 GetAvailableUpgradeCostCount(
		const URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		int64 AvailableCount = 0;

		if (ConsumeOrder !=
				ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly &&
			PlayerInventory)
		{
			for (const FRpgInventoryEntryView& Entry :
				PlayerInventory->GetAllEntries())
			{
				if (Entry.Instance && Entry.Instance->GetItemDef() == ItemDefinition &&
					Entry.StackCount > 0 &&
					Entry.Instance->CanCollapseIntoDefinitionCount() &&
					BaseStorage &&
					BaseStorage->GetBulkCapacityCost(ItemDefinition) > 0)
				{
					AvailableCount = FMath::Min<int64>(
						MAX_int32,
						AvailableCount + Entry.StackCount);
				}
			}
		}

		if (ConsumeOrder !=
				ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly &&
			BaseStorage)
		{
			AvailableCount = FMath::Min<int64>(
				MAX_int32,
				AvailableCount +
					BaseStorage->GetResourceCount(ItemDefinition));
		}

		return static_cast<int32>(AvailableCount);
	}

	bool ConsumeUpgradeCostFromPlayer(
		URpgInventoryManagerComponent* PlayerInventory,
		const URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 CountToConsume)
	{
		if (CountToConsume <= 0)
		{
			return true;
		}
		if (!PlayerInventory || !BaseStorage ||
			BaseStorage->GetBulkCapacityCost(ItemDefinition) <= 0)
		{
			return false;
		}

		int32 Remaining = CountToConsume;
		const TArray<FRpgInventoryEntryView> Entries =
			PlayerInventory->GetAllEntries();
		for (const FRpgInventoryEntryView& Entry : Entries)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDefinition ||
				Entry.StackCount <= 0 ||
				!Entry.Instance->CanCollapseIntoDefinitionCount())
			{
				continue;
			}

			const int32 ConsumeCount = FMath::Min(Remaining, Entry.StackCount);
			const FRpgInventoryMutationResult Result =
				PlayerInventory->ConsumeItemById(Entry.ItemId, ConsumeCount);
			if (!Result.IsSuccess() || Result.AppliedQuantity != ConsumeCount)
			{
				return false;
			}
			Remaining -= ConsumeCount;
		}
		return Remaining == 0;
	}

	bool ConsumeUpgradeCostFromBase(
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 CountToConsume)
	{
		return CountToConsume <= 0 ||
			(BaseStorage &&
			 BaseStorage->WithdrawResource(
				 ItemDefinition,
				 CountToConsume));
	}

	bool ConsumeUpgradeCost(
		URpgInventoryManagerComponent* PlayerInventory,
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder)
	{
		if (!ItemDefinition || Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = Count;
		auto ConsumeFromBase = [&]()
		{
			const int32 AvailableInBase =
				BaseStorage
					? BaseStorage->GetResourceCount(
						ItemDefinition)
					: 0;
			const int32 CountToConsume =
				FMath::Min(AvailableInBase, RemainingCount);
			if (!ConsumeUpgradeCostFromBase(
					BaseStorage,
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer =
				PlayerInventory
					? PlayerInventory->
						GetTotalItemCountByDefinition(
							ItemDefinition)
					: 0;
			const int32 CountToConsume =
				FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(
					PlayerInventory,
					BaseStorage,
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::
			BaseThenPlayer:
			return ConsumeFromBase() &&
				ConsumeFromPlayer() &&
				RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::
			PlayerThenBase:
			return ConsumeFromPlayer() &&
				ConsumeFromBase() &&
				RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount <= 0;

		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount <= 0;
		}

		return false;
	}

	bool AdmitModernStorageCommand(
		URpgBaseStorageComponent* BaseStorage,
		APlayerController* RequestingController,
		const FRpgBaseStorageRequestContext& Context,
		uint32 PayloadHash,
		FRpgBaseStorageCommandResult& OutResult)
	{
		if (!BaseStorage ||
			!BaseStorage->AdmitCommand(
				Context,
				PayloadHash,
				RequestingController,
				OutResult))
		{
			return false;
		}
		if (Context.ExpectedNetworkRevision == INDEX_NONE)
		{
			OutResult = BaseStorage->CompleteCommand(
				Context,
				PayloadHash,
				ERpgBaseStorageResultCode::InvalidRequest);
			return false;
		}
		return true;
	}

	bool HasRequiredContainmentConfiguration(
		const URpgBaseStorageComponent* BaseStorage,
		const URpgInventoryFragment_ContainmentProfile* Profile)
	{
		return BaseStorage && Profile && Profile->IsStructurallyValid() &&
			BaseStorage->GetInstalledCapabilities().HasAllExact(
				Profile->RequiredContainmentCapabilityTags) &&
			BaseStorage->GetContainmentStrength() >=
				Profile->RequiredContainmentStrength &&
			BaseStorage->GetCorruptionProtection() >=
				Profile->RequiredCorruptionProtection;
	}

	bool CanAffordBaseResourceCosts(
		const URpgBaseStorageComponent* BaseStorage,
		const TMap<TSubclassOf<URpgInventoryItemDefinition>, int32>& Costs)
	{
		if (!BaseStorage)
		{
			return false;
		}
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
			Costs)
		{
			if (BaseStorage->GetResourceCount(Cost.Key) < Cost.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool ConsumeUpgradePaymentTracked(
		URpgInventoryManagerComponent* PlayerInventory,
		URpgBaseStorageComponent* BaseStorage,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 Count,
		ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder,
		int32& OutBaseConsumed)
	{
		OutBaseConsumed = 0;
		if (!PlayerInventory || !BaseStorage || !ItemDefinition || Count <= 0)
		{
			return false;
		}

		int32 RemainingCount = Count;
		auto ConsumeFromBase = [&]()
		{
			const int32 CountToConsume = FMath::Min(
				BaseStorage->GetResourceCount(ItemDefinition),
				RemainingCount);
			if (CountToConsume > 0 &&
				!BaseStorage->WithdrawResource(
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			OutBaseConsumed += CountToConsume;
			RemainingCount -= CountToConsume;
			return true;
		};
		auto ConsumeFromPlayer = [&]()
		{
			const int32 AvailableInPlayer =
				GetAvailableUpgradeCostCount(
					PlayerInventory,
					BaseStorage,
					ItemDefinition,
					ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly);
			const int32 CountToConsume =
				FMath::Min(AvailableInPlayer, RemainingCount);
			if (!ConsumeUpgradeCostFromPlayer(
					PlayerInventory,
					BaseStorage,
					ItemDefinition,
					CountToConsume))
			{
				return false;
			}
			RemainingCount -= CountToConsume;
			return true;
		};

		switch (ConsumeOrder)
		{
		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseThenPlayer:
			return ConsumeFromBase() && ConsumeFromPlayer() &&
				RemainingCount == 0;
		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerThenBase:
			return ConsumeFromPlayer() && ConsumeFromBase() &&
				RemainingCount == 0;
		case ERpgBaseStorageUpgradeCostConsumeOrder::BaseOnly:
			return ConsumeFromBase() && RemainingCount == 0;
		case ERpgBaseStorageUpgradeCostConsumeOrder::PlayerOnly:
			return ConsumeFromPlayer() && RemainingCount == 0;
		}
		return false;
	}
}

FRpgBaseStorageActionHandler::EMaterialDepositResult
FRpgBaseStorageActionHandler::TryDepositMaterialStack(
	URpgInventoryManagerComponent* Inventory,
	URpgBaseStorageComponent* BaseStorage,
	FRpgInventoryItemId ItemId,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 AvailableCount,
	int32 CountToStore) const
{
	if (!Inventory || !BaseStorage || !ItemId.IsValid() ||
		!ItemDefinition || AvailableCount <= 0 || CountToStore <= 0 ||
		CountToStore > AvailableCount)
	{
		return EMaterialDepositResult::Rejected;
	}

	const TArray<FRpgInventoryEntryView> EntriesBefore =
		Inventory->GetAllEntries();
	const FRpgInventoryEntryView* EntryBefore =
		EntriesBefore.FindByPredicate(
			[&ItemId](const FRpgInventoryEntryView& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	const FRpgInventoryGraphSaveData GraphBefore =
		Inventory->ExportInventoryGraph();
	const FRpgInventorySavedItem* SavedItemBefore =
		GraphBefore.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& Candidate)
			{
				return Candidate.ItemId == ItemId;
			});
	if (!EntryBefore || !EntryBefore->Instance ||
		EntryBefore->Instance->GetItemDef() != ItemDefinition ||
		EntryBefore->StackCount != AvailableCount ||
		GraphBefore.Items.Num() != EntriesBefore.Num() ||
		!SavedItemBefore)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material deposit skipped for %s because its exact pre-consume graph could not be exported."),
			*ItemId.ToString());
		return EMaterialDepositResult::Rejected;
	}
	if (!BaseStorage->CanStoreResourceInstance(
			EntryBefore->Instance,
			CountToStore))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Verbose,
			TEXT("Material deposit kept runtime-state item %s in concrete inventory because BaseStorage only represents stateless definition/count credits."),
			*ItemId.ToString());
		return EMaterialDepositResult::Rejected;
	}
	FRpgBaseResourceMutationCheckpoint BulkCheckpoint;
	if (!BaseStorage->CaptureResourceMutationCheckpoint(
			ItemDefinition,
			BulkCheckpoint))
	{
		return EMaterialDepositResult::Rejected;
	}

	const FRpgInventorySavedItem SavedItemSnapshot =
		*SavedItemBefore;
	URpgInventoryItemInstance* const InstanceBefore =
		EntryBefore->Instance;
	const FGuid EntryIdBefore = EntryBefore->EntryId;
	const FRpgInventoryGridPlacement PlacementBefore =
		EntryBefore->Placement;
	auto RestoreExactInventoryGraph = [&]()
	{
		FRpgInventoryMutationResult RollbackResult;
		if (!Inventory->RestoreRuntimeCheckpoint(
				GraphBefore,
				RollbackResult) ||
			!RollbackResult.IsSuccess())
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit exact graph rollback failed for %s after storing %d x %s was rejected (code %d)."),
				*ItemId.ToString(),
				CountToStore,
				*GetNameSafe(ItemDefinition),
				static_cast<int32>(RollbackResult.Code));
			return false;
		}

		const TArray<FRpgInventoryEntryView> RestoredEntries =
			Inventory->GetAllEntries();
		const FRpgInventoryEntryView* RestoredEntry =
			RestoredEntries.FindByPredicate(
				[&ItemId](
					const FRpgInventoryEntryView& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const FRpgInventoryGraphSaveData RestoredGraph =
			Inventory->ExportInventoryGraph();
		const FRpgInventorySavedItem* RestoredSavedItem =
			RestoredGraph.Items.FindByPredicate(
				[&ItemId](
					const FRpgInventorySavedItem& Candidate)
				{
					return Candidate.ItemId == ItemId;
				});
		const bool bExactRollback =
			RestoredEntry &&
			RestoredEntry->Instance == InstanceBefore &&
			RestoredEntry->EntryId == EntryIdBefore &&
			RestoredEntry->StackCount == AvailableCount &&
			RestoredEntry->Placement == PlacementBefore &&
			RestoredSavedItem &&
			RestoredSavedItem->ItemDefinition ==
				SavedItemSnapshot.ItemDefinition &&
			RestoredSavedItem->Container ==
				SavedItemSnapshot.Container &&
			RestoredSavedItem->Placement ==
				SavedItemSnapshot.Placement &&
			HasEquivalentRuntimeState(
				RestoredSavedItem->RuntimeState,
				SavedItemSnapshot.RuntimeState);
		if (!bExactRollback)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit rollback for %s completed with item identity, entry identity, placement, or runtime-state drift."),
				*ItemId.ToString());
		}
		return bExactRollback;
	};

	// Keep one concrete unit alive for a whole-stack deposit so a failed
	// storage write can restore the same UObject and replicated EntryId.
	const int32 CountToConsumeBeforeStore =
		CountToStore == AvailableCount
			? CountToStore - 1
			: CountToStore;
	if (CountToConsumeBeforeStore > 0)
	{
		const FRpgInventoryMutationResult ConsumeResult =
			Inventory->ConsumeItemById(
				ItemId,
				CountToConsumeBeforeStore);
		if (!ConsumeResult.IsSuccess() ||
			ConsumeResult.AppliedQuantity !=
				CountToConsumeBeforeStore)
		{
			if (ConsumeResult.AppliedQuantity > 0)
			{
				return RestoreExactInventoryGraph()
					? EMaterialDepositResult::RolledBack
					: EMaterialDepositResult::RollbackFailed;
			}
			return EMaterialDepositResult::Rejected;
		}
	}

	if (!BaseStorage->StoreResourceInstance(
			InstanceBefore,
			CountToStore))
	{
		const bool bInventoryRolledBack = RestoreExactInventoryGraph();
		const bool bStorageRolledBack =
			BaseStorage->RestoreResourceMutationCheckpoint(
				BulkCheckpoint);
		if (!bInventoryRolledBack || !bStorageRolledBack)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Material-deposit store rejection rollback failed for %s (Storage=%s, Inventory=%s)."),
				*ItemId.ToString(),
				bStorageRolledBack ? TEXT("restored") : TEXT("failed"),
				bInventoryRolledBack ? TEXT("restored") : TEXT("failed"));
		}
		return bInventoryRolledBack && bStorageRolledBack
			? EMaterialDepositResult::RolledBack
			: EMaterialDepositResult::RollbackFailed;
	}

	const int32 RemainingConsumeCount =
		CountToStore - CountToConsumeBeforeStore;
	if (RemainingConsumeCount <= 0)
	{
		return EMaterialDepositResult::Success;
	}

	const FRpgInventoryMutationResult FinalConsumeResult =
		Inventory->ConsumeItemById(
			ItemId,
			RemainingConsumeCount);
	if (FinalConsumeResult.IsSuccess() &&
		FinalConsumeResult.AppliedQuantity ==
			RemainingConsumeCount)
	{
		return EMaterialDepositResult::Success;
	}

	const bool bStorageRolledBack =
		BaseStorage->RestoreResourceMutationCheckpoint(BulkCheckpoint);
	const bool bInventoryRolledBack =
		RestoreExactInventoryGraph();
	if (!bStorageRolledBack || !bInventoryRolledBack)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Error,
			TEXT("Material-deposit finalization rollback failed for %s (%d x %s, Storage=%s, Inventory=%s)."),
			*ItemId.ToString(),
			CountToStore,
			*GetNameSafe(ItemDefinition),
			bStorageRolledBack
				? TEXT("restored")
				: TEXT("failed"),
			bInventoryRolledBack
				? TEXT("restored")
				: TEXT("failed"));
	}
	return bStorageRolledBack && bInventoryRolledBack
		? EMaterialDepositResult::RolledBack
		: EMaterialDepositResult::RollbackFailed;
}

void FRpgBaseStorageActionHandler::SmartDeposit(
	FRpgBaseStorageSmartDepositRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(Request);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_SmartDeposit,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest));
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_SmartDeposit,
			Result);
		return;
	}

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const AActor* RequestingActor = GetRequestingActor();
	if (!PlayerInventory ||
		!FindAccessibleBaseStation(BaseCamp, RequestingActor))
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			PlayerInventory
				? ERpgBaseStorageResultCode::NoAccess
				: ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_SmartDeposit,
			Result);
		return;
	}

	int32 RequestedCount = 0;
	int32 AppliedCount = 0;
	bool bAttemptedTransfer = false;
	bool bHadRolledBackConflict = false;
	bool bRollbackFailed = false;
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;
	const TArray<FRpgInventoryEntryView> Entries =
		PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0)
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
			Item->GetItemDef();
		FRpgBaseStorageResourceCommandOutcome Outcome;
		Outcome.ItemId = Entry.ItemId;
		Outcome.ItemDefinition = ItemDefinition;
		Outcome.RequestedCount = Entry.StackCount;
		const URpgInventoryFragment_StorageProfile* Profile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				ItemDefinition);
		if (!Item->CanCollapseIntoDefinitionCount() || !Profile ||
			!BaseStorage->CanAutoDepositBulk(ItemDefinition))
		{
			Outcome.Code = ERpgBaseStorageResultCode::UnsupportedMode;
			ResourceOutcomes.Add(MoveTemp(Outcome));
			continue;
		}
		if (!FindAccessibleBaseStation(
				BaseCamp,
				RequestingActor,
				ItemDefinition))
		{
			Outcome.Code = ERpgBaseStorageResultCode::NoAccess;
			ResourceOutcomes.Add(MoveTemp(Outcome));
			continue;
		}

		RequestedCount = static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(RequestedCount) + Entry.StackCount));
		const int32 CountToDeposit = FMath::Min(
			Entry.StackCount,
			BaseStorage->GetFreeResourceCapacity(ItemDefinition));
		if (CountToDeposit <= 0)
		{
			Outcome.Code = ERpgBaseStorageResultCode::CapacityFull;
			ResourceOutcomes.Add(MoveTemp(Outcome));
			continue;
		}
		if (!BaseStorage->CanStoreResourceInstance(Item, CountToDeposit))
		{
			Outcome.Code = ERpgBaseStorageResultCode::UnsupportedMode;
			ResourceOutcomes.Add(MoveTemp(Outcome));
			continue;
		}

		bAttemptedTransfer = true;
		const EMaterialDepositResult DepositResult =
			TryDepositMaterialStack(
				PlayerInventory,
				BaseStorage,
				Entry.ItemId,
				ItemDefinition,
				Entry.StackCount,
				CountToDeposit);
		if (DepositResult == EMaterialDepositResult::Success)
		{
			Outcome.AppliedCount = CountToDeposit;
			Outcome.Code = CountToDeposit == Entry.StackCount
				? ERpgBaseStorageResultCode::Success
				: ERpgBaseStorageResultCode::Partial;
			AppliedCount = static_cast<int32>(FMath::Min<int64>(
				MAX_int32,
				static_cast<int64>(AppliedCount) + CountToDeposit));
		}
		else
		{
			bHadRolledBackConflict =
				DepositResult == EMaterialDepositResult::RolledBack ||
				bHadRolledBackConflict;
			bRollbackFailed =
				DepositResult == EMaterialDepositResult::RollbackFailed;
			Outcome.Code = bRollbackFailed
				? ERpgBaseStorageResultCode::InternalRollback
				: ERpgBaseStorageResultCode::Conflict;
		}
		ResourceOutcomes.Add(MoveTemp(Outcome));
		if (bRollbackFailed)
		{
			break;
		}
	}

	ERpgBaseStorageResultCode Code =
		ERpgBaseStorageResultCode::UnsupportedMode;
	if (bRollbackFailed)
	{
		Code = ERpgBaseStorageResultCode::InternalRollback;
	}
	else if (RequestedCount > 0 && AppliedCount == RequestedCount)
	{
		Code = ERpgBaseStorageResultCode::Success;
	}
	else if (AppliedCount > 0)
	{
		Code = ERpgBaseStorageResultCode::Partial;
	}
	else if (RequestedCount > 0)
	{
		Code = bHadRolledBackConflict || bAttemptedTransfer
			? ERpgBaseStorageResultCode::Conflict
			: ERpgBaseStorageResultCode::CapacityFull;
	}

	Result = BaseStorage->CompleteCommand(
		Request.Context,
		PayloadHash,
		Code,
		RequestedCount,
		AppliedCount,
		ResourceOutcomes);
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_SmartDeposit,
		Result);
}

void FRpgBaseStorageActionHandler::DepositExact(
	FRpgBaseStorageDepositRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(Request);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				Request.RequestedCount),
			Request.ItemId);
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}

	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			Request.RequestedCount,
			Applied);
	};

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!PlayerInventory || !Request.ItemId.IsValid() ||
		!Request.ExpectedEntryId.IsValid() ||
		!Request.ExpectedSourcePlacement.IsValid() ||
		Request.ExpectedInventoryRevision == INDEX_NONE ||
		Request.ExpectedSourceQuantity <= 0 || Request.RequestedCount <= 0 ||
		Request.RequestedCount > Request.ExpectedSourceQuantity)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}
	if (!FindAccessibleBaseStation(BaseCamp, GetRequestingActor()))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}
	if (PlayerInventory->GetInventoryRevision() !=
		Request.ExpectedInventoryRevision)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		PlayerInventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry =
		FindEntryByItemId(Entries, Request.ItemId);
	if (!Entry || !Entry->Instance)
	{
		Complete(ERpgBaseStorageResultCode::MissingItem);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Entry->Instance->GetItemDef();
	if (Entry->EntryId != Request.ExpectedEntryId ||
		Entry->Placement != Request.ExpectedSourcePlacement ||
		Entry->StackCount != Request.ExpectedSourceQuantity)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	const URpgInventoryFragment_StorageProfile* Profile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			ItemDefinition);
	if (!Profile ||
		!BaseStorage->CanManuallyDepositBulk(ItemDefinition) ||
		!Entry->Instance->CanCollapseIntoDefinitionCount())
	{
		Complete(ERpgBaseStorageResultCode::UnsupportedMode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (!FindAccessibleBaseStation(
			BaseCamp,
			GetRequestingActor(),
			ItemDefinition))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId);
		return;
	}
	const int32 CountToDeposit = FMath::Min(
		Request.RequestedCount,
		BaseStorage->GetFreeResourceCapacity(ItemDefinition));
	if (CountToDeposit <= 0)
	{
		Complete(ERpgBaseStorageResultCode::CapacityFull);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (!BaseStorage->CanStoreResourceInstance(
			Entry->Instance,
			CountToDeposit))
	{
		Complete(ERpgBaseStorageResultCode::UnsupportedMode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Deposit,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	const EMaterialDepositResult DepositResult = TryDepositMaterialStack(
		PlayerInventory,
		BaseStorage,
		Request.ItemId,
		ItemDefinition,
		Entry->StackCount,
		CountToDeposit);
	Complete(
		DepositResult == EMaterialDepositResult::Success
			? (CountToDeposit == Request.RequestedCount
				? ERpgBaseStorageResultCode::Success
				: ERpgBaseStorageResultCode::Partial)
			: (DepositResult == EMaterialDepositResult::RollbackFailed
				? ERpgBaseStorageResultCode::InternalRollback
				: ERpgBaseStorageResultCode::Conflict),
		DepositResult == EMaterialDepositResult::Success
			? CountToDeposit
			: 0);
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_Deposit,
		Result,
		Request.ItemId,
		ItemDefinition);
}

void FRpgBaseStorageActionHandler::WithdrawExact(
	FRpgBaseStorageWithdrawRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(Request);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Withdraw,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				Request.RequestedCount),
			FRpgInventoryItemId(),
			Request.ItemDefinition);
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Withdraw,
			Result,
			FRpgInventoryItemId(),
			Request.ItemDefinition);
		return;
	}
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			Request.RequestedCount,
			Applied);
	};

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	const URpgInventoryFragment_StorageProfile* Profile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			Request.ItemDefinition);
	if (!PlayerInventory || !Request.ItemDefinition ||
		Request.RequestedCount <= 0)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
	}
	else if (!FindAccessibleBaseStation(
			BaseCamp,
			GetRequestingActor(),
			Request.ItemDefinition))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
	}
	else if (!Profile ||
		!BaseStorage->CanManuallyDepositBulk(Request.ItemDefinition))
	{
		Complete(ERpgBaseStorageResultCode::UnsupportedMode);
	}
	else if (BaseStorage->GetResourceCount(Request.ItemDefinition) <
		Request.RequestedCount)
	{
		Complete(ERpgBaseStorageResultCode::MissingCosts);
	}
	else if (!PlayerInventory->CanAddItemDefinition(
			Request.ItemDefinition,
			Request.RequestedCount))
	{
		Complete(ERpgBaseStorageResultCode::NoPlacement);
	}
	else
	{
		FRpgInventoryGraphSaveData PlayerCheckpoint;
		FRpgBaseResourceMutationCheckpoint StorageCheckpoint;
		if (!BaseStorage->CaptureResourceMutationCheckpoint(
				Request.ItemDefinition,
				StorageCheckpoint) ||
			!CaptureInventoryCheckpoint(
				PlayerInventory,
				PlayerCheckpoint))
		{
			Complete(ERpgBaseStorageResultCode::InvalidRequest);
		}
		else if (!BaseStorage->WithdrawResource(
				Request.ItemDefinition,
				Request.RequestedCount))
		{
			Complete(ERpgBaseStorageResultCode::Conflict);
		}
		else if (PlayerInventory->GrantItemDefinition(
				Request.ItemDefinition,
				Request.RequestedCount))
		{
			Complete(
				ERpgBaseStorageResultCode::Success,
				Request.RequestedCount);
		}
		else
		{
			const bool bInventoryRestored = RestoreInventoryCheckpoint(
				PlayerInventory,
				PlayerCheckpoint);
			const bool bStorageRestored =
				BaseStorage->RestoreResourceMutationCheckpoint(
					StorageCheckpoint);
			Complete(
				bInventoryRestored && bStorageRestored
					? ERpgBaseStorageResultCode::NoPlacement
					: ERpgBaseStorageResultCode::InternalRollback);
		}
	}

	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_Withdraw,
		Result,
		FRpgInventoryItemId(),
		Request.ItemDefinition);
}

void FRpgBaseStorageActionHandler::DepositAllMaterials(
	URpgBaseStorageStationComponent* Station)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage)
	{
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		PlayerInventory->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		URpgInventoryItemInstance* Item = Entry.Instance;
		if (!Item || Entry.StackCount <= 0 ||
			!IsMaterialItem(Item))
		{
			continue;
		}

		const TSubclassOf<URpgInventoryItemDefinition>
			ItemDefinition = Item->GetItemDef();
		if (!Station->AllowsResourceDefinition(ItemDefinition))
		{
			continue;
		}

		const int32 CountToDeposit = FMath::Min(
			Entry.StackCount,
			BaseStorage->GetFreeResourceCapacity(
				ItemDefinition));
		if (CountToDeposit <= 0 ||
			!BaseStorage->CanStoreResource(
				ItemDefinition,
				CountToDeposit))
		{
			continue;
		}

		TryDepositMaterialStack(
			PlayerInventory,
			BaseStorage,
			Entry.ItemId,
			ItemDefinition,
			Entry.StackCount,
			CountToDeposit);
	}
}

void FRpgBaseStorageActionHandler::DepositMaterialStack(
	URpgBaseStorageStationComponent* Station,
	URpgInventoryItemInstance* Item,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage ||
		!Item ||
		!IsMaterialItem(Item))
	{
		return;
	}

	const int32 AvailableCount =
		PlayerInventory->GetItemStackCount(Item);
	if (AvailableCount <= 0)
	{
		return;
	}

	const int32 RequestedCount =
		StackCount <= 0 ? AvailableCount : StackCount;
	const int32 TransferCount =
		FMath::Min(AvailableCount, RequestedCount);
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item->GetItemDef();
	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (TransferCount <= 0 ||
		!BaseStorage->CanStoreResource(
			ItemDefinition,
			TransferCount))
	{
		return;
	}

	TryDepositMaterialStack(
		PlayerInventory,
		BaseStorage,
		Item->GetItemId(),
		ItemDefinition,
		AvailableCount,
		TransferCount);
}

void FRpgBaseStorageActionHandler::WithdrawResource(
	URpgBaseStorageStationComponent* Station,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 StackCount)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!PlayerInventory ||
		!BaseStorage ||
		!ItemDefinition ||
		StackCount <= 0)
	{
		return;
	}

	if (!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	if (BaseStorage->GetResourceCount(ItemDefinition) <
			StackCount ||
		!PlayerInventory->CanAddItemDefinition(
			ItemDefinition,
			StackCount))
	{
		return;
	}

	if (BaseStorage->WithdrawResource(
			ItemDefinition,
			StackCount))
	{
		if (!PlayerInventory->GrantItemDefinition(
				ItemDefinition,
				StackCount) &&
			!BaseStorage->StoreDefinitionResource(
				ItemDefinition,
				StackCount))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Error,
				TEXT("Resource-withdraw rollback failed after the player inventory rejected %d x %s."),
				StackCount,
				*GetNameSafe(ItemDefinition));
		}
	}
}

void FRpgBaseStorageActionHandler::InstallUpgradeById(
	FRpgBaseStorageUpgradeRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(
		Request,
		ERpgStorageCommandHashSalt::Install);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				1));
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			1,
			Applied,
			ResourceOutcomes);
	};

	URpgBaseStorageUpgradeDefinition* Upgrade =
		ResolveStorageUpgrade(Request.UpgradeId);
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	APlayerController* RequestingController =
		Cast<APlayerController>(GetOwner());
	if (!Upgrade || Upgrade->TargetAnchorId != Request.ExpectedAnchorId ||
		!PlayerInventory || !RequestingController)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	URpgBaseStorageStationComponent* AccessStation =
		FindAccessibleBaseStation(
			BaseCamp,
			GetRequestingActor(),
			nullptr,
			Upgrade);
	const bool bBaseWasUnowned = BaseCamp->GetOwnerProfileKey().IsEmpty();
	if (!AccessStation ||
		(!bBaseWasUnowned && !BaseCamp->IsBaseOwner(GetOwner())))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}
	if (!BaseStorage->GetInstalledCapabilities().HasAllExact(
			Upgrade->RequiredInstalledCapabilityTags))
	{
		Complete(ERpgBaseStorageResultCode::CapabilityLocked);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	const ARpgGameStateBase* GameState = GetWorld()
		? GetWorld()->GetGameState<ARpgGameStateBase>()
		: nullptr;
	const URpgWorldStorageKnowledgeComponent* Knowledge = GameState
		? GameState->GetWorldStorageKnowledgeComponent()
		: nullptr;
	if (!Upgrade->RequiredKnowledgeTags.IsEmpty() &&
		(!Knowledge || !Knowledge->HasAllKnowledgeTags(
			Upgrade->RequiredKnowledgeTags)))
	{
		Complete(ERpgBaseStorageResultCode::KnowledgeMissing);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	FText FailureReason;
	if (!BaseStorage->CanInstallUpgrade(Upgrade, FailureReason))
	{
		Complete(ERpgBaseStorageResultCode::Conflict);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> Costs;
	if (!AggregateBulkCosts(Upgrade->Costs, BaseStorage, Costs))
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}
	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder =
		AccessStation->GetUpgradeCostConsumeOrder();
	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
		Costs)
	{
		if (GetAvailableUpgradeCostCount(
				PlayerInventory,
				BaseStorage,
				Cost.Key,
				ConsumeOrder) < Cost.Value)
		{
			ResourceOutcomes = BuildDefinitionResourceOutcomes(
				Costs,
				ERpgBaseStorageResultCode::MissingCosts,
				false);
			Complete(ERpgBaseStorageResultCode::MissingCosts);
			SendBaseStorageCommandFeedback(
				TAG_Rpg_BaseStorage_Action_InstallUpgrade,
				Result);
			return;
		}
	}

	FRpgInventoryGraphSaveData PlayerCheckpoint;
	if (!CaptureInventoryCheckpoint(PlayerInventory, PlayerCheckpoint))
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}
	FBaseResourceCheckpointMap BaseResourceCheckpoints;
	if (!CaptureBaseResourceCheckpoints(
			BaseStorage,
			Costs,
			BaseResourceCheckpoints))
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	bool bPaymentCommitted = true;
	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
		Costs)
	{
		int32 BaseConsumed = 0;
		const bool bCostCommitted = ConsumeUpgradePaymentTracked(
			PlayerInventory,
			BaseStorage,
			Cost.Key,
			Cost.Value,
			ConsumeOrder,
			BaseConsumed);
		if (!bCostCommitted)
		{
			bPaymentCommitted = false;
			break;
		}
	}

	auto RollbackPayment = [&]()
	{
		const bool bPlayerRestored = RestoreInventoryCheckpoint(
			PlayerInventory,
			PlayerCheckpoint);
		const bool bBaseRestored = RestoreBaseResourceCheckpoints(
			BaseStorage,
			BaseResourceCheckpoints);
		return bPlayerRestored && bBaseRestored;
	};
	if (!bPaymentCommitted)
	{
		const ERpgBaseStorageResultCode FailureCode = RollbackPayment()
			? ERpgBaseStorageResultCode::Conflict
			: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		Complete(FailureCode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_InstallUpgrade,
			Result);
		return;
	}

	if (!BaseStorage->InstallUpgrade(Upgrade))
	{
		const ERpgBaseStorageResultCode FailureCode = RollbackPayment()
			? ERpgBaseStorageResultCode::Conflict
			: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		Complete(FailureCode);
	}
	else if (bBaseWasUnowned &&
		!BaseCamp->EnsureClaimedByController(RequestingController))
	{
		const bool bUpgradeRolledBack =
			BaseStorage->DecommissionUpgrade(Upgrade);
		const bool bPaymentRolledBack = RollbackPayment();
		const ERpgBaseStorageResultCode FailureCode =
			bUpgradeRolledBack && bPaymentRolledBack
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		Complete(FailureCode);
	}
	else
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			ERpgBaseStorageResultCode::Success,
			true);
		Complete(ERpgBaseStorageResultCode::Success, 1);
	}
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_InstallUpgrade,
		Result);
}

void FRpgBaseStorageActionHandler::DecommissionUpgrade(
	FRpgBaseStorageUpgradeRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(
		Request,
		ERpgStorageCommandHashSalt::Decommission);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				1));
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			1,
			Applied,
			ResourceOutcomes);
	};

	URpgBaseStorageUpgradeDefinition* Upgrade =
		ResolveStorageUpgrade(Request.UpgradeId);
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	if (!Upgrade || Upgrade->TargetAnchorId != Request.ExpectedAnchorId ||
		!PlayerInventory)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}
	if (!FindAccessibleBaseStation(
			BaseCamp,
			GetRequestingActor(),
			nullptr,
			Upgrade) ||
		!BaseCamp->IsBaseOwner(GetOwner()))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}

	FText FailureReason;
	if (!BaseStorage->CanDecommissionUpgrade(Upgrade, FailureReason))
	{
		Complete(ERpgBaseStorageResultCode::Conflict);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> Refunds;
	if (!AggregateBulkCosts(
			Upgrade->DecommissionRefunds,
			BaseStorage,
			Refunds))
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}

	FRpgInventoryGraphSaveData PlayerCheckpoint;
	if (!CaptureInventoryCheckpoint(PlayerInventory, PlayerCheckpoint))
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}

	bool bRefundsGranted = true;
	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Refund :
		Refunds)
	{
		if (!PlayerInventory->GrantItemDefinition(
				Refund.Key,
				Refund.Value))
		{
			bRefundsGranted = false;
			break;
		}
	}
	if (!bRefundsGranted)
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreInventoryCheckpoint(PlayerInventory, PlayerCheckpoint)
				? ERpgBaseStorageResultCode::NoPlacement
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Refunds,
			FailureCode,
			false);
		Complete(FailureCode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
			Result);
		return;
	}

	if (!BaseStorage->DecommissionUpgrade(Upgrade))
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreInventoryCheckpoint(PlayerInventory, PlayerCheckpoint)
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Refunds,
			FailureCode,
			false);
		Complete(FailureCode);
	}
	else
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Refunds,
			ERpgBaseStorageResultCode::Success,
			true);
		Complete(ERpgBaseStorageResultCode::Success, 1);
	}
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_DecommissionUpgrade,
		Result);
}

void FRpgBaseStorageActionHandler::InstallUpgrade(
	URpgBaseStorageStationComponent* Station,
	URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	URpgInventoryManagerComponent* PlayerInventory =
		FindPlayerInventory();
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!Station)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: Station is null. Owner=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!CanAccessBaseStorageStation(Station))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: station access denied. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!PlayerInventory)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: player inventory missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	if (!BaseStorage)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: base storage missing. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	ARpgBaseCampActor* BaseCamp = Station->GetBaseCamp();
	APlayerController* RequestingController =
		Cast<APlayerController>(GetOwner());
	if (!RequestingController || !BaseCamp ||
		!BaseCamp->EnsureClaimedByController(RequestingController))
	{
		UE_LOG(LogRpgInventoryUiActions, Warning,
			TEXT("Install base storage upgrade failed: only the stable base owner may install upgrades."));
		return;
	}

	if (!UpgradeDefinition)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: upgrade definition is null. Owner=%s Station=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station));
		return;
	}

	if (!Station->CanInstallUpgrade(UpgradeDefinition))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Install base storage upgrade failed: station cannot install upgrade, maybe already installed or station tags do not match. Owner=%s Station=%s Upgrade=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Station),
			*GetNameSafe(UpgradeDefinition));
		return;
	}

	const ERpgBaseStorageUpgradeCostConsumeOrder ConsumeOrder =
		Station->GetUpgradeCostConsumeOrder();
	UE_LOG(
		LogRpgInventoryUiActions,
		Log,
		TEXT("Install base storage upgrade requested: Owner=%s Station=%s Upgrade=%s CostCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		UpgradeDefinition->Costs.Num());

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> AggregatedCosts;
	for (const FRpgBaseStorageUpgradeCost& Cost : UpgradeDefinition->Costs)
	{
		if (!Cost.ItemDefinition)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: empty cost item definition. Upgrade=%s"),
				*GetNameSafe(UpgradeDefinition));
			return;
		}

		if (Cost.Count <= 0)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: invalid cost count. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition),
				Cost.Count);
			return;
		}

		if (!IsMaterialItemDefinition(Cost.ItemDefinition))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: cost item is not an explicit Materials BulkResource. Upgrade=%s ItemDef=%s"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}
		if (BaseStorage->GetBulkCapacityCost(Cost.ItemDefinition) <= 0)
		{
			UE_LOG(LogRpgInventoryUiActions, Warning,
				TEXT("Install base storage upgrade failed: cost definition %s is not an explicit BulkResource."),
				*GetNameSafe(Cost.ItemDefinition));
			return;
		}
		AggregatedCosts.FindOrAdd(Cost.ItemDefinition) += Cost.Count;
	}

	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
		AggregatedCosts)
	{
		const int32 AvailableCount =
			GetAvailableUpgradeCostCount(
				PlayerInventory,
				BaseStorage,
				Cost.Key,
				ConsumeOrder);
		if (AvailableCount < Cost.Value)
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: not enough resources. Upgrade=%s ItemDef=%s Available=%d Required=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.Key),
				AvailableCount,
				Cost.Value);
			return;
		}
	}

	const FRpgInventoryGraphSaveData PlayerCheckpoint =
		PlayerInventory->ExportInventoryGraph();
	FRpgBaseStorageSaveData StorageCheckpoint;
	BaseStorage->ExportStorageState(StorageCheckpoint);
	auto RollbackPayment = [&]()
	{
		FRpgInventoryMutationResult InventoryRollback;
		FString StorageRollbackError;
		const bool bInventoryRolledBack =
			PlayerInventory->RestoreRuntimeCheckpoint(
				PlayerCheckpoint, InventoryRollback) &&
			InventoryRollback.IsSuccess();
		const bool bStorageRolledBack =
			BaseStorage->RestoreStorageState(
				StorageCheckpoint, StorageRollbackError);
		if (!bInventoryRolledBack || !bStorageRolledBack)
		{
			UE_LOG(LogRpgInventoryUiActions, Error,
				TEXT("Atomic upgrade payment rollback failed. Inventory=%s Storage=%s Error=%s"),
				bInventoryRolledBack ? TEXT("restored") : TEXT("failed"),
				bStorageRolledBack ? TEXT("restored") : TEXT("failed"),
				*StorageRollbackError);
		}
	};

	for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Cost :
		AggregatedCosts)
	{
		if (!ConsumeUpgradeCost(
				PlayerInventory,
				BaseStorage,
				Cost.Key,
				Cost.Value,
				ConsumeOrder))
		{
			UE_LOG(
				LogRpgInventoryUiActions,
				Warning,
				TEXT("Install base storage upgrade failed: cost consume failed after validation. Upgrade=%s ItemDef=%s Count=%d"),
				*GetNameSafe(UpgradeDefinition),
				*GetNameSafe(Cost.Key),
				Cost.Value);
			RollbackPayment();
			return;
		}
	}

	const bool bInstalled =
		Station->InstallUpgrade(UpgradeDefinition);
	if (!bInstalled)
	{
		RollbackPayment();
	}
	UE_LOG(
		LogRpgInventoryUiActions,
		Log,
		TEXT("Install base storage upgrade result: Owner=%s Station=%s Upgrade=%s Installed=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Station),
		*GetNameSafe(UpgradeDefinition),
		bInstalled ? TEXT("true") : TEXT("false"));
}

void FRpgBaseStorageActionHandler::StabilizeContainedItem(
	FRpgBaseStorageRiftItemRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(
		Request,
		ERpgStorageCommandHashSalt::Stabilize);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				1),
			Request.ItemId);
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			1,
			Applied);
	};

	URpgInventoryManagerComponent* ContainmentInventory = BaseCamp
		? BaseCamp->GetContainmentInventoryComponent()
		: nullptr;
	if (!ContainmentInventory || !Request.ItemId.IsValid() ||
		Request.ExpectedContainmentRevision == INDEX_NONE ||
		Request.bConfirmed)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}
	if (!FindAccessibleBaseStation(BaseCamp, GetRequestingActor()))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}
	if (!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftContainment) ||
		!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftStabilize))
	{
		Complete(ERpgBaseStorageResultCode::CapabilityLocked);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}
	if (ContainmentInventory->GetInventoryRevision() !=
		Request.ExpectedContainmentRevision)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		ContainmentInventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry =
		FindEntryByItemId(Entries, Request.ItemId);
	if (!Entry || !Entry->Instance)
	{
		Complete(ERpgBaseStorageResultCode::MissingItem);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}
	if (BaseCamp->IsContainmentItemStabilized(Request.ItemId) !=
		Request.bExpectedStabilized)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId);
		return;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Entry->Instance->GetItemDef();
	const URpgInventoryFragment_StorageProfile* StorageProfile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			ItemDefinition);
	const URpgInventoryFragment_ContainmentProfile* ContainmentProfile =
		Entry->Instance->FindFragmentByClass<
			URpgInventoryFragment_ContainmentProfile>();
	auto CompleteWithStabilizationState = [
		&Result,
		&ResourceOutcomes,
		BaseStorage,
		&Request,
		PayloadHash,
		BaseCamp](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		FRpgBaseStorageCommandResult DetailedResult;
		DetailedResult.Code = Code;
		DetailedResult.RequestedCount = 1;
		DetailedResult.AppliedCount = Applied;
		DetailedResult.bHasStabilizationState = true;
		DetailedResult.bWasStabilized = false;
		DetailedResult.bIsStabilized =
			BaseCamp->IsContainmentItemStabilized(Request.ItemId);
		DetailedResult.ResourceOutcomes = ResourceOutcomes;
		Result = BaseStorage->CompleteDetailedCommand(
			Request.Context,
			PayloadHash,
			MoveTemp(DetailedResult));
	};
	if (Entry->StackCount != 1 || !StorageProfile ||
		!StorageProfile->RequiresContainment() ||
		!StorageProfile->IsStructurallyValid() ||
		StorageProfile->StorageDomainTag !=
			RpgGameplayTags::Storage_Domain_RiftContainment ||
		!BaseStorage->GetInstalledCapabilities().HasAllExact(
			StorageProfile->RequiredStorageCapabilityTags) ||
		!HasRequiredContainmentConfiguration(
			BaseStorage,
			ContainmentProfile) ||
		BaseStorage->GetContainmentSlotCapacity() <
			ContainmentProfile->RequiredSealedSlots ||
		BaseStorage->IsContainmentDomainOverCapacity())
	{
		Complete(ERpgBaseStorageResultCode::UnsupportedMode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (BaseCamp->IsContainmentItemStabilized(Request.ItemId))
	{
		FRpgBaseStorageCommandResult DetailedResult;
		DetailedResult.Code = ERpgBaseStorageResultCode::Conflict;
		DetailedResult.RequestedCount = 1;
		DetailedResult.bHasStabilizationState = true;
		DetailedResult.bWasStabilized = true;
		DetailedResult.bIsStabilized = true;
		Result = BaseStorage->CompleteDetailedCommand(
			Request.Context,
			PayloadHash,
			MoveTemp(DetailedResult));
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> Costs;
	if (!AggregateBulkCosts(
			ContainmentProfile->StabilizationCosts,
			BaseStorage,
			Costs))
	{
		CompleteWithStabilizationState(
			ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (!CanAffordBaseResourceCosts(BaseStorage, Costs))
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			ERpgBaseStorageResultCode::MissingCosts,
			false);
		CompleteWithStabilizationState(
			ERpgBaseStorageResultCode::MissingCosts);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Stabilize,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	FBaseResourceCheckpointMap ResourceCheckpoints;
	if (!ConsumeBaseResourceCosts(
			BaseStorage,
			Costs,
			ResourceCheckpoints))
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreBaseResourceCheckpoints(
				BaseStorage,
				ResourceCheckpoints)
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		CompleteWithStabilizationState(FailureCode);
	}
	else if (!BaseCamp->SetContainmentItemStabilized(
			Request.ItemId,
			true))
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreBaseResourceCheckpoints(
				BaseStorage,
				ResourceCheckpoints)
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		CompleteWithStabilizationState(FailureCode);
	}
	else
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			ERpgBaseStorageResultCode::Success,
			true);
		CompleteWithStabilizationState(
			ERpgBaseStorageResultCode::Success,
			1);
	}
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_Stabilize,
		Result,
		Request.ItemId,
		ItemDefinition);
}

void FRpgBaseStorageActionHandler::ExtractContainedItem(
	FRpgBaseStorageRiftItemRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(
		Request,
		ERpgStorageCommandHashSalt::Extract);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest,
				1),
			Request.ItemId);
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			1,
			Applied);
	};

	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory();
	URpgInventoryManagerComponent* ContainmentInventory = BaseCamp
		? BaseCamp->GetContainmentInventoryComponent()
		: nullptr;
	if (!PlayerInventory || !ContainmentInventory ||
		!Request.ItemId.IsValid() ||
		Request.ExpectedContainmentRevision == INDEX_NONE)
	{
		Complete(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}
	if (!FindAccessibleBaseStation(BaseCamp, GetRequestingActor()) ||
		!BaseCamp->IsBaseOwner(GetOwner()))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}
	if (!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftContainment) ||
		!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftExtract))
	{
		Complete(ERpgBaseStorageResultCode::CapabilityLocked);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}
	if (ContainmentInventory->GetInventoryRevision() !=
		Request.ExpectedContainmentRevision)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}

	const TArray<FRpgInventoryEntryView> Entries =
		ContainmentInventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry =
		FindEntryByItemId(Entries, Request.ItemId);
	if (!Entry || !Entry->Instance)
	{
		Complete(ERpgBaseStorageResultCode::MissingItem);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}
	if (BaseCamp->IsContainmentItemStabilized(Request.ItemId) !=
		Request.bExpectedStabilized)
	{
		Complete(ERpgBaseStorageResultCode::Stale);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId);
		return;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Entry->Instance->GetItemDef();
	const URpgInventoryFragment_StorageProfile* StorageProfile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			ItemDefinition);
	const URpgInventoryFragment_ContainmentProfile* ContainmentProfile =
		Entry->Instance->FindFragmentByClass<
			URpgInventoryFragment_ContainmentProfile>();
	if (Entry->StackCount != 1 || !StorageProfile ||
		!StorageProfile->RequiresContainment() ||
		!StorageProfile->IsStructurallyValid() ||
		StorageProfile->StorageDomainTag !=
			RpgGameplayTags::Storage_Domain_RiftContainment ||
		!BaseStorage->GetInstalledCapabilities().HasAllExact(
			StorageProfile->RequiredStorageCapabilityTags) ||
		!HasRequiredContainmentConfiguration(
			BaseStorage,
			ContainmentProfile) ||
		!ContainmentProfile->HasExtractionOutput())
	{
		Complete(ERpgBaseStorageResultCode::UnsupportedMode);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (!BaseCamp->IsContainmentItemStabilized(Request.ItemId))
	{
		FRpgBaseStorageCommandResult DetailedResult;
		DetailedResult.Code = ERpgBaseStorageResultCode::Conflict;
		DetailedResult.RequestedCount = 1;
		DetailedResult.bHasStabilizationState = true;
		DetailedResult.bWasStabilized = false;
		DetailedResult.bIsStabilized = false;
		Result = BaseStorage->CompleteDetailedCommand(
			Request.Context,
			PayloadHash,
			MoveTemp(DetailedResult));
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	const int32 PreviousStrain = BaseStorage->GetRiftStrain();
	const int32 MitigatedExtractionStrain =
		BaseStorage->GetMitigatedRiftStrainDelta(
			ContainmentProfile->ExtractionStrain);
	const int32 PreviewStrain = PreviousStrain +
		MitigatedExtractionStrain;
	auto CompleteExtraction = [
		&Result,
		BaseStorage,
		&Request,
		PayloadHash,
		ContainmentProfile,
		PreviousStrain](
			ERpgBaseStorageResultCode Code,
			int32 Applied,
			int32 ResultingStrain,
			bool bResultingStabilized)
	{
		FRpgBaseStorageCommandResult DetailedResult;
		DetailedResult.Code = Code;
		DetailedResult.RequestedCount = 1;
		DetailedResult.AppliedCount = Applied;
		DetailedResult.RiftOutputItemDefinition =
			ContainmentProfile->ExtractionOutputDefinition;
		DetailedResult.RiftOutputCount =
			ContainmentProfile->ExtractionOutputCount;
		DetailedResult.RiftStrainBefore = PreviousStrain;
		DetailedResult.RiftStrainAfter = ResultingStrain;
		DetailedResult.bHasStabilizationState = true;
		DetailedResult.bWasStabilized = true;
		DetailedResult.bIsStabilized = bResultingStabilized;
		Result = BaseStorage->CompleteDetailedCommand(
			Request.Context,
			PayloadHash,
			MoveTemp(DetailedResult));
	};
	if (PreviousStrain >= 100 || MitigatedExtractionStrain >
		100 - PreviousStrain)
	{
		CompleteExtraction(
			ERpgBaseStorageResultCode::StrainBlocked,
			0,
			PreviewStrain,
			true);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}
	if (!PlayerInventory->CanAddItemDefinition(
			ContainmentProfile->ExtractionOutputDefinition,
			ContainmentProfile->ExtractionOutputCount))
	{
		CompleteExtraction(
			ERpgBaseStorageResultCode::NoPlacement,
			0,
			PreviousStrain,
			true);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}
	if (!Request.bConfirmed)
	{
		CompleteExtraction(
			ERpgBaseStorageResultCode::ConfirmationRequired,
			0,
			PreviewStrain,
			true);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}

	FRpgInventoryGraphSaveData PlayerCheckpoint;
	if (!CaptureInventoryCheckpoint(PlayerInventory, PlayerCheckpoint))
	{
		CompleteExtraction(
			ERpgBaseStorageResultCode::InvalidRequest,
			0,
			PreviousStrain,
			true);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ItemDefinition);
		return;
	}

	// The contained instance is the destructive input of extraction. Keep its
	// concrete UObject, entry id, and placement alive until every reversible
	// output/state mutation has succeeded. Restoring a graph after consuming the
	// input could recreate the same persistent ItemId on a different UObject and
	// silently invalidate gameplay references.
	TObjectPtr<URpgInventoryItemInstance> OriginalContainmentInstance =
		Entry->Instance;
	const FGuid OriginalContainmentEntryId = Entry->EntryId;
	const FRpgInventoryGridPlacement OriginalContainmentPlacement =
		Entry->Placement;
	const int32 OriginalContainmentCount = Entry->StackCount;
	const int32 PreviousCleanseableStrain =
		BaseStorage->GetCleanseableRiftStrain();
	auto IsOriginalContainmentEntryIntact = [&]()
	{
		const TArray<FRpgInventoryEntryView> CurrentEntries =
			ContainmentInventory->GetAllEntries();
		const FRpgInventoryEntryView* CurrentEntry =
			FindEntryByItemId(CurrentEntries, Request.ItemId);
		return CurrentEntry &&
			CurrentEntry->Instance == OriginalContainmentInstance &&
			CurrentEntry->EntryId == OriginalContainmentEntryId &&
			CurrentEntry->StackCount == OriginalContainmentCount &&
			CurrentEntry->Placement == OriginalContainmentPlacement;
	};
	auto RollbackReversibleExtraction = [&](bool bRestoreContainmentState)
	{
		const bool bContainmentStateRestored =
			!bRestoreContainmentState ||
			BaseCamp->SetContainmentItemStabilized(
				Request.ItemId,
				true);
		const bool bStrainRestored =
			BaseStorage->RestoreRiftStrainCheckpoint(
				PreviousCleanseableStrain);
		const bool bPlayerRestored = RestoreInventoryCheckpoint(
			PlayerInventory,
			PlayerCheckpoint);
		return bContainmentStateRestored && bStrainRestored &&
			bPlayerRestored &&
			IsOriginalContainmentEntryIntact() &&
			BaseCamp->IsContainmentItemStabilized(Request.ItemId);
	};

	if (!PlayerInventory->GrantItemDefinition(
			ContainmentProfile->ExtractionOutputDefinition,
			ContainmentProfile->ExtractionOutputCount))
	{
		CompleteExtraction(
			ERpgBaseStorageResultCode::NoPlacement,
			0,
			PreviousStrain,
			true);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}

	const bool bStrainApplied =
		ContainmentProfile->ExtractionStrain <= 0 ||
		BaseStorage->TryAddRiftStrain(
			ContainmentProfile->ExtractionStrain);
	if (!bStrainApplied)
	{
		const bool bRolledBack = RollbackReversibleExtraction(false);
		const int32 ResultingStrain = BaseStorage->GetRiftStrain();
		const bool bResultingStabilized =
			BaseCamp->IsContainmentItemStabilized(Request.ItemId);
		CompleteExtraction(
			bRolledBack
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback,
			0,
			ResultingStrain,
			bResultingStabilized);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}

	if (!BaseCamp->ForgetContainmentItemState(Request.ItemId))
	{
		const bool bRolledBack = RollbackReversibleExtraction(false);
		const int32 ResultingStrain = BaseStorage->GetRiftStrain();
		const bool bResultingStabilized =
			BaseCamp->IsContainmentItemStabilized(Request.ItemId);
		CompleteExtraction(
			bRolledBack
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback,
			0,
			ResultingStrain,
			bResultingStabilized);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}

	const FRpgInventoryMutationResult ConsumeResult =
		ContainmentInventory->ConsumeItemById(Request.ItemId, 1);
	if (!ConsumeResult.IsSuccess() || ConsumeResult.AppliedQuantity != 1)
	{
		// Never rebuild the vault graph here: doing so would manufacture a new
		// UObject for the consumed input. Roll back only while the exact original
		// entry is demonstrably still present; otherwise fail closed and taint the
		// storage network through InternalRollback.
		const bool bRolledBack = RollbackReversibleExtraction(true);
		const int32 ResultingStrain = BaseStorage->GetRiftStrain();
		const bool bResultingStabilized =
			BaseCamp->IsContainmentItemStabilized(Request.ItemId);
		CompleteExtraction(
			bRolledBack
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback,
			0,
			ResultingStrain,
			bResultingStabilized);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Extract,
			Result,
			Request.ItemId,
			ContainmentProfile->ExtractionOutputDefinition);
		return;
	}

	CompleteExtraction(
		ERpgBaseStorageResultCode::Success,
		1,
		BaseStorage->GetRiftStrain(),
		false);
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_Extract,
		Result,
		Request.ItemId,
		ContainmentProfile->ExtractionOutputDefinition);
}

void FRpgBaseStorageActionHandler::CleanseRiftStrain(
	FRpgBaseStorageCleanseRequest Request)
{
	const uint32 PayloadHash = GetStoragePayloadHash(Request);
	ARpgBaseCampActor* BaseCamp = ResolveUniqueBaseCamp(
		GetWorld(),
		Request.Context.BaseId);
	URpgBaseStorageComponent* BaseStorage = BaseCamp
		? BaseCamp->GetBaseStorageComponent()
		: nullptr;
	if (!BaseStorage)
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			MakeRejectedStorageResult(
				Request.Context,
				ERpgBaseStorageResultCode::InvalidRequest));
		return;
	}

	FRpgBaseStorageCommandResult Result;
	if (!AdmitModernStorageCommand(
			BaseStorage,
			Cast<APlayerController>(GetOwner()),
			Request.Context,
			PayloadHash,
			Result))
	{
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}
	TArray<FRpgBaseStorageResourceCommandOutcome> ResourceOutcomes;
	const int32 RequestedCleanseAmount =
		FMath::Max(0, BaseStorage->GetRiftCleanseAmount());
	auto Complete = [&](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		Result = BaseStorage->CompleteCommand(
			Request.Context,
			PayloadHash,
			Code,
			RequestedCleanseAmount,
			Applied);
	};

	if (!FindAccessibleBaseStation(BaseCamp, GetRequestingActor()))
	{
		Complete(ERpgBaseStorageResultCode::NoAccess);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}
	if (!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftContainment) ||
		!BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftStabilize))
	{
		Complete(ERpgBaseStorageResultCode::CapabilityLocked);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}
	const int32 PreviousStrain = BaseStorage->GetRiftStrain();
	const int32 PreviousCleanseableStrain =
		BaseStorage->GetCleanseableRiftStrain();
	auto CompleteCleanse = [
		&Result,
		&ResourceOutcomes,
		BaseStorage,
		&Request,
		PayloadHash,
		RequestedCleanseAmount,
		PreviousStrain](ERpgBaseStorageResultCode Code, int32 Applied = 0)
	{
		FRpgBaseStorageCommandResult DetailedResult;
		DetailedResult.Code = Code;
		DetailedResult.RequestedCount = RequestedCleanseAmount;
		DetailedResult.AppliedCount = Applied;
		DetailedResult.RiftStrainBefore = PreviousStrain;
		DetailedResult.RiftStrainAfter = BaseStorage->GetRiftStrain();
		DetailedResult.ResourceOutcomes = ResourceOutcomes;
		Result = BaseStorage->CompleteDetailedCommand(
			Request.Context,
			PayloadHash,
			MoveTemp(DetailedResult));
	};
	if (RequestedCleanseAmount <= 0 || PreviousCleanseableStrain <= 0)
	{
		CompleteCleanse(ERpgBaseStorageResultCode::Conflict);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}

	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> Costs;
	if (!AggregateBulkCosts(
			BaseStorage->GetRiftCleanseCosts(),
			BaseStorage,
			Costs))
	{
		CompleteCleanse(ERpgBaseStorageResultCode::InvalidRequest);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}
	if (!CanAffordBaseResourceCosts(BaseStorage, Costs))
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			ERpgBaseStorageResultCode::MissingCosts,
			false);
		CompleteCleanse(ERpgBaseStorageResultCode::MissingCosts);
		SendBaseStorageCommandFeedback(
			TAG_Rpg_BaseStorage_Action_Cleanse,
			Result);
		return;
	}

	FBaseResourceCheckpointMap ResourceCheckpoints;
	if (!ConsumeBaseResourceCosts(
			BaseStorage,
			Costs,
			ResourceCheckpoints))
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreBaseResourceCheckpoints(
				BaseStorage,
				ResourceCheckpoints)
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		CompleteCleanse(FailureCode);
	}
	else if (!BaseStorage->CleanseRiftStrain(RequestedCleanseAmount))
	{
		const ERpgBaseStorageResultCode FailureCode =
			RestoreBaseResourceCheckpoints(
				BaseStorage,
				ResourceCheckpoints)
				? ERpgBaseStorageResultCode::Conflict
				: ERpgBaseStorageResultCode::InternalRollback;
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			FailureCode,
			false);
		CompleteCleanse(FailureCode);
	}
	else
	{
		ResourceOutcomes = BuildDefinitionResourceOutcomes(
			Costs,
			ERpgBaseStorageResultCode::Success,
			true);
		CompleteCleanse(
			ERpgBaseStorageResultCode::Success,
			PreviousCleanseableStrain -
				BaseStorage->GetCleanseableRiftStrain());
	}
	SendBaseStorageCommandFeedback(
		TAG_Rpg_BaseStorage_Action_Cleanse,
		Result);
}

void FRpgBaseStorageActionHandler::ApplyResourceSort(
	URpgBaseStorageStationComponent* Station,
	ERpgInventorySortMode SortMode)
{
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) || !BaseStorage)
	{
		return;
	}

	BaseStorage->ApplyResourceSort(SortMode);
}

void FRpgBaseStorageActionHandler::MoveResourceEntry(
	URpgBaseStorageStationComponent* Station,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 TargetIndex)
{
	URpgBaseStorageComponent* BaseStorage =
		Station ? Station->GetBaseStorage() : nullptr;
	if (!CanAccessBaseStorageStation(Station) ||
		!BaseStorage ||
		!Station->AllowsResourceDefinition(ItemDefinition))
	{
		return;
	}

	BaseStorage->MoveResourceEntry(ItemDefinition, TargetIndex);
}
