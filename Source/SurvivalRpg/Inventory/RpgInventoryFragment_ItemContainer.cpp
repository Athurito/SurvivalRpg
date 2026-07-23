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

	const bool bProvidesNestedContainer =
		ItemCDO->FindFragmentByClass(URpgInventoryFragment_ItemContainer::StaticClass()) != nullptr;
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

void URpgInventoryFragment_ItemContainer::
	GetAuthoredContainerDefinitions(
		TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const
{
	OutContainers.Append(ProvidedContainers);
}

bool URpgInventoryFragment_ItemContainer::
	HasStructurallyValidProvidedContainers() const
{
	TArray<FRpgInventoryItemContainerDefinition> AuthoredContainers;
	GetAuthoredContainerDefinitions(AuthoredContainers);

	TSet<FName> UniqueContainerIds;
	for (const FRpgInventoryItemContainerDefinition& Definition :
		AuthoredContainers)
	{
		if (!Definition.IsValid() ||
			UniqueContainerIds.Contains(Definition.ContainerId))
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
