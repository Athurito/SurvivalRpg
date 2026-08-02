#include "RpgBaseStorageComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Engine/AssetManager.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "RpgBaseCampActor.h"
#include "RpgBaseStorageDomainAnchorComponent.h"
#include "RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_BaseStorage_Message_Changed, "Rpg.BaseStorage.Message.Changed");

namespace
{
	const URpgInventoryFragment_ItemTraits* GetItemTraits(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())) : nullptr;
	}

	FString GetDisplayNameForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO = ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO ? ItemCDO->DisplayName.ToString() : FString();
	}

	ERpgInventoryItemCategory GetCategoryForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		if (const URpgInventoryFragment_ItemTraits* Traits = GetItemTraits(ItemDef))
		{
			return Traits->ItemCategory;
		}

		return ERpgInventoryItemCategory::Misc;
	}

	const URpgInventoryFragment_StorageProfile* GetStorageProfile(
		TSubclassOf<URpgInventoryItemDefinition> ItemDef)
	{
		const URpgInventoryItemDefinition* ItemCDO =
			ItemDef ? GetDefault<URpgInventoryItemDefinition>(ItemDef) : nullptr;
		return ItemCDO
			? Cast<URpgInventoryFragment_StorageProfile>(
				ItemCDO->FindFragmentByClass(
					URpgInventoryFragment_StorageProfile::StaticClass()))
			: nullptr;
	}
}

TArray<FRpgBaseResourceEntryView> FRpgBaseResourceList::GetAllResources() const
{
	TArray<FRpgBaseResourceEntryView> Results;
	Results.Reserve(Entries.Num());

	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		if (!Entry.ItemDefinition)
		{
			continue;
		}

		FRpgBaseResourceEntryView& View = Results.AddDefaulted_GetRef();
		View.ItemDefinition = Entry.ItemDefinition;
		View.Count = Entry.Count;
		View.Capacity = Entry.Capacity;
		View.SortIndex = Entry.SortIndex;
	}

	Results.Sort([](const FRpgBaseResourceEntryView& A, const FRpgBaseResourceEntryView& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	return Results;
}

int32 FRpgBaseResourceList::GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? Entry->Count : 0;
}

int32 FRpgBaseResourceList::GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? Entry->Capacity : 0;
}

int32 FRpgBaseResourceList::GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	return Entry ? FMath::Max(0, Entry->Capacity - Entry->Count) : 0;
}

bool FRpgBaseResourceList::CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	return ItemDefinition && Count > 0 && GetFreeResourceCapacity(ItemDefinition) >= Count;
}

bool FRpgBaseResourceList::StoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	if (!CanStoreResource(ItemDefinition, Count))
	{
		return false;
	}

	FRpgBaseResourceEntry& Entry = FindOrAddEntry(ItemDefinition);
	const int32 OldCount = Entry.Count;
	const int32 OldCapacity = Entry.Capacity;
	Entry.Count += Count;
	MarkItemDirty(Entry);
	BroadcastChangeMessage(Entry, OldCount, OldCapacity);
	return true;
}

bool FRpgBaseResourceList::WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	FRpgBaseResourceEntry* Entry = FindEntry(ItemDefinition);
	if (!Entry || Count <= 0 || Entry->Count < Count)
	{
		return false;
	}

	const int32 OldCount = Entry->Count;
	const int32 OldCapacity = Entry->Capacity;
	Entry->Count -= Count;
	MarkItemDirty(*Entry);
	BroadcastChangeMessage(*Entry, OldCount, OldCapacity);
	return true;
}

void FRpgBaseResourceList::AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity)
{
	if (!ItemDefinition || DeltaCapacity == 0)
	{
		return;
	}

	FRpgBaseResourceEntry& Entry = FindOrAddEntry(ItemDefinition);
	const int32 OldCount = Entry.Count;
	const int32 OldCapacity = Entry.Capacity;
	Entry.Capacity = FMath::Max(Entry.Count, Entry.Capacity + DeltaCapacity);
	MarkItemDirty(Entry);
	BroadcastChangeMessage(Entry, OldCount, OldCapacity);
}

bool FRpgBaseResourceList::ApplySort(ERpgInventorySortMode SortMode)
{
	if (Entries.Num() <= 1)
	{
		return false;
	}

	TArray<FRpgBaseResourceEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
	}

	switch (SortMode)
	{
	case ERpgInventorySortMode::Manual:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			return A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Name:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Category:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			const int32 CategoryA = static_cast<int32>(GetCategoryForDefinition(A.ItemDefinition));
			const int32 CategoryB = static_cast<int32>(GetCategoryForDefinition(B.ItemDefinition));
			if (CategoryA != CategoryB)
			{
				return CategoryA < CategoryB;
			}

			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::StackCount:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			if (A.Count != B.Count)
			{
				return A.Count > B.Count;
			}

			const int32 NameCompare = GetDisplayNameForDefinition(A.ItemDefinition).Compare(GetDisplayNameForDefinition(B.ItemDefinition), ESearchCase::IgnoreCase);
			return NameCompare != 0 ? NameCompare < 0 : A.SortIndex < B.SortIndex;
		});
		break;

	case ERpgInventorySortMode::Recent:
		SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
		{
			return A.SortIndex > B.SortIndex;
		});
		break;
	}

	return SetOrderFromSortedEntryPointers(SortedEntries);
}

bool FRpgBaseResourceList::MoveResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	if (!ItemDefinition || Entries.Num() <= 0)
	{
		return false;
	}

	TArray<FRpgBaseResourceEntry*> SortedEntries;
	SortedEntries.Reserve(Entries.Num());
	FRpgBaseResourceEntry* MovingEntry = nullptr;
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		SortedEntries.Add(&Entry);
		if (Entry.ItemDefinition == ItemDefinition)
		{
			MovingEntry = &Entry;
		}
	}

	if (!MovingEntry)
	{
		return false;
	}

	SortedEntries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	SortedEntries.Remove(MovingEntry);
	SortedEntries.Insert(MovingEntry, FMath::Clamp(TargetIndex, 0, SortedEntries.Num()));
	return SetOrderFromSortedEntryPointers(SortedEntries);
}

void FRpgBaseResourceList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, Entry.Count, Entry.Capacity);
		Entry.LastObservedCount = 0;
		Entry.LastObservedCapacity = 0;
	}
}

void FRpgBaseResourceList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, 0, 0);
		Entry.LastObservedCount = Entry.Count;
		Entry.LastObservedCapacity = Entry.Capacity;
	}
}

void FRpgBaseResourceList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FRpgBaseResourceEntry& Entry = Entries[Index];
		check(Entry.LastObservedCount != INDEX_NONE);
		check(Entry.LastObservedCapacity != INDEX_NONE);
		BroadcastChangeMessage(Entry, Entry.LastObservedCount, Entry.LastObservedCapacity, Entry.LastObservedCount == Entry.Count && Entry.LastObservedCapacity == Entry.Capacity);
		Entry.LastObservedCount = Entry.Count;
		Entry.LastObservedCapacity = Entry.Capacity;
	}
}

FRpgBaseResourceEntry* FRpgBaseResourceList::FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	for (FRpgBaseResourceEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition == ItemDefinition)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FRpgBaseResourceEntry* FRpgBaseResourceList::FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		if (Entry.ItemDefinition == ItemDefinition)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FRpgBaseResourceEntry& FRpgBaseResourceList::FindOrAddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	if (FRpgBaseResourceEntry* ExistingEntry = FindEntry(ItemDefinition))
	{
		return *ExistingEntry;
	}

	FRpgBaseResourceEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.ItemDefinition = ItemDefinition;
	NewEntry.SortIndex = GetNextSortIndex();
	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, 0, true);
	return NewEntry;
}

void FRpgBaseResourceList::BroadcastChangeMessage(FRpgBaseResourceEntry& Entry, int32 OldCount, int32 OldCapacity, bool bOrderChanged)
{
	if (!OwnerComponent || !OwnerComponent->GetWorld())
	{
		return;
	}
	if (URpgBaseStorageComponent* Storage =
			Cast<URpgBaseStorageComponent>(OwnerComponent);
		Storage && Storage->DeferResourceChangeMessage())
	{
		return;
	}

	FRpgBaseResourceChangeMessage Message;
	Message.StorageOwner = OwnerComponent;
	Message.ItemDefinition = Entry.ItemDefinition;
	Message.NewCount = Entry.Count;
	Message.Delta = Entry.Count - OldCount;
	Message.Capacity = Entry.Capacity;
	Message.SortIndex = Entry.SortIndex;
	Message.bCapacityChanged = Entry.Capacity != OldCapacity;
	Message.bOrderChanged = bOrderChanged;

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(OwnerComponent->GetWorld());
	MessageSubsystem.BroadcastMessage(TAG_Rpg_BaseStorage_Message_Changed, Message);
}

