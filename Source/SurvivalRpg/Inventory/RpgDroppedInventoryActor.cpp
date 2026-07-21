#include "RpgDroppedInventoryActor.h"

#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_Collect.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgDroppedInventoryActor)

DEFINE_LOG_CATEGORY_STATIC(LogRpgDroppedInventoryActor, Log, All);

ARpgDroppedInventoryActor::ARpgDroppedInventoryActor(const FObjectInitializer& ObjectInitializer)
	: Super()
{
	(void)ObjectInitializer;

	LootInventoryComponent = CreateDefaultSubobject<URpgInventoryManagerComponent>(TEXT("LootInventoryComponent"));
	ContainerComponent = CreateDefaultSubobject<URpgInventoryContainerComponent>(TEXT("ContainerComponent"));
}

void ARpgDroppedInventoryActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDefaultPickupInteractionOption();

	if (LootInventoryComponent)
	{
		bool bRuntimeInventoryIsCanonical = !HasAuthority();
		if (HasAuthority())
		{
			LootInventoryComponent->SetCapacityMode(
				ERpgInventoryCapacityMode::Unlimited);
			const FRpgInventoryGraphSaveData EmptyGraph =
				LootInventoryComponent->ExportInventoryGraph();
			if (PopulateLootInventoryFromPickup(StaticInventory))
			{
				StaticInventory = FInventoryPickup();
				bRuntimeInventoryIsCanonical = true;
			}
			else
			{
				FRpgInventoryMutationResult RollbackResult;
				const bool bRolledBack =
					LootInventoryComponent->ImportInventoryGraph(
						EmptyGraph,
						RollbackResult);
				UE_LOG(
					LogRpgDroppedInventoryActor,
					Error,
					TEXT("Failed to initialize the authoritative loot graph for %s; empty-graph rollback %s (Code=%d), and the static pickup fallback remains intact."),
					*GetNameSafe(this),
					bRolledBack ? TEXT("succeeded") : TEXT("failed"),
					static_cast<int32>(RollbackResult.Code));
			}
		}

		// Clients treat the replicated manager as canonical immediately so an empty
		// FastArray and late join never resurrect StaticInventory. Authority retains the
		// static fallback whenever initial graph population was not committed, including
		// the fail-closed case where an empty-graph rollback could not remove every entry.
		bLootInventoryInitialized = bRuntimeInventoryIsCanonical;
	}
}

FInventoryPickup ARpgDroppedInventoryActor::GetPickupInventory() const
{
	if (LootInventoryComponent && bLootInventoryInitialized)
	{
		return BuildPickupInventoryFromLootInventory();
	}

	return StaticInventory;
}

