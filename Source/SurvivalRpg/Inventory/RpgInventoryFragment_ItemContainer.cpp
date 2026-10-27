#include "RpgInventoryFragment_ItemContainer.h"

#include "RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_ItemContainer)

bool FRpgInventoryItemContainerDefinition::AllowsItemDefinition(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	uint8 TargetDepth) const
{
	if (!IsValid() || !ItemDefinition || TargetDepth == 0 || TargetDepth > RpgInventoryMaxItemOwnedDepth)
	{
		return false;
	}

	const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDefinition);
	if (!ItemCDO)
	{
		return false;
	}

	const URpgInventoryFragment_ItemTraits* Traits =
		Cast<URpgInventoryFragment_ItemTraits>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass()));
	if (AllowedCategories.Num() > 0 && (!Traits || !AllowedCategories.Contains(Traits->ItemCategory)))
	{
		return false;
	}

	if (!RequiredItemTags.IsEmpty() && (!Traits || !Traits->ItemTags.HasAll(RequiredItemTags)))
	{
		return false;
	}

	if (!BlockedItemTags.IsEmpty() && Traits && Traits->ItemTags.HasAny(BlockedItemTags))
	{
		return false;
	}

	// Socket grids are no storage, so an item that only provides sockets, such as a socketed tool, is no nested container.
	const URpgInventoryFragment_ItemContainer* ProvidedContainers = Cast<URpgInventoryFragment_ItemContainer>(
		ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemContainer::StaticClass()));
	const bool bProvidesNestedContainer = ProvidedContainers && ProvidedContainers->ProvidesStorageContainers();
	if (bProvidesNestedContainer &&
		(!bAllowNestedContainers || TargetDepth >= GetEffectiveMaxNestingDepth()))
	{
		return false;
	}

	return true;
}

void URpgInventoryFragment_ItemContainer::GetProvidedContainers(
	TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const
{
	OutContainers.Append(ProvidedContainers);
}

bool URpgInventoryFragment_ItemContainer::
	HasStructurallyValidProvidedContainers() const
{
	TArray<FRpgInventoryItemContainerDefinition> ContainerDefinitions;
	GetProvidedContainers(ContainerDefinitions);

	TSet<FName> UniqueContainerIds;
	for (const FRpgInventoryItemContainerDefinition& Definition :
		ContainerDefinitions)
	{
		if (!Definition.IsValid() ||
			UniqueContainerIds.Contains(Definition.ContainerId) ||
			(Definition.bSocketGrid && (Definition.bAllowNestedContainers || Definition.bQuickAccessEligible)))
		{
			return false;
		}

		UniqueContainerIds.Add(Definition.ContainerId);
	}

	return true;
}

const FRpgInventoryItemContainerDefinition* URpgInventoryFragment_ItemContainer::FindProvidedContainer(
	FName ContainerId) const
{
	return ProvidedContainers.FindByPredicate(
		[ContainerId](const FRpgInventoryItemContainerDefinition& Definition)
		{
			return Definition.ContainerId == ContainerId;
		});
}

bool URpgInventoryFragment_ItemContainer::ProvidesStorageContainers() const
{
	TArray<FRpgInventoryItemContainerDefinition> Definitions;
	GetProvidedContainers(Definitions);
	return Definitions.ContainsByPredicate(
		[](const FRpgInventoryItemContainerDefinition& Definition)
		{
			return !Definition.bSocketGrid;
		});
}
