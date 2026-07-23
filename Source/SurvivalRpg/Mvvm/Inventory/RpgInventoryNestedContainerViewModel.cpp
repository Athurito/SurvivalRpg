#include "RpgInventoryNestedContainerViewModel.h"

#include "Algo/Reverse.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryNestedContainerViewModel)

bool URpgInventoryNestedContainerViewModel::OpenContainerByItemId(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryItemId ItemId,
	FName LocalContainerId)
{
	URpgInventoryItemInstance* OwnerItem = Inventory && ItemId.IsValid() ? Inventory->FindItemById(ItemId) : nullptr;
	const URpgInventoryFragment_ItemContainer* ContainerFragment = OwnerItem
		? OwnerItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
		: nullptr;
	if (!OwnerItem || !ContainerFragment)
	{
		return false;
	}

	TArray<FRpgInventoryItemContainerDefinition> Definitions;
	ContainerFragment->GetProvidedContainers(Definitions);
	const FRpgInventoryItemContainerDefinition* SelectedDefinition = Definitions.FindByPredicate(
		[LocalContainerId](const FRpgInventoryItemContainerDefinition& Candidate)
		{
			return Candidate.IsValid() && (LocalContainerId.IsNone() || Candidate.ContainerId == LocalContainerId);
		});
	if (!SelectedDefinition)
	{
		return false;
	}

	FRpgInventoryGridPlacement OwnerPlacement;
	if (!Inventory->GetItemPlacement(OwnerItem, OwnerPlacement))
	{
		return false;
	}

	const uint8 ChildDepth = OwnerPlacement.GetContainerHandle().GetDirectChildDepth();
	if (ChildDepth == 0)
	{
		return false;
	}

	return OpenContainerHandle(
		Inventory,
		FRpgInventoryContainerHandle::MakeItemOwned(ItemId, SelectedDefinition->ContainerId, ChildDepth));
}

bool URpgInventoryNestedContainerViewModel::OpenContainerHandle(
	URpgInventoryManagerComponent* Inventory,
	FRpgInventoryContainerHandle ContainerHandle)
{
	FRpgInventoryGridSize GridSize;
	if (!Inventory || !ContainerHandle.IsValid() ||
		!Inventory->GetGridSizeForContainerHandle(ContainerHandle, GridSize) || !GridSize.IsValid())
	{
		return false;
	}

	if (ContainerHandle.IsItemOwned() && !Inventory->FindItemById(ContainerHandle.ItemOwnerId))
	{
		return false;
	}

	TArray<FRpgInventoryContainerBreadcrumb> ResolvedBreadcrumbs;
	FText ResolvedTitle;
	if (!BuildBreadcrumbs(Inventory, ContainerHandle, ResolvedBreadcrumbs, ResolvedTitle))
	{
		return false;
	}

	EnsurePanelViewModel();
	if (!PanelViewModel)
	{
		return false;
	}

	ObservedInventory = Inventory;
	ActiveContainerHandle = ContainerHandle;
	OpenOwnerItemId = ContainerHandle.IsItemOwned() ? ContainerHandle.ItemOwnerId : FRpgInventoryItemId();
	Breadcrumbs = MoveTemp(ResolvedBreadcrumbs);
	Title = MoveTemp(ResolvedTitle);
	bIsOpen = true;

	bSuppressPanelEntryCallback = true;
	PanelViewModel->BindInventoryContainer(Inventory, ContainerHandle);
	bSuppressPanelEntryCallback = false;
	RefreshFilterPresentation();
	BroadcastPresentationFields();
	return true;
}

bool URpgInventoryNestedContainerViewModel::NavigateToBreadcrumb(int32 BreadcrumbIndex)
{
	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	return Inventory && Breadcrumbs.IsValidIndex(BreadcrumbIndex)
		? OpenContainerHandle(Inventory, Breadcrumbs[BreadcrumbIndex].Handle)
		: false;
}

void URpgInventoryNestedContainerViewModel::CloseContainer()
{
	bSuppressPanelEntryCallback = true;
	if (PanelViewModel)
	{
		PanelViewModel->UnbindInventory();
	}
	bSuppressPanelEntryCallback = false;

	ObservedInventory.Reset();
	bIsOpen = false;
	OpenOwnerItemId.Reset();
	ActiveContainerHandle = FRpgInventoryContainerHandle();
	Title = FText::GetEmpty();
	Breadcrumbs.Reset();
	FilterQuery = FText::GetEmpty();
	EntryFilterPresentation.Reset();
	BroadcastPresentationFields();
}