int32 FRpgBaseResourceList::GetNextSortIndex() const
{
	int32 MaxSortIndex = INDEX_NONE;
	for (const FRpgBaseResourceEntry& Entry : Entries)
	{
		MaxSortIndex = FMath::Max(MaxSortIndex, Entry.SortIndex);
	}

	return MaxSortIndex + 1;
}

void FRpgBaseResourceList::SortEntriesBySortIndex()
{
	Entries.Sort([](const FRpgBaseResourceEntry& A, const FRpgBaseResourceEntry& B)
	{
		return A.SortIndex < B.SortIndex;
	});
}

bool FRpgBaseResourceList::SetOrderFromSortedEntryPointers(const TArray<FRpgBaseResourceEntry*>& SortedEntries)
{
	bool bChanged = false;
	for (int32 Index = 0; Index < SortedEntries.Num(); ++Index)
	{
		FRpgBaseResourceEntry* Entry = SortedEntries[Index];
		if (!Entry || Entry->SortIndex == Index)
		{
			continue;
		}

		const int32 OldCount = Entry->Count;
		const int32 OldCapacity = Entry->Capacity;
		Entry->SortIndex = Index;
		MarkItemDirty(*Entry);
		BroadcastChangeMessage(*Entry, OldCount, OldCapacity, true);
		bChanged = true;
	}

	if (bChanged)
	{
		SortEntriesBySortIndex();
		MarkArrayDirty();
	}

	return bChanged;
}

URpgBaseStorageComponent::URpgBaseStorageComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ResourceList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

#if WITH_EDITOR
EDataValidationResult URpgBaseStorageComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const auto AddError =
		[&Context, &Result](const FText& Message)
		{
			Context.AddError(Message);
			Result = EDataValidationResult::Invalid;
		};

	if (BaseMaterialCapacityPoints < 0 || BaseContainmentSlots < 0)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorage",
			"InvalidBaseCapacity",
			"BaseMaterialCapacityPoints and BaseContainmentSlots must be non-negative."));
	}

	if (!bUseSharedMaterialCapacity)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorage",
			"LegacyCapacityModeUnsupported",
			"V2 base storage requires bUseSharedMaterialCapacity so stations cannot create untracked capacity."));
	}

	if (BaseArmoryGridColumns < 1 || BaseArmoryGridRows < 1)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorage",
			"InvalidBaseArmoryDimensions",
			"BaseArmoryGridColumns and BaseArmoryGridRows must each be at least one cell."));
	}

	for (const FGameplayTag& CapabilityTag : BaselineCapabilities)
	{
		if (!CapabilityTag.IsValid() ||
			CapabilityTag == RpgGameplayTags::Storage_Capability ||
			!CapabilityTag.MatchesTag(RpgGameplayTags::Storage_Capability))
		{
			AddError(NSLOCTEXT(
				"RpgBaseStorage",
				"InvalidBaselineCapability",
				"Every BaselineCapabilities entry must be a strict child of Storage.Capability."));
			break;
		}
	}

	if (RiftCleanseCosts.IsEmpty())
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorage",
			"MissingRiftCleanseCosts",
			"RiftCleanseCosts must contain at least one explicit Materials BulkResource cost."));
	}

	TSet<TSubclassOf<URpgInventoryItemDefinition>> SeenCleanseDefinitions;
	for (int32 CostIndex = 0; CostIndex < RiftCleanseCosts.Num(); ++CostIndex)
	{
		const FRpgBaseStorageOperationCost& Cost = RiftCleanseCosts[CostIndex];
		if (!Cost.ItemDefinition || Cost.Count <= 0)
		{
			AddError(FText::FromString(FString::Printf(
				TEXT("RiftCleanseCosts[%d] requires an item definition and a positive count."),
				CostIndex)));
			continue;
		}

		if (SeenCleanseDefinitions.Contains(Cost.ItemDefinition))
		{
			AddError(FText::FromString(FString::Printf(
				TEXT("RiftCleanseCosts[%d] repeats item definition '%s'; combine duplicate rows into one explicit count."),
				CostIndex,
				*GetPathNameSafe(Cost.ItemDefinition.Get()))));
			continue;
		}
		SeenCleanseDefinitions.Add(Cost.ItemDefinition);

		const URpgInventoryFragment_StorageProfile* Profile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				Cost.ItemDefinition);
		if (!Profile || !Profile->IsBulkResource() ||
			Profile->StorageDomainTag !=
				RpgGameplayTags::Storage_Domain_Materials)
		{
			AddError(FText::FromString(FString::Printf(
				TEXT("RiftCleanseCosts[%d] must reference an explicit Storage.Domain.Materials BulkResource definition."),
				CostIndex)));
		}
	}

	if (RiftCleanseAmount < 1 || RiftCleanseAmount > 100)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorage",
			"InvalidRiftCleanseAmount",
			"RiftCleanseAmount must be between 1 and 100 strain points, inclusive."));
	}

	return Result;
}
#endif

void URpgBaseStorageComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	FText IgnoredFailureReason;
	RebuildDerivedUpgradeState(
		InstalledUpgrades, &IgnoredFailureReason, true);

	if (!bUseSharedMaterialCapacity)
	{
		for (const FRpgBaseResourceCapacity& DefaultCapacity : DefaultResourceCapacities)
		{
			AddResourceCapacity(DefaultCapacity.ItemDefinition, DefaultCapacity.Capacity);
		}
	}

	RefreshDerivedResourceCapacities();
}

void URpgBaseStorageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ResourceList);
	DOREPLIFETIME(ThisClass, MaterialCapacityPoints);
	DOREPLIFETIME(ThisClass, InstalledCapabilities);
	DOREPLIFETIME(ThisClass, InstalledUpgrades);
	DOREPLIFETIME(ThisClass, ContainmentSlotCapacity);
	DOREPLIFETIME(ThisClass, ArmoryGridColumns);
	DOREPLIFETIME(ThisClass, ArmoryGridRows);
	DOREPLIFETIME(ThisClass, bArmoryDomainOverCapacity);
	DOREPLIFETIME(ThisClass, bContainmentDomainOverCapacity);
	DOREPLIFETIME(ThisClass, ContainmentStrength);
	DOREPLIFETIME(ThisClass, CorruptionProtection);
	DOREPLIFETIME(ThisClass, PassiveRiftStrain);
	DOREPLIFETIME(ThisClass, RiftStrainMitigation);
	DOREPLIFETIME(ThisClass, RiftStrain);
	DOREPLIFETIME(ThisClass, NetworkRevision);
	DOREPLIFETIME(ThisClass, bMutationTainted);
}

TArray<FRpgBaseResourceEntryView> URpgBaseStorageComponent::GetAllResources() const
{
	TArray<FRpgBaseResourceEntryView> Results = ResourceList.GetAllResources();
	if (!bUseSharedMaterialCapacity)
	{
		return Results;
	}

	const int32 FreePoints = GetFreeMaterialCapacityPoints();
	for (FRpgBaseResourceEntryView& Entry : Results)
	{
		Entry.CapacityCost = FMath::Max(1, GetBulkCapacityCost(Entry.ItemDefinition));
		Entry.Capacity = Entry.Count + (FreePoints / Entry.CapacityCost);
	}
	return Results;
}

int32 URpgBaseStorageComponent::GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	return ResourceList.GetResourceCount(ItemDefinition);
}

int32 URpgBaseStorageComponent::GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (bUseSharedMaterialCapacity)
	{
		const int32 CapacityCost = GetBulkCapacityCost(ItemDefinition);
		return CapacityCost > 0
			? GetResourceCount(ItemDefinition) + (GetFreeMaterialCapacityPoints() / CapacityCost)
			: GetResourceCount(ItemDefinition);
	}
	return ResourceList.GetResourceCapacity(ItemDefinition);
}

int32 URpgBaseStorageComponent::GetUsedMaterialCapacityPoints() const
{
	return static_cast<int32>(FMath::Min<int64>(
		GetUsedMaterialCapacityPoints64(), MAX_int32));
}

int64 URpgBaseStorageComponent::GetUsedMaterialCapacityPoints64() const
{
	int64 UsedPoints = 0;
	for (const FRpgBaseResourceEntry& Entry : ResourceList.Entries)
	{
		if (!Entry.ItemDefinition || Entry.Count <= 0)
		{
			continue;
		}


		// Preserve corrupt/legacy rows as one point per unit instead of silently
		// treating them as free. All arithmetic saturates before multiplication/addition.
		const int64 Count = static_cast<int64>(Entry.Count);
		const int64 CapacityCost = FMath::Max<int64>(
			1, GetBulkCapacityCost(Entry.ItemDefinition));
		if (Count > MAX_int64 / CapacityCost)
		{
			return MAX_int64;
		}
		const int64 EntryPoints = Count * CapacityCost;
		if (UsedPoints > MAX_int64 - EntryPoints)
		{
			return MAX_int64;
		}
		UsedPoints += EntryPoints;
	}
	return UsedPoints;
}

int64 URpgBaseStorageComponent::GetFreeMaterialCapacityPoints64() const
{
	return FMath::Max<int64>(
		0,
		static_cast<int64>(MaterialCapacityPoints) -
			GetUsedMaterialCapacityPoints64());
}