void ARpgDroppedInventoryActor::SetPickupInventory(const FInventoryPickup& NewPickupInventory)
{
	if (HasAuthority())
	{
		EnsureDefaultPickupInteractionOption();
		if (LootInventoryComponent)
		{
			const TArray<FRpgInventoryEntryView> ExistingEntries = LootInventoryComponent->GetAllEntries();
			const FRpgInventoryGraphSaveData ExistingGraph =
				LootInventoryComponent->ExportInventoryGraph();
			if (ExistingGraph.Items.Num() != ExistingEntries.Num())
			{
				UE_LOG(
					LogRpgDroppedInventoryActor,
					Error,
					TEXT("Cannot replace pickup inventory for %s because its existing graph could not be exported completely."),
					*GetNameSafe(this));
				return;
			}

			auto RestoreExistingGraph =
				[this, &ExistingGraph](const TCHAR* FailureContext)
				{
					FRpgInventoryMutationResult RestoreResult;
					if (!LootInventoryComponent->ImportInventoryGraph(
							ExistingGraph,
							RestoreResult))
					{
						UE_LOG(
							LogRpgDroppedInventoryActor,
							Error,
							TEXT("Pickup replacement rollback failed for %s after %s (Code=%d)."),
							*GetNameSafe(this),
							FailureContext,
							static_cast<int32>(RestoreResult.Code));
					}
				};

			TArray<FRpgInventoryEntryView> RootEntries;
			for (const FRpgInventoryEntryView& Entry : ExistingEntries)
			{
				if (Entry.Placement.GetContainerHandle().IsRoot())
				{
					RootEntries.Add(Entry);
				}
			}

			if (!ExistingEntries.IsEmpty() && RootEntries.IsEmpty())
			{
				UE_LOG(
					LogRpgDroppedInventoryActor,
					Error,
					TEXT("Cannot replace pickup inventory for %s because its existing graph has no root entries."),
					*GetNameSafe(this));
				return;
			}

			for (const FRpgInventoryEntryView& RootEntry : RootEntries)
			{
				if (!RootEntry.ItemId.IsValid() || RootEntry.StackCount <= 0 ||
					!LootInventoryComponent->CanConsumeItemById(
						RootEntry.ItemId,
						RootEntry.StackCount))
				{
					UE_LOG(
						LogRpgDroppedInventoryActor,
						Error,
						TEXT("Cannot replace pickup inventory for %s because root item %s cannot be consumed safely."),
						*GetNameSafe(this),
						*RootEntry.ItemId.ToString());
					return;
				}
			}

			for (const FRpgInventoryEntryView& RootEntry : RootEntries)
			{
				const FRpgInventoryMutationResult RemovalResult =
					LootInventoryComponent->ConsumeItemById(
						RootEntry.ItemId,
						RootEntry.StackCount);
				if (RemovalResult.Code !=
						ERpgInventoryMutationResultCode::Success ||
					RemovalResult.AppliedQuantity != RootEntry.StackCount)
				{
					UE_LOG(
						LogRpgDroppedInventoryActor,
						Error,
						TEXT("Replacing pickup inventory for %s failed while consuming root item %s (Code=%d Applied=%d Requested=%d)."),
						*GetNameSafe(this),
						*RootEntry.ItemId.ToString(),
						static_cast<int32>(RemovalResult.Code),
						RemovalResult.AppliedQuantity,
						RootEntry.StackCount);
					RestoreExistingGraph(TEXT("root consumption"));
					return;
				}
			}

			if (LootInventoryComponent->GetUsedEntryCount() != 0)
			{
				UE_LOG(
					LogRpgDroppedInventoryActor,
					Error,
					TEXT("Cannot replace pickup inventory for %s because %d orphaned entries remain after consuming all roots."),
					*GetNameSafe(this),
					LootInventoryComponent->GetUsedEntryCount());
				RestoreExistingGraph(TEXT("orphan detection"));
				return;
			}

			if (!PopulateLootInventoryFromPickup(NewPickupInventory))
			{
				UE_LOG(
					LogRpgDroppedInventoryActor,
					Error,
					TEXT("Cannot replace pickup inventory for %s because the new pickup payload could not be populated completely."),
					*GetNameSafe(this));
				RestoreExistingGraph(TEXT("new pickup population"));
				return;
			}
		}
		StaticInventory = FInventoryPickup();
		bLootInventoryInitialized = true;
		ForceNetUpdate();
	}
}