void URpgInventoryNestedContainerViewModel::SetFilterQuery(const FText& NewFilterQuery)
{
	if (FilterQuery.EqualTo(NewFilterQuery))
	{
		return;
	}

	FilterQuery = NewFilterQuery;
	RefreshFilterPresentation();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilterQuery);
	OnPresentationChanged.Broadcast();
}

void URpgInventoryNestedContainerViewModel::ClearFilterQuery()
{
	SetFilterQuery(FText::GetEmpty());
}

bool URpgInventoryNestedContainerViewModel::IsEntryDimmed(FGuid EntryId) const
{
	if (!EntryId.IsValid())
	{
		return false;
	}

	const FRpgInventoryEntryFilterPresentation* Presentation = EntryFilterPresentation.FindByPredicate(
		[EntryId](const FRpgInventoryEntryFilterPresentation& Candidate)
		{
			return Candidate.EntryId == EntryId;
		});
	return Presentation && Presentation->bDimmed;
}

TArray<FGuid> URpgInventoryNestedContainerViewModel::GetDimmedEntryIds() const
{
	TArray<FGuid> Result;
	for (const FRpgInventoryEntryFilterPresentation& Presentation : EntryFilterPresentation)
	{
		if (Presentation.bDimmed && Presentation.EntryId.IsValid())
		{
			Result.Add(Presentation.EntryId);
		}
	}
	return Result;
}

void URpgInventoryNestedContainerViewModel::BeginDestroy()
{
	if (PanelViewModel)
	{
		PanelViewModel->OnEntriesChanged.RemoveDynamic(this, &ThisClass::HandlePanelEntriesChanged);
		bSuppressPanelEntryCallback = true;
		PanelViewModel->UnbindInventory();
		bSuppressPanelEntryCallback = false;
	}
	ObservedInventory.Reset();
	Super::BeginDestroy();
}

void URpgInventoryNestedContainerViewModel::HandlePanelEntriesChanged()
{
	if (bSuppressPanelEntryCallback)
	{
		return;
	}

	if (!RevalidateOpenContainerBinding())
	{
		CloseContainer();
		return;
	}

	RefreshFilterPresentation();
	BroadcastPresentationFields();
}

void URpgInventoryNestedContainerViewModel::EnsurePanelViewModel()
{
	if (PanelViewModel)
	{
		return;
	}

	PanelViewModel = NewObject<URpgInventoryPanelViewModel>(this);
	PanelViewModel->OnEntriesChanged.AddUniqueDynamic(this, &ThisClass::HandlePanelEntriesChanged);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PanelViewModel);
}

bool URpgInventoryNestedContainerViewModel::RevalidateOpenContainerBinding()
{
	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	if (!bIsOpen || !Inventory || !ActiveContainerHandle.IsValid())
	{
		return false;
	}

	FRpgInventoryContainerHandle ResolvedHandle = ActiveContainerHandle;
	if (ActiveContainerHandle.IsItemOwned())
	{
		URpgInventoryItemInstance* OwnerItem = Inventory->FindItemById(ActiveContainerHandle.ItemOwnerId);
		FRpgInventoryGridPlacement OwnerPlacement;
		if (!OwnerItem || !Inventory->GetItemPlacement(OwnerItem, OwnerPlacement))
		{
			return false;
		}

		const uint8 ResolvedDepth = OwnerPlacement.GetContainerHandle().GetDirectChildDepth();
		if (ResolvedDepth == 0)
		{
			return false;
		}
		ResolvedHandle = FRpgInventoryContainerHandle::MakeItemOwned(
			ActiveContainerHandle.ItemOwnerId,
			ActiveContainerHandle.ContainerId,
			ResolvedDepth);
	}

	FRpgInventoryGridSize GridSize;
	TArray<FRpgInventoryContainerBreadcrumb> ResolvedBreadcrumbs;
	FText ResolvedTitle;
	if (!Inventory->GetGridSizeForContainerHandle(ResolvedHandle, GridSize) || !GridSize.IsValid() ||
		!BuildBreadcrumbs(Inventory, ResolvedHandle, ResolvedBreadcrumbs, ResolvedTitle))
	{
		return false;
	}

	const bool bHandleChanged = ActiveContainerHandle != ResolvedHandle;
	ActiveContainerHandle = ResolvedHandle;
	OpenOwnerItemId = ResolvedHandle.IsItemOwned() ? ResolvedHandle.ItemOwnerId : FRpgInventoryItemId();
	Breadcrumbs = MoveTemp(ResolvedBreadcrumbs);
	Title = MoveTemp(ResolvedTitle);
	if (bHandleChanged && PanelViewModel)
	{
		bSuppressPanelEntryCallback = true;
		PanelViewModel->BindInventoryContainer(Inventory, ResolvedHandle);
		bSuppressPanelEntryCallback = false;
	}

	return true;
}