int32 URpgBaseStorageComponent::GetFreeMaterialCapacityPoints() const
{
	return static_cast<int32>(FMath::Min<int64>(
		GetFreeMaterialCapacityPoints64(), MAX_int32));
}

bool URpgBaseStorageComponent::IsMaterialDomainOverCapacity() const
{
	return GetUsedMaterialCapacityPoints64() >
		static_cast<int64>(MaterialCapacityPoints);
}

int32 URpgBaseStorageComponent::GetBulkCapacityCost(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	const URpgInventoryFragment_StorageProfile* Profile =
		GetStorageProfile(ItemDefinition);
	return Profile && Profile->CanDepositAsBulk()
		? FMath::Max(1, Profile->BulkCapacityCost)
		: 0;
}

int32 URpgBaseStorageComponent::GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (bMutationTainted)
	{
		return 0;
	}
	if (bUseSharedMaterialCapacity)
	{
		const int32 CapacityCost = GetBulkCapacityCost(ItemDefinition);
		return CapacityCost > 0 ? GetFreeMaterialCapacityPoints() / CapacityCost : 0;
	}
	return ResourceList.GetFreeResourceCapacity(ItemDefinition);
}

bool URpgBaseStorageComponent::CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const
{
	return CanStoreResourceDefinition(ItemDefinition, Count);
}

bool URpgBaseStorageComponent::CanStoreResourceDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 Count) const
{
	if (bMutationTainted || !ItemDefinition || Count <= 0)
	{
		return false;
	}
	if (!CanManuallyDepositBulk(ItemDefinition))
	{
		return false;
	}

	if (!bUseSharedMaterialCapacity)
	{
		return ResourceList.CanStoreResource(ItemDefinition, Count);
	}

	const int32 CapacityCost = GetBulkCapacityCost(ItemDefinition);
	return CapacityCost > 0 &&
		static_cast<int64>(Count) * CapacityCost <=
			GetFreeMaterialCapacityPoints64();
}

bool URpgBaseStorageComponent::CanManuallyDepositBulk(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (bMutationTainted)
	{
		return false;
	}
	const URpgInventoryFragment_StorageProfile* Profile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			ItemDefinition);
	return Profile &&
		Profile->StorageDomainTag ==
			RpgGameplayTags::Storage_Domain_Materials &&
		Profile->CanDepositAsBulkWithCapabilities(InstalledCapabilities);
}

bool URpgBaseStorageComponent::CanAutoDepositBulk(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (bMutationTainted)
	{
		return false;
	}
	const URpgInventoryFragment_StorageProfile* Profile =
		URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
			ItemDefinition);
	return Profile &&
		Profile->StorageDomainTag ==
			RpgGameplayTags::Storage_Domain_Materials &&
		Profile->CanAutoDeposit(InstalledCapabilities);
}

bool URpgBaseStorageComponent::CanCraftFromNetwork(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const
{
	if (bMutationTainted)
	{
		return false;
	}
	const URpgInventoryFragment_StorageProfile* Profile =
		GetStorageProfile(ItemDefinition);
	const FGameplayTag CraftCapability = FGameplayTag::RequestGameplayTag(
		TEXT("Storage.Capability.CraftFromNetwork"), false);
	return Profile &&
		Profile->CanDepositAsBulkWithCapabilities(InstalledCapabilities) &&
		URpgInventoryFragment_StorageProfile::
			IsDefinitionIntrinsicallyCollapsible(ItemDefinition) &&
		Profile->StorageDomainTag ==
			RpgGameplayTags::Storage_Domain_Materials &&
		Profile->bCanCraftFromNetwork &&
		InstalledCapabilities.HasAllExact(
			Profile->RequiredStorageCapabilityTags) &&
		CraftCapability.IsValid() &&
		InstalledCapabilities.HasTagExact(CraftCapability);
}

bool URpgBaseStorageComponent::CanStoreResourceInstance(
	const URpgInventoryItemInstance* Item,
	int32 Count) const
{
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item ? Item->GetItemDef() : nullptr;
	return Item && Item->CanCollapseIntoDefinitionCount() &&
		CanStoreResourceDefinition(ItemDefinition, Count);
}

bool URpgBaseStorageComponent::StoreDefinitionResource(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted)
	{
		return false;
	}

	if (!CanStoreResourceDefinition(ItemDefinition, Count))
	{
		return false;
	}

	if (bUseSharedMaterialCapacity)
	{
		FRpgBaseResourceEntry& Entry = ResourceList.FindOrAddEntry(ItemDefinition);
		Entry.Capacity = Entry.Count + GetFreeResourceCapacity(ItemDefinition);
	}
	const bool bStored = ResourceList.StoreResource(ItemDefinition, Count);
	if (bStored)
	{
		RefreshDerivedResourceCapacities();
		MarkStorageStateDirty();
	}
	return bStored;
}

bool URpgBaseStorageComponent::StoreResourceInstance(
	const URpgInventoryItemInstance* Item,
	int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted ||
		!CanStoreResourceInstance(Item, Count))
	{
		return false;
	}

	if (bUseSharedMaterialCapacity)
	{
		FRpgBaseResourceEntry& Entry =
			ResourceList.FindOrAddEntry(Item->GetItemDef());
		Entry.Capacity = Entry.Count + GetFreeResourceCapacity(Item->GetItemDef());
	}
	const bool bStored = ResourceList.StoreResource(Item->GetItemDef(), Count);
	if (bStored)
	{
		RefreshDerivedResourceCapacities();
		MarkStorageStateDirty();
	}
	return bStored;
}

bool URpgBaseStorageComponent::CaptureResourceMutationCheckpoint(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	FRpgBaseResourceMutationCheckpoint& OutCheckpoint) const
{
	OutCheckpoint = FRpgBaseResourceMutationCheckpoint();
	if (!ItemDefinition)
	{
		return false;
	}

	OutCheckpoint.ItemDefinition = ItemDefinition;
	if (const FRpgBaseResourceEntry* Entry =
		ResourceList.FindEntry(ItemDefinition))
	{
		OutCheckpoint.bHadEntry = true;
		OutCheckpoint.Entry = *Entry;
	}
	return true;
}

bool URpgBaseStorageComponent::RestoreResourceMutationCheckpoint(
	const FRpgBaseResourceMutationCheckpoint& Checkpoint)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!Checkpoint.ItemDefinition)
	{
		return false;
	}

	FRpgBaseResourceEntry* Current =
		ResourceList.FindEntry(Checkpoint.ItemDefinition);
	const int32 OldCount = Current ? Current->Count : 0;
	const int32 OldCapacity = Current ? Current->Capacity : 0;
	const int32 OldSortIndex = Current ? Current->SortIndex : INDEX_NONE;
	if (Checkpoint.bHadEntry)
	{
		if (Current)
		{
			*Current = Checkpoint.Entry;
		}
		else
		{
			Current = &ResourceList.Entries.Add_GetRef(
				Checkpoint.Entry);
		}
		ResourceList.MarkItemDirty(*Current);
		ResourceList.SortEntriesBySortIndex();
		Current = ResourceList.FindEntry(Checkpoint.ItemDefinition);
		if (!Current)
		{
			return false;
		}
		ResourceList.BroadcastChangeMessage(
			*Current,
			OldCount,
			OldCapacity,
			OldSortIndex != Current->SortIndex);
	}
	else if (Current)
	{
		FRpgBaseResourceEntry RemovedSnapshot = *Current;
		RemovedSnapshot.Count = 0;
		RemovedSnapshot.Capacity = 0;
		ResourceList.Entries.RemoveAll(
			[&Checkpoint](const FRpgBaseResourceEntry& Candidate)
			{
				return Candidate.ItemDefinition ==
					Checkpoint.ItemDefinition;
			});
		ResourceList.MarkArrayDirty();
		ResourceList.BroadcastChangeMessage(
			RemovedSnapshot,
			OldCount,
			OldCapacity,
			true);
	}

	RefreshDerivedResourceCapacities();
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted)
	{
		return false;
	}

	const bool bWithdrawn = ResourceList.WithdrawResource(ItemDefinition, Count);
	if (bWithdrawn)
	{
		RefreshDerivedResourceCapacities();
		MarkStorageStateDirty();
	}
	return bWithdrawn;
}

void URpgBaseStorageComponent::AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted)
	{
		return;
	}

	if (!bUseSharedMaterialCapacity)
	{
		ResourceList.AddResourceCapacity(ItemDefinition, DeltaCapacity);
		MarkStorageStateDirty();
	}
}

bool URpgBaseStorageComponent::AddMaterialCapacityPoints(int32 DeltaCapacityPoints)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted ||
		DeltaCapacityPoints == 0)
	{
		return false;
	}

	const int64 ProposedCapacity =
		static_cast<int64>(MaterialCapacityPoints) + DeltaCapacityPoints;
	if (ProposedCapacity < 0 || ProposedCapacity > MAX_int32 ||
		ProposedCapacity < GetUsedMaterialCapacityPoints64())
	{
		return false;
	}

	MaterialCapacityPoints = static_cast<int32>(ProposedCapacity);
	RefreshDerivedResourceCapacities();
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::HasInstalledCapability(FGameplayTag CapabilityTag) const
{
	return CapabilityTag.IsValid() && InstalledCapabilities.HasTagExact(CapabilityTag);
}