FRpgInventoryMutationResult ARpgDroppedInventoryActor::TransferItemFromInventory(
	URpgInventoryManagerComponent* SourceInventory,
	FRpgInventoryItemId ItemId,
	int32 StackCount,
	FGuid RequestId,
	bool bPreventStackMerge)
{
	FRpgInventoryTransferIntent Intent;
	Intent.ItemId = ItemId;
	Intent.Quantity = StackCount;
	Intent.RequestId = RequestId;
	Intent.EnsureRequestId();
	if (const FRecentDropTransferResult* CachedResult =
			RecentDropTransferResults.Find(Intent.RequestId))
	{
		const bool bHasSourceInventory = SourceInventory != nullptr;
		const bool bHasTargetInventory = LootInventoryComponent != nullptr;
		const bool bSourceEpochMatches =
			CachedResult->bHadSourceInventory == bHasSourceInventory &&
			(!bHasSourceInventory ||
			 (CachedResult->SourceInventory.Get() == SourceInventory &&
			  CachedResult->SourceMutationEpoch ==
				  SourceInventory->GetMutationEpoch()));
		const bool bTargetEpochMatches =
			CachedResult->bHadTargetInventory == bHasTargetInventory &&
			(!bHasTargetInventory ||
			 (CachedResult->TargetInventory.Get() == LootInventoryComponent &&
			  CachedResult->TargetMutationEpoch ==
				  LootInventoryComponent->GetMutationEpoch()));
		if (bSourceEpochMatches && bTargetEpochMatches &&
			CachedResult->SourceInventory.Get() == SourceInventory &&
			CachedResult->Intent.ItemId == ItemId &&
			CachedResult->Intent.Quantity == StackCount &&
			CachedResult->bPreventStackMerge == bPreventStackMerge)
		{
			return TransferItemFromInventoryByIntent(
				SourceInventory,
				CachedResult->Intent,
				bPreventStackMerge);
		}
	}

	if (SourceInventory && ItemId.IsValid())
	{
		for (const FRpgInventoryEntryView& Entry :
			SourceInventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId && Entry.EntryId.IsValid() &&
				Entry.Placement.IsValid() && Entry.StackCount > 0)
			{
				Intent.ExpectedEntryId = Entry.EntryId;
				Intent.ExpectedSourcePlacement = Entry.Placement;
				Intent.ExpectedSourceQuantity = Entry.StackCount;
				break;
			}
		}
	}

	return TransferItemFromInventoryByIntent(
		SourceInventory,
		MoveTemp(Intent),
		bPreventStackMerge);
}

