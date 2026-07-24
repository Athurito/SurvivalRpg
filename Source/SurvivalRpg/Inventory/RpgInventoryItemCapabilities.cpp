#include "RpgInventoryItemCapabilities.h"

#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

namespace
{
	const URpgInventoryFragment_UsableItem* FindUsableContract(
		const URpgInventoryItemInstance* Item)
	{
		return Item
			? Item->FindFragmentByClass<URpgInventoryFragment_UsableItem>()
			: nullptr;
	}
}

bool FRpgInventoryItemCapabilities::HasItemContainerContract(
	const URpgInventoryItemInstance* Item)
{
	const URpgInventoryFragment_ItemContainer* ContainerContract =
		Item
			? Item->FindFragmentByClass<
				URpgInventoryFragment_ItemContainer>()
			: nullptr;
	if (!ContainerContract)
	{
		return false;
	}

	TArray<FRpgInventoryItemContainerDefinition> Containers;
	ContainerContract->GetProvidedContainers(Containers);
	return Containers.ContainsByPredicate(
		[](const FRpgInventoryItemContainerDefinition& Definition)
		{
			return Definition.IsValid();
		});
}

bool FRpgInventoryItemCapabilities::HasUsableContract(
	const URpgInventoryItemInstance* Item)
{
	return FindUsableContract(Item) != nullptr;
}

FRpgInventoryUseCapabilityEvaluation
FRpgInventoryItemCapabilities::EvaluateUse(
	const URpgInventoryItemInstance* Item,
	const URpgInventoryManagerComponent* SourceInventory,
	const URpgInventoryManagerComponent* PlayerInventory,
	int32 AvailableStackCount,
	int32 UseCount)
{
	FRpgInventoryUseCapabilityEvaluation Evaluation;
	Evaluation.UseContract = FindUsableContract(Item);
	if (!Evaluation.UseContract || !Evaluation.UseContract->UseAbility)
	{
		return Evaluation;
	}

	if (Evaluation.UseContract->bOnlyFromPlayerInventory &&
		SourceInventory != PlayerInventory)
	{
		Evaluation.Result =
			ERpgInventoryUseCapabilityResult::WrongInventory;
		return Evaluation;
	}

	const int32 ConsumePerUse =
		FMath::Max(0, Evaluation.UseContract->ConsumeCount);
	if (UseCount <= 0 || (ConsumePerUse == 0 && UseCount != 1))
	{
		Evaluation.Result =
			ERpgInventoryUseCapabilityResult::InvalidRequest;
		return Evaluation;
	}

	const int64 RequestedConsumeCount =
		static_cast<int64>(ConsumePerUse) *
		static_cast<int64>(UseCount);
	if (RequestedConsumeCount > MAX_int32)
	{
		Evaluation.Result =
			ERpgInventoryUseCapabilityResult::InvalidRequest;
		return Evaluation;
	}

	Evaluation.RequiredConsumeCount =
		static_cast<int32>(RequestedConsumeCount);
	if (AvailableStackCount <= 0 ||
		Evaluation.RequiredConsumeCount > AvailableStackCount)
	{
		Evaluation.Result =
			ERpgInventoryUseCapabilityResult::InsufficientQuantity;
		return Evaluation;
	}

	Evaluation.Result = ERpgInventoryUseCapabilityResult::Available;
	return Evaluation;
}

ERpgInventoryManualDropPolicy
FRpgInventoryItemCapabilities::ResolveManualDropPolicy(
	const URpgInventoryItemInstance* Item)
{
	const URpgInventoryFragment_ItemTraits* Traits = Item
		? Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>()
		: nullptr;
	return Traits
		? Traits->GetResolvedManualDropPolicy()
		: ERpgInventoryManualDropPolicy::Direct;
}

FRpgInventorySpatialCapability
FRpgInventoryItemCapabilities::ResolveSpatial(
	const URpgInventoryItemInstance* Item)
{
	FRpgInventorySpatialCapability Capability;
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item ? Item->GetItemDef() : nullptr;
	const URpgInventoryFragment_SpatialItem* SpatialFragment =
		URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
			ItemDefinition);
	if (SpatialFragment)
	{
		Capability.Footprint = SpatialFragment->Footprint;
		Capability.bAllowRotation = SpatialFragment->bAllowRotation;
	}
	return Capability;
}

bool FRpgInventoryItemCapabilities::ShouldUseAsPrimaryAction(
	const URpgInventoryItemInstance* Item,
	bool bHasDefaultEquipmentDestination)
{
	if (!bHasDefaultEquipmentDestination)
	{
		return true;
	}

	const URpgInventoryFragment_UsableItem* UseContract =
		FindUsableContract(Item);
	return UseContract &&
		UseContract->HybridQuickAction !=
			ERpgInventoryHybridQuickAction::EquipAndActivate;
}