TArray<URpgBaseStorageUpgradeDefinition*>
URpgBaseStorageComponent::GetInstalledUpgrades() const
{
	TArray<URpgBaseStorageUpgradeDefinition*> Results;
	Results.Reserve(InstalledUpgrades.Num());
	for (URpgBaseStorageUpgradeDefinition* Upgrade : InstalledUpgrades)
	{
		if (Upgrade)
		{
			Results.Add(Upgrade);
		}
	}
	return Results;
}

bool URpgBaseStorageComponent::HasInstalledUpgrade(
	const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const
{
	return UpgradeDefinition && InstalledUpgrades.Contains(UpgradeDefinition);
}

bool URpgBaseStorageComponent::CanInstallUpgrade(
	const URpgBaseStorageUpgradeDefinition* UpgradeDefinition,
	FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (bMutationTainted)
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeMutationTainted",
			"The storage network is locked because an atomic rollback failed.");
		return false;
	}
	if (!UpgradeDefinition || HasInstalledUpgrade(UpgradeDefinition))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeInvalidOrInstalled",
			"This upgrade is invalid or already installed.");
		return false;
	}
	if (!InstalledCapabilities.HasAllExact(
			UpgradeDefinition->RequiredInstalledCapabilityTags))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeCapabilityMissing",
			"A required storage capability is missing.");
		return false;
	}

	const UWorld* World = GetWorld();
	const ARpgGameStateBase* GameState =
		World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
	const URpgWorldStorageKnowledgeComponent* Knowledge = GameState
		? GameState->GetWorldStorageKnowledgeComponent() : nullptr;
	if (!UpgradeDefinition->RequiredKnowledgeTags.IsEmpty() &&
		(!Knowledge || !Knowledge->HasAllKnowledgeTags(
			UpgradeDefinition->RequiredKnowledgeTags)))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeKnowledgeMissing",
			"The world has not discovered the required storage knowledge.");
		return false;
	}

	if (!UpgradeDefinition->TargetDomainTag.IsValid() ||
		UpgradeDefinition->TargetAnchorId.IsNone())
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeTargetMissing",
			"Storage-network upgrades require an explicit domain and anchor.");
		return false;
	}

	bool bFoundAnchor = false;
	if (const AActor* OwnerActor = GetOwner())
	{
		TInlineComponentArray<URpgBaseStorageDomainAnchorComponent*> Anchors;
		OwnerActor->GetComponents(Anchors);
		for (const URpgBaseStorageDomainAnchorComponent* Anchor : Anchors)
		{
			if (Anchor && Anchor->GetAnchorId() == UpgradeDefinition->TargetAnchorId &&
				Anchor->GetDomainTag() == UpgradeDefinition->TargetDomainTag)
			{
				bFoundAnchor = true;
				break;
			}
		}
	}
	if (!bFoundAnchor)
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "UpgradeAnchorMissing",
			"The targeted storage-domain anchor is missing or belongs to another domain.");
		return false;
	}

	return true;
}

bool URpgBaseStorageComponent::InstallUpgrade(
	URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	AActor* OwnerActor = GetOwner();
	FText FailureReason;
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!CanInstallUpgrade(UpgradeDefinition, FailureReason))
	{
		return false;
	}

	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> Candidate =
		InstalledUpgrades;
	Candidate.Add(UpgradeDefinition);
	if (!RebuildDerivedUpgradeState(Candidate, &FailureReason))
	{
		return false;
	}
	InstalledUpgrades = MoveTemp(Candidate);
	RefreshDerivedResourceCapacities();
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::CanDecommissionUpgrade(
	const URpgBaseStorageUpgradeDefinition* UpgradeDefinition,
	FText& OutFailureReason) const
{
	OutFailureReason = FText::GetEmpty();
	if (bMutationTainted)
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionMutationTainted",
			"The storage network is locked because an atomic rollback failed.");
		return false;
	}
	if (!UpgradeDefinition || !InstalledUpgrades.Contains(UpgradeDefinition))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionNotInstalled",
			"This upgrade is not installed.");
		return false;
	}

	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> CandidateUpgrades =
		InstalledUpgrades;
	CandidateUpgrades.RemoveAll(
		[UpgradeDefinition](
			const TObjectPtr<URpgBaseStorageUpgradeDefinition>& Candidate)
		{
			return Candidate.Get() == UpgradeDefinition;
		});
	FGameplayTagContainer CandidateCapabilities =
		BuildEffectiveBaselineCapabilities();
	for (const URpgBaseStorageUpgradeDefinition* Candidate : CandidateUpgrades)
	{
		if (Candidate)
		{
			CandidateCapabilities.AppendTags(
				Candidate->GrantedCapabilityTags);
		}
	}
	if (!ValidateUpgradeSetRequirements(
			CandidateUpgrades,
			CandidateCapabilities,
			&OutFailureReason))
	{
		return false;
	}

	int64 ProposedMaterialCapacity = FMath::Max(0, BaseMaterialCapacityPoints);
	int64 ProposedContainmentSlots = FMath::Max(0, BaseContainmentSlots);
	int64 ProposedArmoryColumns = FMath::Max(1, BaseArmoryGridColumns);
	int64 ProposedArmoryRows = FMath::Max(1, BaseArmoryGridRows);
	for (const URpgBaseStorageUpgradeDefinition* Installed : CandidateUpgrades)
	{
		if (!Installed)
		{
			continue;
		}
		const FString Domain = Installed->TargetDomainTag.ToString();
		if (Domain == TEXT("Storage.Domain.Material") ||
			Domain == TEXT("Storage.Domain.Materials"))
		{
			ProposedMaterialCapacity += Installed->CapacityEffect.AdditionalCapacity;
		}
		if (Domain == TEXT("Storage.Domain.RiftContainment"))
		{
			ProposedContainmentSlots +=
				Installed->ContainmentEffect.AdditionalSealedSlots;
		}
		if (Domain == TEXT("Storage.Domain.Armory"))
		{
			ProposedArmoryColumns += Installed->GridEffect.AdditionalColumns;
			ProposedArmoryRows += Installed->GridEffect.AdditionalRows;
		}
	}
	const bool bProposedArmoryDimensionsSupported =
		ProposedArmoryColumns <= MAX_int32 &&
		ProposedArmoryRows <= MAX_int32;
	const int64 ProposedArmoryEntryBudget =
		bProposedArmoryDimensionsSupported
			? ProposedArmoryColumns * ProposedArmoryRows
			: MAX_int64;
	if (ProposedMaterialCapacity > MAX_int32 ||
		ProposedContainmentSlots > MAX_int32 ||
		ProposedArmoryColumns > MAX_int32 ||
		ProposedArmoryRows > MAX_int32 ||
		ProposedArmoryEntryBudget > MAX_int32)
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionCapacityOverflow",
			"The remaining upgrade set exceeds the supported capacity range.");
		return false;
	}
	if (ProposedMaterialCapacity < GetUsedMaterialCapacityPoints64())
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionMaterialsDoNotFit",
			"Stored materials would exceed the remaining domain capacity.");
		return false;
	}
	const ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(GetOwner());
	const URpgInventoryManagerComponent* Containment = BaseCamp
		? BaseCamp->GetContainmentInventoryComponent() : nullptr;
	if (Containment && ProposedContainmentSlots < Containment->GetUsedEntryCount())
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionContainmentDoesNotFit",
			"Contained Rift items would lose their sealed slots.");
		return false;
	}
	FRpgInventoryGridSize ProposedContainmentSize;
	ProposedContainmentSize.Width = FMath::Max(
		1, static_cast<int32>(ProposedContainmentSlots));
	ProposedContainmentSize.Height = 1;
	if (Containment && !Containment->CanSetDefaultGridSize(ProposedContainmentSize))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionContainmentPlacementDoesNotFit",
			"A contained Rift item occupies a slot that would be removed.");
		return false;
	}
	const URpgInventoryManagerComponent* Armory = BaseCamp
		? BaseCamp->GetArmoryInventoryComponent() : nullptr;
	FRpgInventoryGridSize ProposedArmorySize;
	ProposedArmorySize.Width = static_cast<int32>(ProposedArmoryColumns);
	ProposedArmorySize.Height = static_cast<int32>(ProposedArmoryRows);
	if (Armory && !Armory->CanSetDefaultGridSize(ProposedArmorySize))
	{
		OutFailureReason = NSLOCTEXT(
			"RpgBaseStorage", "DecommissionArmoryDoesNotFit",
			"Armory placements would be clipped by the remaining grid size.");
		return false;
	}
	return true;
}