FRpgInventoryMutationResult
ARpgDroppedInventoryActor::TransferItemFromInventoryByIntent(
	URpgInventoryManagerComponent* SourceInventory,
	FRpgInventoryTransferIntent Intent,
	bool bPreventStackMerge)
{
	Intent.EnsureRequestId();
	Intent.TargetContainer = LootInventoryComponent
		? FRpgInventoryContainerHandle::MakeRoot(
			LootInventoryComponent->GetDefaultContainerId())
		: FRpgInventoryContainerHandle();

	FRpgInventoryMutationResult Result;
	Result.RequestId = Intent.RequestId;
	Result.Operation = ERpgInventoryMutationOperation::Drop;
	Result.RequestedQuantity = Intent.Quantity;
	if (TryReplayRecentDropTransfer(
			SourceInventory,
			Intent,
			bPreventStackMerge,
			Result))
	{
		return Result;
	}
	auto CacheResult =
		[this,
		 SourceInventory,
		 Intent,
		 bPreventStackMerge](
			FRpgInventoryMutationResult ResultToCache)
		{
			return CacheRecentDropTransfer(
				SourceInventory,
				Intent,
				bPreventStackMerge,
				MoveTemp(ResultToCache));
		};

	if (!HasAuthority())
	{
		Result.Code = ERpgInventoryMutationResultCode::AuthorityRequired;
		return CacheResult(MoveTemp(Result));
	}
	if (!SourceInventory || SourceInventory == LootInventoryComponent ||
		!LootInventoryComponent || !Intent.ItemId.IsValid() ||
		Intent.Quantity <= 0)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}

	URpgInventoryItemInstance* Item =
		SourceInventory->FindItemById(Intent.ItemId);
	if (!Item)
	{
		Result.Code = ERpgInventoryMutationResultCode::ItemNotFound;
		return CacheResult(MoveTemp(Result));
	}
	if (!Intent.ExpectedEntryId.IsValid() ||
		!Intent.ExpectedSourcePlacement.IsValid() ||
		Intent.ExpectedSourceQuantity <= 0 ||
		Intent.Quantity > Intent.ExpectedSourceQuantity)
	{
		Result.Code = ERpgInventoryMutationResultCode::InvalidRequest;
		return CacheResult(MoveTemp(Result));
	}

	const TArray<FRpgInventoryEntryView> SourceEntries =
		SourceInventory->GetAllEntries();
	const FRpgInventoryEntryView* SourceEntry =
		SourceEntries.FindByPredicate(
			[&Intent](const FRpgInventoryEntryView& Entry)
			{
				return Entry.EntryId == Intent.ExpectedEntryId;
			});
	if (!SourceEntry)
	{
		const bool bItemStillHasAnEntry =
			SourceEntries.ContainsByPredicate(
				[&Intent](const FRpgInventoryEntryView& Entry)
				{
					return Entry.ItemId == Intent.ItemId;
				});
		Result.Code = bItemStillHasAnEntry
			? ERpgInventoryMutationResultCode::SourceMismatch
			: ERpgInventoryMutationResultCode::ItemNotFound;
		return CacheResult(MoveTemp(Result));
	}
	if (!SourceEntry->EntryId.IsValid() ||
		!SourceEntry->Placement.IsValid() || SourceEntry->Instance != Item ||
		SourceEntry->ItemId != Intent.ItemId ||
		SourceEntry->Placement != Intent.ExpectedSourcePlacement ||
		SourceEntry->StackCount != Intent.ExpectedSourceQuantity)
	{
		Result.Code = ERpgInventoryMutationResultCode::SourceMismatch;
		return CacheResult(MoveTemp(Result));
	}

	if (bPreventStackMerge)
	{
		auto TryUseConcretePlacement =
			[this, &Intent, Item](
				const FRpgInventoryGridPlacement& Candidate)
			{
				if (!LootInventoryComponent
						->CanReceiveTransferredItemInstanceToPlacement(
							Item,
							Intent.Quantity,
							Candidate))
				{
					return false;
				}

				Intent.TargetPlacement = Candidate;
				return true;
			};

		const FRpgInventoryGridPlacement RequestedPlacement =
			Intent.TargetPlacement;
		Intent.TargetPlacement = FRpgInventoryGridPlacement();
		if (RequestedPlacement.IsValid())
		{
			TryUseConcretePlacement(RequestedPlacement);
		}

		auto TryFindConcretePlacement =
			[&Intent, &TryUseConcretePlacement](
				const FRpgInventoryGridSize& GridSize)
			{
				for (int32 RotationIndex = 0;
					RotationIndex < 2 &&
						!Intent.TargetPlacement.IsValid();
					++RotationIndex)
				{
					for (int32 Y = 0;
						Y < GridSize.Height &&
							!Intent.TargetPlacement.IsValid();
						++Y)
					{
						for (int32 X = 0;
							X < GridSize.Width;
							++X)
						{
							FRpgInventoryGridPlacement Candidate;
							Candidate.SetContainerHandle(
								Intent.TargetContainer);
							Candidate.X = X;
							Candidate.Y = Y;
							Candidate.bRotated =
								RotationIndex == 1;
							if (TryUseConcretePlacement(Candidate))
							{
								break;
							}
						}
					}
				}
			};

		FRpgInventoryGridSize GridSize;
		if (LootInventoryComponent->GetGridSizeForContainerHandle(
				Intent.TargetContainer,
				GridSize))
		{
			TryFindConcretePlacement(GridSize);
		}
		if (!Intent.TargetPlacement.IsValid())
		{
			const FRpgInventoryGridSize ItemSize =
				Intent.ExpectedSourcePlacement.GetUnrotatedSize();
			FRpgInventoryGridSize ExpandedSize;
			ExpandedSize.Width =
				FMath::Max(GridSize.Width, ItemSize.Width);
			ExpandedSize.Height =
				GridSize.Height + FMath::Max(1, ItemSize.Height);
			if (LootInventoryComponent
					->ExpandDefaultGridToMinimum(ExpandedSize) &&
				LootInventoryComponent->GetGridSizeForContainerHandle(
					Intent.TargetContainer,
					GridSize))
			{
				TryFindConcretePlacement(GridSize);
			}
		}
		if (!Intent.TargetPlacement.IsValid())
		{
			Result.Code = ERpgInventoryMutationResultCode::NoSpace;
			return CacheResult(MoveTemp(Result));
		}
	}
	Result = SourceInventory->DropItem(
		LootInventoryComponent,
		Intent);
	if (Result.IsSuccess())
	{
		EnsureDefaultPickupInteractionOption();
		bLootInventoryInitialized = true;
		ForceNetUpdate();
	}
	return CacheResult(MoveTemp(Result));
}

