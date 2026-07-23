#include "RpgInventoryFragment_SlotContainerProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_SlotContainerProvider)

namespace
{
	FRpgInventoryItemContainerDefinition ConvertLegacyContainerDefinition(
		const FRpgInventorySlotGroupDefinition& LegacyGroup)
	{
		FRpgInventoryItemContainerDefinition Converted;
		Converted.ContainerId = LegacyGroup.ContainerId;
		Converted.DisplayName = LegacyGroup.DisplayName;
		Converted.Icon = LegacyGroup.Icon;
		Converted.GridSize = LegacyGroup.GridSize;
		Converted.AllowedCategories =
			LegacyGroup.Rule.AllowedCategories;
		Converted.RequiredItemTags =
			LegacyGroup.Rule.RequiredItemTags;
		Converted.BlockedItemTags =
			LegacyGroup.Rule.BlockedItemTags;
		Converted.bQuickAccessEligible =
			LegacyGroup.Rule.bActionbarBindable;
		Converted.bAllowNestedContainers = false;
		Converted.MaxNestingDepth =
			RpgInventoryMaxItemOwnedDepth;
		return Converted;
	}
}

void URpgInventoryFragment_SlotContainerProvider::GetProvidedContainers(
	TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const
{
	Super::GetProvidedContainers(OutContainers);

	for (const FRpgInventorySlotGroupDefinition& LegacyGroup : ProvidedSlotGroups)
	{
		if (LegacyGroup.ContainerId.IsNone())
		{
			continue;
		}

		if (OutContainers.ContainsByPredicate(
				[&LegacyGroup](const FRpgInventoryItemContainerDefinition& Existing)
				{
					return Existing.ContainerId == LegacyGroup.ContainerId;
				}))
		{
			continue;
		}

		OutContainers.Add(
			ConvertLegacyContainerDefinition(LegacyGroup));
	}
}

void URpgInventoryFragment_SlotContainerProvider::
	GetAuthoredContainerDefinitions(
		TArray<FRpgInventoryItemContainerDefinition>& OutContainers) const
{
	Super::GetAuthoredContainerDefinitions(OutContainers);

	for (const FRpgInventorySlotGroupDefinition& LegacyGroup :
		ProvidedSlotGroups)
	{
		OutContainers.Add(
			ConvertLegacyContainerDefinition(LegacyGroup));
	}
}