bool URpgBaseStorageComponent::DecommissionUpgrade(
	URpgBaseStorageUpgradeDefinition* UpgradeDefinition)
{
	AActor* OwnerActor = GetOwner();
	FText FailureReason;
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!CanDecommissionUpgrade(UpgradeDefinition, FailureReason))
	{
		return false;
	}

	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> Candidate =
		InstalledUpgrades;
	Candidate.RemoveAll(
		[UpgradeDefinition](
			const TObjectPtr<URpgBaseStorageUpgradeDefinition>& Installed)
		{
			return Installed.Get() == UpgradeDefinition;
		});
	if (!RebuildDerivedUpgradeState(Candidate, &FailureReason))
	{
		return false;
	}
	InstalledUpgrades = MoveTemp(Candidate);
	RefreshDerivedResourceCapacities();
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::TryAddRiftStrain(int32 StrainDelta)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted ||
		StrainDelta <= 0)
	{
		return false;
	}

	const int32 AppliedStrain =
		GetMitigatedRiftStrainDelta(StrainDelta);
	if (AppliedStrain <= 0)
	{
		return true;
	}
	if (AppliedStrain > 100 - GetRiftStrain())
	{
		return false;
	}

	RiftStrain += AppliedStrain;
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::CleanseRiftStrain(int32 CleanseAmount)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted ||
		CleanseAmount <= 0 ||
		RiftStrain <= 0)
	{
		return false;
	}

	RiftStrain = FMath::Max(0, RiftStrain - CleanseAmount);
	MarkStorageStateDirty();
	return true;
}

bool URpgBaseStorageComponent::RestoreRiftStrainCheckpoint(
	int32 PreviousCleanseableStrain)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		PreviousCleanseableStrain < 0 || PreviousCleanseableStrain > 100)
	{
		return false;
	}

	RiftStrain = PreviousCleanseableStrain;
	MarkStorageStateDirty();
	return true;
}

void URpgBaseStorageComponent::ExportStorageState(
	FRpgBaseStorageSaveData& OutSaveData) const
{
	OutSaveData.BulkEntries.Reset();
	for (const FRpgBaseResourceEntry& Entry : ResourceList.Entries)
	{
		if (!Entry.ItemDefinition || Entry.Count <= 0)
		{
			continue;
		}

		FRpgBaseStorageBulkSaveEntry& SavedEntry =
			OutSaveData.BulkEntries.AddDefaulted_GetRef();
		SavedEntry.ItemDefinition = Entry.ItemDefinition;
		SavedEntry.Count = Entry.Count;
		SavedEntry.SortIndex = Entry.SortIndex;
	}
	OutSaveData.InstalledCapabilities = InstalledCapabilities;
	OutSaveData.RiftStrain = RiftStrain;
	OutSaveData.InstalledUpgrades.Reset();
	for (const URpgBaseStorageUpgradeDefinition* Upgrade : InstalledUpgrades)
	{
		if (!Upgrade || !Upgrade->GetPrimaryAssetId().IsValid())
		{
			continue;
		}
		FRpgBaseStorageInstalledUpgradeSaveData& SavedUpgrade =
			OutSaveData.InstalledUpgrades.AddDefaulted_GetRef();
		SavedUpgrade.UpgradeId = Upgrade->GetPrimaryAssetId();
		SavedUpgrade.AnchorId = Upgrade->TargetAnchorId;
	}
}

bool URpgBaseStorageComponent::RestoreStorageState(
	const FRpgBaseStorageSaveData& SaveData,
	FString& OutError)
{
	OutError.Reset();
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		OutError = TEXT("Base storage restore requires authority.");
		return false;
	}

	TArray<FRpgBaseResourceEntry> StagedEntries;
	StagedEntries.Reserve(SaveData.BulkEntries.Num());
	TSet<TSubclassOf<URpgInventoryItemDefinition>> SeenDefinitions;
	for (const FRpgBaseStorageBulkSaveEntry& SavedEntry : SaveData.BulkEntries)
	{
		UClass* LoadedClass = SavedEntry.ItemDefinition.LoadSynchronous();
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = LoadedClass;
		if (!ItemDefinition || SavedEntry.Count <= 0 ||
			SeenDefinitions.Contains(ItemDefinition) ||
			GetBulkCapacityCost(ItemDefinition) <= 0)
		{
			OutError = TEXT("Base storage save contains an invalid, duplicate, or non-bulk material definition.");
			return false;
		}

		SeenDefinitions.Add(ItemDefinition);
		FRpgBaseResourceEntry& Entry = StagedEntries.AddDefaulted_GetRef();
		Entry.ItemDefinition = ItemDefinition;
		Entry.Count = SavedEntry.Count;
		Entry.SortIndex = SavedEntry.SortIndex;
	}

	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> StagedUpgrades;
	TSet<FPrimaryAssetId> SeenUpgradeIds;
	for (const FRpgBaseStorageInstalledUpgradeSaveData& SavedUpgrade :
		SaveData.InstalledUpgrades)
	{
		URpgBaseStorageUpgradeDefinition* Upgrade =
			ResolveUpgradeDefinition(SavedUpgrade.UpgradeId);
		if (!Upgrade || SeenUpgradeIds.Contains(SavedUpgrade.UpgradeId) ||
			SavedUpgrade.AnchorId.IsNone() ||
			Upgrade->TargetAnchorId != SavedUpgrade.AnchorId ||
			!Upgrade->TargetDomainTag.IsValid())
		{
			OutError = TEXT("Base storage save references a missing, duplicate, or anchor-mismatched upgrade asset.");
			return false;
		}
		SeenUpgradeIds.Add(SavedUpgrade.UpgradeId);
		StagedUpgrades.Add(Upgrade);
	}

	const TArray<FRpgBaseResourceEntry> PreviousEntries =
		ResourceList.Entries;
	const int32 PreviousMaterialCapacityPoints = MaterialCapacityPoints;
	const int32 PreviousContainmentSlotCapacity = ContainmentSlotCapacity;
	const int32 PreviousArmoryGridColumns = ArmoryGridColumns;
	const int32 PreviousArmoryGridRows = ArmoryGridRows;
	const float PreviousContainmentStrength = ContainmentStrength;
	const float PreviousCorruptionProtection = CorruptionProtection;
	const int32 PreviousPassiveRiftStrain = PassiveRiftStrain;
	const int32 PreviousRiftStrainMitigation = RiftStrainMitigation;
	const FGameplayTagContainer PreviousCapabilities =
		InstalledCapabilities;
	const bool bPreviousArmoryOverCapacity = bArmoryDomainOverCapacity;
	const bool bPreviousContainmentOverCapacity =
		bContainmentDomainOverCapacity;
	const int32 PreviousRiftStrain = RiftStrain;
	ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(OwnerActor);
	URpgInventoryManagerComponent* Armory = BaseCamp
		? BaseCamp->GetArmoryInventoryComponent()
		: nullptr;
	URpgInventoryManagerComponent* Containment = BaseCamp
		? BaseCamp->GetContainmentInventoryComponent()
		: nullptr;
	const FRpgInventoryGridSize PreviousArmoryGrid = Armory
		? Armory->GetDefaultGridSize()
		: FRpgInventoryGridSize();
	const FRpgInventoryGridSize PreviousContainmentGrid = Containment
		? Containment->GetDefaultGridSize()
		: FRpgInventoryGridSize();
	const int32 PreviousArmoryEntryBudget = Armory
		? Armory->GetFixedMaxEntries()
		: 0;
	const int32 PreviousContainmentEntryBudget = Containment
		? Containment->GetFixedMaxEntries()
		: 0;
	auto RestorePreviousState = [&]()
	{
		ResourceList.Entries = PreviousEntries;
		ResourceList.MarkArrayDirty();
		MaterialCapacityPoints = PreviousMaterialCapacityPoints;
		ContainmentSlotCapacity = PreviousContainmentSlotCapacity;
		ArmoryGridColumns = PreviousArmoryGridColumns;
		ArmoryGridRows = PreviousArmoryGridRows;
		ContainmentStrength = PreviousContainmentStrength;
		CorruptionProtection = PreviousCorruptionProtection;
		PassiveRiftStrain = PreviousPassiveRiftStrain;
		RiftStrainMitigation = PreviousRiftStrainMitigation;
		InstalledCapabilities = PreviousCapabilities;
		bArmoryDomainOverCapacity = bPreviousArmoryOverCapacity;
		bContainmentDomainOverCapacity =
			bPreviousContainmentOverCapacity;
		RiftStrain = PreviousRiftStrain;
		bool bRestored = true;
		if (Armory)
		{
			bRestored = Armory->SetDefaultGridSize(
				PreviousArmoryGrid) && bRestored;
			Armory->SetFixedMaxEntries(PreviousArmoryEntryBudget);
		}
		if (Containment)
		{
			bRestored = Containment->SetDefaultGridSize(
				PreviousContainmentGrid) && bRestored;
			Containment->SetFixedMaxEntries(
				PreviousContainmentEntryBudget);
		}
		return bRestored;
	};

	ResourceList.Entries = MoveTemp(StagedEntries);
	ResourceList.MarkArrayDirty();
	FText UpgradeFailureReason;
	if (!RebuildDerivedUpgradeState(
			StagedUpgrades, &UpgradeFailureReason, true))
	{
		OutError = UpgradeFailureReason.ToString();
		if (!RestorePreviousState())
		{
			OutError += TEXT(" Component rollback also failed.");
		}
		return false;
	}
	InstalledUpgrades = MoveTemp(StagedUpgrades);
	RiftStrain = FMath::Clamp(SaveData.RiftStrain, 0, 100);
	RefreshDerivedResourceCapacities();
	bMutationTainted = false;
	ResetCommandEpochAfterRestore();
	MarkStorageStateDirty();
	return true;
}