bool ARpgDroppedInventoryActor::AreDropTransferIntentsEquivalent(
	const FRpgInventoryTransferIntent& A,
	const FRpgInventoryTransferIntent& B)
{
	return A.RequestId == B.RequestId && A.ItemId == B.ItemId &&
		A.ExpectedEntryId == B.ExpectedEntryId &&
		A.ExpectedSourcePlacement == B.ExpectedSourcePlacement &&
		A.ExpectedSourceQuantity == B.ExpectedSourceQuantity &&
		A.TargetContainer == B.TargetContainer &&
		A.TargetPlacement == B.TargetPlacement && A.Quantity == B.Quantity;
}

bool ARpgDroppedInventoryActor::TryReplayRecentDropTransfer(
	URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryTransferIntent& Intent,
	bool bPreventStackMerge,
	FRpgInventoryMutationResult& OutResult) const
{
	const FRecentDropTransferResult* CachedResult =
		RecentDropTransferResults.Find(Intent.RequestId);
	if (!CachedResult)
	{
		return false;
	}

	const bool bHadSourceInventory = SourceInventory != nullptr;
	const bool bHadTargetInventory = LootInventoryComponent != nullptr;
	if ((bHadSourceInventory &&
			CachedResult->SourceInventory.Get() == SourceInventory &&
			CachedResult->SourceMutationEpoch !=
				SourceInventory->GetMutationEpoch()) ||
		(bHadTargetInventory &&
			CachedResult->TargetInventory.Get() ==
				LootInventoryComponent &&
			CachedResult->TargetMutationEpoch !=
				LootInventoryComponent->GetMutationEpoch()) ||
		CachedResult->bHadTargetInventory != bHadTargetInventory ||
		(bHadTargetInventory &&
			CachedResult->TargetInventory.Get() !=
				LootInventoryComponent))
	{
		// A successful RestoreInventoryGraph establishes a new command epoch.
		// The same RequestId must be evaluated against that restored state instead
		// of replaying a result produced by either graph's previous history.
		return false;
	}

	if (CachedResult->bHadSourceInventory == bHadSourceInventory &&
		(!bHadSourceInventory ||
			CachedResult->SourceInventory.Get() == SourceInventory) &&
		AreDropTransferIntentsEquivalent(CachedResult->Intent, Intent) &&
		CachedResult->bPreventStackMerge == bPreventStackMerge)
	{
		OutResult = CachedResult->Result;
		return true;
	}

	OutResult = FRpgInventoryMutationResult();
	OutResult.RequestId = Intent.RequestId;
	OutResult.Operation = ERpgInventoryMutationOperation::Drop;
	OutResult.RequestedQuantity = Intent.Quantity;
	OutResult.Code = ERpgInventoryMutationResultCode::InvalidRequest;
	return true;
}

FRpgInventoryMutationResult
ARpgDroppedInventoryActor::CacheRecentDropTransfer(
	URpgInventoryManagerComponent* SourceInventory,
	const FRpgInventoryTransferIntent& Intent,
	bool bPreventStackMerge,
	FRpgInventoryMutationResult Result)
{
	if (!Result.RequestId.IsValid())
	{
		return Result;
	}

	FRecentDropTransferResult& CachedResult =
		RecentDropTransferResults.FindOrAdd(Result.RequestId);
	CachedResult.SourceInventory = SourceInventory;
	CachedResult.bHadSourceInventory = SourceInventory != nullptr;
	CachedResult.SourceMutationEpoch = SourceInventory
		? SourceInventory->GetMutationEpoch()
		: 0;
	CachedResult.TargetInventory = LootInventoryComponent;
	CachedResult.bHadTargetInventory = LootInventoryComponent != nullptr;
	CachedResult.TargetMutationEpoch = LootInventoryComponent
		? LootInventoryComponent->GetMutationEpoch()
		: 0;
	CachedResult.Intent = Intent;
	CachedResult.bPreventStackMerge = bPreventStackMerge;
	CachedResult.Result = Result;
	RecentDropTransferOrder.Remove(Result.RequestId);
	RecentDropTransferOrder.Add(Result.RequestId);
	while (RecentDropTransferOrder.Num() > MaxRecentDropTransferResults)
	{
		RecentDropTransferResults.Remove(RecentDropTransferOrder[0]);
		RecentDropTransferOrder.RemoveAt(
			0,
			1,
			EAllowShrinking::No);
	}
	return Result;
}