void URpgInventoryNestedContainerViewModel::RefreshFilterPresentation()
{
	EntryFilterPresentation.Reset();

	FString NormalizedQuery = FilterQuery.ToString();
	NormalizedQuery.TrimStartAndEndInline();
	NormalizedQuery.ToLowerInline();
	TArray<FString> QueryTokens;
	NormalizedQuery.ParseIntoArrayWS(QueryTokens);

	if (PanelViewModel)
	{
		for (const URpgInventoryEntryViewModel* Entry : PanelViewModel->GetEntries())
		{
			if (!Entry || Entry->IsEmptySlot() || !Entry->GetItemInstance())
			{
				continue;
			}

			FRpgInventoryEntryFilterPresentation& Presentation = EntryFilterPresentation.AddDefaulted_GetRef();
			Presentation.ItemId = Entry->GetItemId();
			Presentation.EntryId = Entry->GetEntryId();
			Presentation.bMatchesFilter = DoesEntryMatchQuery(Entry, QueryTokens);
			Presentation.bDimmed = !Presentation.bMatchesFilter;
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryFilterPresentation);
}

bool URpgInventoryNestedContainerViewModel::BuildBreadcrumbs(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryContainerHandle& ContainerHandle,
	TArray<FRpgInventoryContainerBreadcrumb>& OutBreadcrumbs,
	FText& OutTitle) const
{
	OutBreadcrumbs.Reset();
	OutTitle = FText::GetEmpty();
	if (!Inventory || !ContainerHandle.IsValid())
	{
		return false;
	}

	TArray<FRpgInventoryContainerBreadcrumb> ReverseBreadcrumbs;
	TSet<FRpgInventoryItemId> VisitedOwners;
	FRpgInventoryContainerHandle CurrentHandle = ContainerHandle;
	for (int32 Guard = 0; Guard <= static_cast<int32>(FRpgInventoryContainerHandle::MaxItemOwnedDepth) + 1; ++Guard)
	{
		FRpgInventoryContainerBreadcrumb& Breadcrumb = ReverseBreadcrumbs.AddDefaulted_GetRef();
		Breadcrumb.Handle = CurrentHandle;
		Breadcrumb.OwnerItemId = CurrentHandle.IsItemOwned() ? CurrentHandle.ItemOwnerId : FRpgInventoryItemId();
		Breadcrumb.Label = ResolveBreadcrumbLabel(Inventory, CurrentHandle);

		if (CurrentHandle.IsRoot())
		{
			break;
		}

		if (!CurrentHandle.IsItemOwned() || VisitedOwners.Contains(CurrentHandle.ItemOwnerId))
		{
			return false;
		}
		VisitedOwners.Add(CurrentHandle.ItemOwnerId);

		URpgInventoryItemInstance* OwnerItem = Inventory->FindItemById(CurrentHandle.ItemOwnerId);
		FRpgInventoryGridPlacement OwnerPlacement;
		if (!OwnerItem || !Inventory->GetItemPlacement(OwnerItem, OwnerPlacement))
		{
			return false;
		}

		const FRpgInventoryContainerHandle ParentHandle = OwnerPlacement.GetContainerHandle();
		if (!ParentHandle.IsValid() || ParentHandle.GetDirectChildDepth() != CurrentHandle.Depth)
		{
			return false;
		}
		CurrentHandle = ParentHandle;
	}

	if (ReverseBreadcrumbs.IsEmpty() || !ReverseBreadcrumbs.Last().Handle.IsRoot())
	{
		return false;
	}

	Algo::Reverse(ReverseBreadcrumbs);
	for (int32 Index = 0; Index < ReverseBreadcrumbs.Num(); ++Index)
	{
		ReverseBreadcrumbs[Index].bCurrent = Index == ReverseBreadcrumbs.Num() - 1;
	}
	OutTitle = ReverseBreadcrumbs.Last().Label;
	OutBreadcrumbs = MoveTemp(ReverseBreadcrumbs);
	return true;
}

FText URpgInventoryNestedContainerViewModel::ResolveBreadcrumbLabel(
	URpgInventoryManagerComponent* Inventory,
	const FRpgInventoryContainerHandle& ContainerHandle) const
{
	if (ContainerHandle.IsRoot())
	{
		return FText::FromName(ContainerHandle.Root);
	}

	URpgInventoryItemInstance* OwnerItem = Inventory ? Inventory->FindItemById(ContainerHandle.ItemOwnerId) : nullptr;
	FText ItemLabel = FText::GetEmpty();
	if (OwnerItem && OwnerItem->GetItemDef())
	{
		if (const URpgInventoryItemDefinition* Definition = GetDefault<URpgInventoryItemDefinition>(OwnerItem->GetItemDef()))
		{
			ItemLabel = Definition->DisplayName;
		}
	}

	FText CompartmentLabel = FText::FromName(ContainerHandle.ContainerId);
	if (const URpgInventoryFragment_ItemContainer* Fragment = OwnerItem
		? OwnerItem->FindFragmentByClass<URpgInventoryFragment_ItemContainer>()
		: nullptr)
	{
		TArray<FRpgInventoryItemContainerDefinition> Definitions;
		Fragment->GetProvidedContainers(Definitions);
		if (const FRpgInventoryItemContainerDefinition* Definition = Definitions.FindByPredicate(
			[&ContainerHandle](const FRpgInventoryItemContainerDefinition& Candidate)
			{
				return Candidate.ContainerId == ContainerHandle.ContainerId;
			}); Definition && !Definition->DisplayName.IsEmpty())
		{
			CompartmentLabel = Definition->DisplayName;
		}
	}

	if (ItemLabel.IsEmpty())
	{
		return CompartmentLabel;
	}
	return FText::Format(
		NSLOCTEXT("RpgInventory", "NestedContainerBreadcrumbLabel", "{0} / {1}"),
		ItemLabel,
		CompartmentLabel);
}

bool URpgInventoryNestedContainerViewModel::DoesEntryMatchQuery(
	const URpgInventoryEntryViewModel* Entry,
	const TArray<FString>& QueryTokens) const
{
	if (QueryTokens.IsEmpty())
	{
		return true;
	}
	if (!Entry || !Entry->GetItemInstance())
	{
		return false;
	}

	URpgInventoryItemInstance* Item = Entry->GetItemInstance();
	FString SearchText = FString::Printf(
		TEXT("%s %s %s"),
		*Entry->GetDisplayName().ToString(),
		*Entry->GetShortDisplayName().ToString(),
		*GetNameSafe(Item->GetItemDef()));

	if (const URpgInventoryFragment_ItemTraits* Traits = Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>())
	{
		SearchText += TEXT(" ");
		SearchText += Traits->ItemTags.ToStringSimple();
		if (const UEnum* CategoryEnum = StaticEnum<ERpgInventoryItemCategory>())
		{
			SearchText += TEXT(" ");
			SearchText += CategoryEnum->GetNameStringByValue(static_cast<int64>(Traits->ItemCategory));
		}
	}

	if (const URpgInventoryFragment_UIData* UIData = Item->FindFragmentByClass<URpgInventoryFragment_UIData>())
	{
		SearchText += TEXT(" ");
		SearchText += UIData->Description.ToString();
		SearchText += TEXT(" ");
		SearchText += UIData->PresentationTags.ToStringSimple();
	}

	SearchText.ToLowerInline();
	for (const FString& Token : QueryTokens)
	{
		if (!SearchText.Contains(Token, ESearchCase::CaseSensitive))
		{
			return false;
		}
	}
	return true;
}

void URpgInventoryNestedContainerViewModel::BroadcastPresentationFields()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsOpen);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OpenOwnerItemId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ActiveContainerHandle);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Title);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Breadcrumbs);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FilterQuery);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryFilterPresentation);
	OnPresentationChanged.Broadcast();
}