void URpgBaseStorageComponent::RefreshConcreteDomainOverCapacityState()
{
	AActor* OwnerActor = GetOwner();
	ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(OwnerActor);
	if (!OwnerActor || !OwnerActor->HasAuthority() || !BaseCamp)
	{
		return;
	}
	if (bConcreteDomainCapacityRefreshInProgress)
	{
		return;
	}
	TGuardValue<bool> RefreshGuard(
		bConcreteDomainCapacityRefreshInProgress, true);

	URpgInventoryManagerComponent* Armory =
		BaseCamp->GetArmoryInventoryComponent();
	URpgInventoryManagerComponent* Containment =
		BaseCamp->GetContainmentInventoryComponent();
	FRpgInventoryGridSize ConfiguredArmorySize;
	ConfiguredArmorySize.Width = ArmoryGridColumns;
	ConfiguredArmorySize.Height = ArmoryGridRows;
	FRpgInventoryGridSize ConfiguredContainmentSize;
	ConfiguredContainmentSize.Width = FMath::Max(
		1, ContainmentSlotCapacity);
	ConfiguredContainmentSize.Height = 1;

	// A rebalanced save may temporarily expand the concrete grid to preserve
	// placements. Once all placements fit again, restore the exact derived grid
	// and entry budget instead of leaving the domain sticky-overfull.
	bool bNeedsDeferredRetry = false;
	if (Armory && Armory->CanSetDefaultGridSize(ConfiguredArmorySize))
	{
		if (Armory->SetDefaultGridSize(ConfiguredArmorySize))
		{
			Armory->SetFixedMaxEntries(ArmoryGridColumns * ArmoryGridRows);
		}
		else
		{
			bNeedsDeferredRetry = true;
		}
	}
	if (Containment &&
		Containment->CanSetDefaultGridSize(ConfiguredContainmentSize))
	{
		if (Containment->SetDefaultGridSize(ConfiguredContainmentSize))
		{
			Containment->SetFixedMaxEntries(ContainmentSlotCapacity);
		}
		else
		{
			bNeedsDeferredRetry = true;
		}
	}

	const FRpgInventoryGridSize ArmorySize = Armory
		? Armory->GetDefaultGridSize() : FRpgInventoryGridSize();
	const FRpgInventoryGridSize ContainmentSize = Containment
		? Containment->GetDefaultGridSize() : FRpgInventoryGridSize();
	const int64 ArmoryBudget =
		static_cast<int64>(ArmoryGridColumns) * ArmoryGridRows;
	const bool bNewArmoryOverCapacity = Armory &&
		(Armory->GetUsedEntryCount() > ArmoryBudget ||
		 ArmorySize.Width > ArmoryGridColumns ||
		 ArmorySize.Height > ArmoryGridRows);
	const bool bNewContainmentOverCapacity = Containment &&
		(Containment->GetUsedEntryCount() > ContainmentSlotCapacity ||
		 ContainmentSize.Width > FMath::Max(1, ContainmentSlotCapacity));
	if (bArmoryDomainOverCapacity != bNewArmoryOverCapacity ||
		bContainmentDomainOverCapacity != bNewContainmentOverCapacity)
	{
		bArmoryDomainOverCapacity = bNewArmoryOverCapacity;
		bContainmentDomainOverCapacity = bNewContainmentOverCapacity;
		OwnerActor->ForceNetUpdate();
	}
	if (bNeedsDeferredRetry)
	{
		ScheduleConcreteDomainCapacityRefresh();
	}
}

void URpgBaseStorageComponent::ScheduleConcreteDomainCapacityRefresh()
{
	if (bConcreteDomainCapacityRefreshPending || !GetWorld())
	{
		return;
	}

	bConcreteDomainCapacityRefreshPending = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(
			this,
			[this]()
			{
				bConcreteDomainCapacityRefreshPending = false;
				RefreshConcreteDomainOverCapacityState();
				if (ARpgBaseCampActor* BaseCamp =
						Cast<ARpgBaseCampActor>(GetOwner()))
				{
					BaseCamp->RefreshStorageAnchorVisuals();
				}
			}));
}

bool URpgBaseStorageComponent::AdmitCommand(
	const FRpgBaseStorageRequestContext& Context,
	uint32 PayloadHash,
	APlayerController* RequestingController,
	FRpgBaseStorageCommandResult& OutResult)
{
	OutResult = FRpgBaseStorageCommandResult();
	OutResult.RequestId = Context.RequestId;
	OutResult.BaseId = Context.BaseId;
	OutResult.NetworkRevision = NetworkRevision;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() ||
		!RequestingController ||
		!Context.RequestId.IsValid() || Context.BaseId.IsNone() ||
		Context.BaseId != GetOwningBaseId())
	{
		OutResult.Code = ERpgBaseStorageResultCode::InvalidRequest;
		return false;
	}

	if (const FRecentCommandResult* Cached =
			RecentCommandResults.Find(Context.RequestId))
	{
		if (Cached->CommandEpoch == CommandEpoch &&
			Cached->PayloadHash == PayloadHash &&
			Cached->RequestingController == RequestingController &&
			Cached->Result.BaseId == Context.BaseId)
		{
			OutResult = Cached->Result;
		}
		else
		{
			OutResult.Code = ERpgBaseStorageResultCode::Conflict;
		}
		return false;
	}
	if (bMutationTainted)
	{
		OutResult.Code = ERpgBaseStorageResultCode::InternalRollback;
		CacheCommandResult(
			Context.RequestId,
			PayloadHash,
			RequestingController,
			OutResult);
		return false;
	}
	if (ActiveCommandRequestId.IsValid())
	{
		OutResult.Code = ERpgBaseStorageResultCode::Conflict;
		return false;
	}

	if (Context.ExpectedNetworkRevision != INDEX_NONE &&
		Context.ExpectedNetworkRevision != NetworkRevision)
	{
		OutResult.Code = ERpgBaseStorageResultCode::Stale;
		CacheCommandResult(
			Context.RequestId,
			PayloadHash,
			RequestingController,
			OutResult);
		return false;
	}

	AdmittedCommandRequesters.Add(
		Context.RequestId,
		RequestingController);
	ActiveCommandRequestId = Context.RequestId;
	CommandResourceEntriesBefore = ResourceList.Entries;
	bCommandStorageStateDirty = false;
	bCommandResourceMessagesDeferred = false;
	return true;
}

FRpgBaseStorageCommandResult URpgBaseStorageComponent::CompleteCommand(
	const FRpgBaseStorageRequestContext& Context,
	uint32 PayloadHash,
	ERpgBaseStorageResultCode Code,
	int32 RequestedCount,
	int32 AppliedCount,
	const TArray<FRpgBaseStorageResourceCommandOutcome>& ResourceOutcomes)
{
	FRpgBaseStorageCommandResult Result;
	Result.RequestId = Context.RequestId;
	Result.BaseId = Context.BaseId;
	Result.Code = Code;
	Result.RequestedCount = RequestedCount;
	Result.AppliedCount = AppliedCount;
	Result.ResourceOutcomes = ResourceOutcomes;
	return CompleteDetailedCommand(Context, PayloadHash, MoveTemp(Result));
}

FRpgBaseStorageCommandResult URpgBaseStorageComponent::CompleteDetailedCommand(
	const FRpgBaseStorageRequestContext& Context,
	uint32 PayloadHash,
	FRpgBaseStorageCommandResult Result)
{
	const bool bOwnsActiveCommand =
		ActiveCommandRequestId == Context.RequestId;
	if (bOwnsActiveCommand &&
		Result.Code == ERpgBaseStorageResultCode::InternalRollback)
	{
		TaintAfterRollbackFailure();
		bCommandStorageStateDirty = true;
	}
	const bool bStorageStateChanged =
		bOwnsActiveCommand && bCommandStorageStateDirty;
	if (bOwnsActiveCommand)
	{
		ActiveCommandRequestId.Invalidate();
		bCommandStorageStateDirty = false;
	}
	if (bStorageStateChanged)
	{
		MarkStorageStateDirty();
	}
	Result.RequestId = Context.RequestId;
	Result.BaseId = Context.BaseId;
	Result.NetworkRevision = NetworkRevision;
	if (Context.RequestId.IsValid())
	{
		APlayerController* RequestingController = nullptr;
		if (const TWeakObjectPtr<APlayerController>* PendingRequester =
				AdmittedCommandRequesters.Find(Context.RequestId))
		{
			RequestingController = PendingRequester->Get();
		}
		AdmittedCommandRequesters.Remove(Context.RequestId);
		if (RequestingController)
		{
			CacheCommandResult(
				Context.RequestId,
				PayloadHash,
				RequestingController,
				Result);
		}
	}
	if (bOwnsActiveCommand)
	{
		FlushDeferredResourceChangeMessages();
	}
	return Result;
}