bool ARpgDroppedInventoryActor::MergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 StackCount)
{
	if (!HasAuthority() || !ItemDefinition || StackCount <= 0)
	{
		return false;
	}

	EnsureDefaultPickupInteractionOption();
	if (!LootInventoryComponent || !LootInventoryComponent->CanAddItemDefinition(ItemDefinition, StackCount))
	{
		return false;
	}

	if (!LootInventoryComponent->GrantItemDefinition(ItemDefinition, StackCount))
	{
		return false;
	}
	bLootInventoryInitialized = true;
	ForceNetUpdate();
	return true;
}

bool ARpgDroppedInventoryActor::CanMergePickupTemplate(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (!ItemDefinition || !LootInventoryComponent)
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> Entries = LootInventoryComponent->GetAllEntries();
	for (const FRpgInventoryEntryView& Entry : Entries)
	{
		if (!Entry.Instance || Entry.Instance->GetItemDef() != ItemDefinition)
		{
			return false;
		}
	}

	return Entries.IsEmpty() || LootInventoryComponent->CanAddItemDefinition(ItemDefinition, 1);
}

void ARpgDroppedInventoryActor::EnsureDefaultPickupInteractionOption()
{
	if (!Option.InteractionAbilityToGrant)
	{
		Option.InteractionAbilityToGrant = URpgGameplayAbility_Collect::StaticClass();
	}

	if (Option.Text.IsEmpty())
	{
		Option.Text = NSLOCTEXT("RpgInventory", "PickupDroppedInventoryText", "Pick Up");
	}

	if (Option.SubText.IsEmpty())
	{
		Option.SubText = NSLOCTEXT("RpgInventory", "PickupDroppedInventorySubText", "Loot");
	}
}

bool ARpgDroppedInventoryActor::PopulateLootInventoryFromPickup(
	const FInventoryPickup& PickupInventory)
{
	if (!HasAuthority() || !LootInventoryComponent)
	{
		return false;
	}

	for (const FPickupTemplate& Template : PickupInventory.Templates)
	{
		if (!Template.ItemDef || Template.StackCount <= 0 ||
			!LootInventoryComponent->GrantItemDefinition(
				Template.ItemDef,
				Template.StackCount))
		{
			return false;
		}
	}

	for (const FPickupInstance& Instance : PickupInventory.Instances)
	{
		if (!Instance.Item ||
			!LootInventoryComponent->BootstrapItemInstance(Instance.Item))
		{
			return false;
		}
	}
	return true;
}

FInventoryPickup ARpgDroppedInventoryActor::BuildPickupInventoryFromLootInventory() const
{
	FInventoryPickup PickupInventory;
	if (!LootInventoryComponent)
	{
		return PickupInventory;
	}

	for (const FRpgInventoryEntryView& Entry : LootInventoryComponent->GetAllEntries())
	{
		URpgInventoryItemInstance* ItemInstance = Entry.Instance;
		if (!ItemInstance)
		{
			continue;
		}

		if (URpgInventoryManagerComponent::
				GetEffectiveMaxStackSizeForDefinition(
					ItemInstance->GetItemDef()) > 1)
		{
			FPickupTemplate& Template = PickupInventory.Templates.AddDefaulted_GetRef();
			Template.ItemDef = ItemInstance->GetItemDef();
			Template.StackCount = Entry.StackCount;
		}
		else
		{
			FPickupInstance& Instance = PickupInventory.Instances.AddDefaulted_GetRef();
			Instance.Item = ItemInstance;
		}
	}

	return PickupInventory;
}