void URpgBaseStorageComponent::ResetCommandEpochAfterRestore()
{
	++CommandEpoch;
	RecentCommandResults.Reset();
	RecentCommandOrder.Reset();
	AdmittedCommandRequesters.Reset();
	ActiveCommandRequestId.Invalidate();
	CommandResourceEntriesBefore.Reset();
	bCommandStorageStateDirty = false;
	bCommandResourceMessagesDeferred = false;
}

void URpgBaseStorageComponent::NotifyExternalStorageStateMutation()
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority() && !bMutationTainted)
	{
		MarkStorageStateDirty();
	}
}

void URpgBaseStorageComponent::TaintAfterRollbackFailure()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	bMutationTainted = true;
	OwnerActor->ForceNetUpdate();
	if (ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(OwnerActor))
	{
		if (ARpgGameModeBase* GameMode = OwnerActor->GetWorld()
				? OwnerActor->GetWorld()->GetAuthGameMode<ARpgGameModeBase>()
				: nullptr)
		{
			GameMode->BlockDiskWritesAfterStorageRollbackFailure(
				BaseCamp->GetBaseId());
		}
	}
}

bool URpgBaseStorageComponent::ApplyResourceSort(ERpgInventorySortMode SortMode)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted)
	{
		return false;
	}

	const bool bChanged = ResourceList.ApplySort(SortMode);
	if (bChanged)
	{
		MarkStorageStateDirty();
	}
	return bChanged;
}

bool URpgBaseStorageComponent::MoveResourceEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || bMutationTainted)
	{
		return false;
	}

	const bool bChanged = ResourceList.MoveResource(ItemDefinition, TargetIndex);
	if (bChanged)
	{
		MarkStorageStateDirty();
	}
	return bChanged;
}

void URpgBaseStorageComponent::MarkStorageStateDirty()
{
	if (ActiveCommandRequestId.IsValid())
	{
		bCommandStorageStateDirty = true;
		return;
	}

	checkf(
		NetworkRevision < MAX_int64,
		TEXT("Base-storage network revision exhausted its 64-bit monotone range."));
	++NetworkRevision;
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
		if (ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(OwnerActor))
		{
			BaseCamp->RefreshStorageAnchorVisuals();
			if (ARpgGameModeBase* GameMode =
					OwnerActor->GetWorld()->GetAuthGameMode<ARpgGameModeBase>())
			{
				GameMode->MarkBaseStorageSaveDirty(BaseCamp);
			}
		}
	}
}

bool URpgBaseStorageComponent::DeferResourceChangeMessage()
{
	if (!ActiveCommandRequestId.IsValid())
	{
		return false;
	}
	bCommandResourceMessagesDeferred = true;
	bCommandStorageStateDirty = true;
	return true;
}

void URpgBaseStorageComponent::FlushDeferredResourceChangeMessages()
{
	const bool bShouldBroadcast = bCommandResourceMessagesDeferred;
	bCommandResourceMessagesDeferred = false;
	TArray<FRpgBaseResourceEntry> EntriesBefore =
		MoveTemp(CommandResourceEntriesBefore);
	CommandResourceEntriesBefore.Reset();
	if (!bShouldBroadcast)
	{
		return;
	}

	for (FRpgBaseResourceEntry& Current : ResourceList.Entries)
	{
		const FRpgBaseResourceEntry* Previous =
			EntriesBefore.FindByPredicate(
				[&Current](const FRpgBaseResourceEntry& Candidate)
				{
					return Candidate.ItemDefinition ==
						Current.ItemDefinition;
				});
		const int32 PreviousCount = Previous ? Previous->Count : 0;
		const int32 PreviousCapacity = Previous ? Previous->Capacity : 0;
		const bool bOrderChanged =
			!Previous || Previous->SortIndex != Current.SortIndex;
		if (PreviousCount != Current.Count ||
			PreviousCapacity != Current.Capacity || bOrderChanged)
		{
			ResourceList.BroadcastChangeMessage(
				Current,
				PreviousCount,
				PreviousCapacity,
				bOrderChanged);
		}
	}

	for (const FRpgBaseResourceEntry& Previous : EntriesBefore)
	{
		if (ResourceList.Entries.ContainsByPredicate(
				[&Previous](const FRpgBaseResourceEntry& Current)
				{
					return Current.ItemDefinition ==
						Previous.ItemDefinition;
				}))
		{
			continue;
		}

		FRpgBaseResourceEntry Removed = Previous;
		Removed.Count = 0;
		Removed.Capacity = 0;
		ResourceList.BroadcastChangeMessage(
			Removed,
			Previous.Count,
			Previous.Capacity,
			true);
	}
}

void URpgBaseStorageComponent::RefreshDerivedResourceCapacities()
{
	if (!bUseSharedMaterialCapacity)
	{
		return;
	}

	const int32 FreePoints = GetFreeMaterialCapacityPoints();
	for (FRpgBaseResourceEntry& Entry : ResourceList.Entries)
	{
		const int32 Cost = FMath::Max(1, GetBulkCapacityCost(Entry.ItemDefinition));
		const int32 DerivedCapacity = Entry.Count + FreePoints / Cost;
		if (Entry.Capacity != DerivedCapacity)
		{
			const int32 OldCount = Entry.Count;
			const int32 OldCapacity = Entry.Capacity;
			Entry.Capacity = DerivedCapacity;
			ResourceList.MarkItemDirty(Entry);
			ResourceList.BroadcastChangeMessage(Entry, OldCount, OldCapacity);
		}
	}
}

FGameplayTagContainer
URpgBaseStorageComponent::BuildEffectiveBaselineCapabilities() const
{
	FGameplayTagContainer Capabilities = BaselineCapabilities;
	const auto AddCapabilityIfRegistered =
		[&Capabilities](const TCHAR* TagName)
		{
			const FGameplayTag Tag =
				FGameplayTag::RequestGameplayTag(TagName, false);
			if (Tag.IsValid())
			{
				Capabilities.AddTag(Tag);
			}
		};
	AddCapabilityIfRegistered(TEXT("Storage.Capability.AutoDepositBulk"));
	AddCapabilityIfRegistered(TEXT("Storage.Capability.CraftFromNetwork"));
	AddCapabilityIfRegistered(TEXT("Storage.Capability.PersonalLocker"));
	return Capabilities;
}

bool URpgBaseStorageComponent::ValidateUpgradeSetRequirements(
	const TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>>& CandidateUpgrades,
	const FGameplayTagContainer& CandidateCapabilities,
	FText* OutFailureReason) const
{
	const UWorld* World = GetWorld();
	const ARpgGameStateBase* GameState =
		World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
	const URpgWorldStorageKnowledgeComponent* Knowledge = GameState
		? GameState->GetWorldStorageKnowledgeComponent() : nullptr;

	for (const URpgBaseStorageUpgradeDefinition* Upgrade : CandidateUpgrades)
	{
		if (!Upgrade)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeSetRequirementInvalidAsset",
					"The installed upgrade set contains an invalid asset.");
			}
			return false;
		}
		if (!CandidateCapabilities.HasAllExact(
				Upgrade->RequiredInstalledCapabilityTags))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeSetCapabilityMissing",
					"A remaining upgrade would lose a required storage capability.");
			}
			return false;
		}
		if (!Upgrade->RequiredKnowledgeTags.IsEmpty() &&
			(!Knowledge || !Knowledge->HasAllKnowledgeTags(
				Upgrade->RequiredKnowledgeTags)))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeSetKnowledgeMissing",
					"The world is missing knowledge required by an installed upgrade.");
			}
			return false;
		}
	}

	return true;
}

bool URpgBaseStorageComponent::RebuildDerivedUpgradeState(
	const TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>>& CandidateUpgrades,
	FText* OutFailureReason,
	bool bAllowPreservedOverCapacityState)
{
	int64 NewMaterialCapacity = FMath::Max(0, BaseMaterialCapacityPoints);
	int64 NewContainmentSlots = FMath::Max(0, BaseContainmentSlots);
	int64 NewArmoryColumns = FMath::Max(1, BaseArmoryGridColumns);
	int64 NewArmoryRows = FMath::Max(1, BaseArmoryGridRows);
	double NewContainmentStrength = 0.0;
	double NewCorruptionProtection = 0.0;
	double NewAddedStrain = 0.0;
	double NewStrainTolerance = 0.0;
	double NewStrainMitigation = 0.0;
	FGameplayTagContainer NewCapabilities =
		BuildEffectiveBaselineCapabilities();

	TSet<const URpgBaseStorageUpgradeDefinition*> SeenUpgrades;
	for (const URpgBaseStorageUpgradeDefinition* Upgrade : CandidateUpgrades)
	{
		if (!Upgrade || SeenUpgrades.Contains(Upgrade))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeSetInvalid",
					"The installed upgrade set contains an invalid or duplicate asset.");
			}
			return false;
		}
		SeenUpgrades.Add(Upgrade);
		if (Upgrade->TargetAnchorId.IsNone() ||
			!Upgrade->TargetDomainTag.IsValid())
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeSetTargetMissing",
					"Every installed storage-network upgrade requires an explicit domain and anchor.");
			}
			return false;
		}

		NewCapabilities.AppendTags(Upgrade->GrantedCapabilityTags);
		const FString DomainName = Upgrade->TargetDomainTag.ToString();
		if (DomainName == TEXT("Storage.Domain.Material") ||
			DomainName == TEXT("Storage.Domain.Materials"))
		{
			NewMaterialCapacity += Upgrade->CapacityEffect.AdditionalCapacity;
		}
		else if (DomainName == TEXT("Storage.Domain.RiftContainment"))
		{
			NewContainmentSlots +=
				Upgrade->ContainmentEffect.AdditionalSealedSlots;
			NewContainmentStrength +=
				Upgrade->ContainmentEffect.ContainmentStrengthDelta;
			NewCorruptionProtection +=
				Upgrade->ContainmentEffect.CorruptionProtectionDelta;
			NewAddedStrain += Upgrade->StrainEffect.AddedStrain;
			NewStrainTolerance +=
				Upgrade->StrainEffect.StrainToleranceDelta;
			NewStrainMitigation +=
				Upgrade->StrainEffect.StrainMitigation;
		}
		else if (DomainName == TEXT("Storage.Domain.Armory"))
		{
			NewArmoryColumns += Upgrade->GridEffect.AdditionalColumns;
			NewArmoryRows += Upgrade->GridEffect.AdditionalRows;
		}
	}
	if (!ValidateUpgradeSetRequirements(
			CandidateUpgrades,
			NewCapabilities,
			OutFailureReason))
	{
		return false;
	}

	const bool bArmoryDimensionsSupported =
		NewArmoryColumns <= MAX_int32 && NewArmoryRows <= MAX_int32;
	const int64 NewArmoryEntryBudget = bArmoryDimensionsSupported
		? NewArmoryColumns * NewArmoryRows
		: MAX_int64;
	if (NewMaterialCapacity > MAX_int32 || NewContainmentSlots > MAX_int32 ||
		NewArmoryColumns > MAX_int32 || NewArmoryRows > MAX_int32 ||
		NewArmoryEntryBudget > MAX_int32 ||
		!FMath::IsFinite(NewContainmentStrength) ||
		!FMath::IsFinite(NewCorruptionProtection) ||
		!FMath::IsFinite(NewAddedStrain) ||
		!FMath::IsFinite(NewStrainTolerance) ||
		!FMath::IsFinite(NewStrainMitigation) ||
		NewAddedStrain > MAX_int32 ||
		NewStrainTolerance > MAX_int32 ||
		NewStrainMitigation > MAX_int32)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = NSLOCTEXT(
				"RpgBaseStorage", "UpgradeCapacityOverflow",
				"The combined upgrade capacity exceeds the supported range.");
		}
		return false;
	}
	if (!bAllowPreservedOverCapacityState &&
		NewMaterialCapacity < GetUsedMaterialCapacityPoints64())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = NSLOCTEXT(
				"RpgBaseStorage", "UpgradeMaterialCapacityTooSmall",
				"Stored materials do not fit into the resulting capacity.");
		}
		return false;
	}

	ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(GetOwner());
	URpgInventoryManagerComponent* Armory = BaseCamp
		? BaseCamp->GetArmoryInventoryComponent() : nullptr;
	URpgInventoryManagerComponent* Containment = BaseCamp
		? BaseCamp->GetContainmentInventoryComponent() : nullptr;
	if (!bAllowPreservedOverCapacityState && Containment &&
		NewContainmentSlots < Containment->GetUsedEntryCount())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = NSLOCTEXT(
				"RpgBaseStorage", "UpgradeContainmentCapacityTooSmall",
				"Contained Rift items do not fit into the resulting sealed slots.");
		}
		return false;
	}
	FRpgInventoryGridSize NewContainmentSize;
	NewContainmentSize.Width = FMath::Max(1, static_cast<int32>(NewContainmentSlots));
	NewContainmentSize.Height = 1;
	if (!bAllowPreservedOverCapacityState && Containment &&
		!Containment->CanSetDefaultGridSize(NewContainmentSize))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = NSLOCTEXT(
				"RpgBaseStorage", "UpgradeContainmentGridTooSmall",
				"A contained Rift item occupies a slot outside the resulting grid.");
		}
		return false;
	}
	FRpgInventoryGridSize NewArmorySize;
	NewArmorySize.Width = static_cast<int32>(NewArmoryColumns);
	NewArmorySize.Height = static_cast<int32>(NewArmoryRows);
	if (!bAllowPreservedOverCapacityState && Armory &&
		!Armory->CanSetDefaultGridSize(NewArmorySize))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = NSLOCTEXT(
				"RpgBaseStorage", "UpgradeArmoryGridTooSmall",
				"Stored Armory placements do not fit inside the resulting grid.");
		}
		return false;
	}

	const FRpgInventoryGridSize PreviousArmorySize = Armory
		? Armory->GetDefaultGridSize()
		: FRpgInventoryGridSize();
	if (Armory)
	{
		if (!Armory->SetDefaultGridSize(NewArmorySize))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeArmoryResizeFailed",
					"The Armory grid could not be resized authoritatively.");
			}
			return false;
		}
	}
	if (Containment)
	{
		if (!Containment->SetDefaultGridSize(NewContainmentSize))
		{
			if (Armory)
			{
				Armory->SetDefaultGridSize(PreviousArmorySize);
			}
			if (OutFailureReason)
			{
				*OutFailureReason = NSLOCTEXT(
					"RpgBaseStorage", "UpgradeContainmentResizeFailed",
					"The containment slot grid could not be resized authoritatively.");
			}
			return false;
		}
	}

	MaterialCapacityPoints = static_cast<int32>(NewMaterialCapacity);
	ContainmentSlotCapacity = static_cast<int32>(NewContainmentSlots);
	ArmoryGridColumns = static_cast<int32>(NewArmoryColumns);
	ArmoryGridRows = static_cast<int32>(NewArmoryRows);
	ContainmentStrength = static_cast<float>(NewContainmentStrength);
	CorruptionProtection = static_cast<float>(NewCorruptionProtection);
	PassiveRiftStrain = FMath::Clamp(
		FMath::CeilToInt(NewAddedStrain - NewStrainTolerance),
		0,
		100);
	RiftStrainMitigation = FMath::Max(
		0,
		FMath::FloorToInt(NewStrainMitigation));
	InstalledCapabilities = MoveTemp(NewCapabilities);
	if (Armory)
	{
		Armory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
		Armory->SetFixedMaxEntries(
			static_cast<int32>(NewArmoryEntryBudget));
	}
	if (Containment)
	{
		Containment->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
		Containment->SetFixedMaxEntries(ContainmentSlotCapacity);
	}
	bArmoryDomainOverCapacity = false;
	bContainmentDomainOverCapacity = false;
	return true;
}

URpgBaseStorageUpgradeDefinition*
URpgBaseStorageComponent::ResolveUpgradeDefinition(
	const FPrimaryAssetId& UpgradeId) const
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
	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(UpgradeId);
	return AssetPath.IsValid()
		? Cast<URpgBaseStorageUpgradeDefinition>(AssetPath.TryLoad())
		: nullptr;
}

FName URpgBaseStorageComponent::GetOwningBaseId() const
{
	const ARpgBaseCampActor* BaseCamp = Cast<ARpgBaseCampActor>(GetOwner());
	return BaseCamp ? BaseCamp->GetBaseId() : NAME_None;
}

void URpgBaseStorageComponent::CacheCommandResult(
	const FGuid& RequestId,
	uint32 PayloadHash,
	APlayerController* RequestingController,
	const FRpgBaseStorageCommandResult& Result)
{
	if (!RequestId.IsValid() || !RequestingController)
	{
		return;
	}

	FRecentCommandResult& Cached = RecentCommandResults.FindOrAdd(RequestId);
	Cached.PayloadHash = PayloadHash;
	Cached.CommandEpoch = CommandEpoch;
	Cached.RequestingController = RequestingController;
	Cached.Result = Result;
	RecentCommandOrder.Remove(RequestId);
	RecentCommandOrder.Add(RequestId);

	while (RecentCommandOrder.Num() > MaxRecentCommandResults)
	{
		const FGuid Oldest = RecentCommandOrder[0];
		RecentCommandOrder.RemoveAt(0, 1, EAllowShrinking::No);
		RecentCommandResults.Remove(Oldest);
	}
}
