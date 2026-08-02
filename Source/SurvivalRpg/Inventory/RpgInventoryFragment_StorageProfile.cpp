#include "RpgInventoryFragment_StorageProfile.h"

#include "RpgInventoryFragment_ContainmentProfile.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_StorageProfile)

namespace RpgInventoryStorageProfilePrivate
{
	bool IsStrictChildOf(const FGameplayTag Tag, const FGameplayTag RootTag)
	{
		return Tag.IsValid() && RootTag.IsValid() && Tag != RootTag && Tag.MatchesTag(RootTag);
	}

	bool AreStrictChildrenOf(const FGameplayTagContainer& Tags, const FGameplayTag RootTag)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (!IsStrictChildOf(Tag, RootTag))
			{
				return false;
			}
		}
		return true;
	}

	template <typename FragmentType>
	int32 CountFragments(const URpgInventoryItemDefinition* ItemDefinition)
	{
		int32 Count = 0;
		if (ItemDefinition)
		{
			for (const URpgInventoryItemFragment* Fragment :
				ItemDefinition->Fragments)
			{
				Count += Fragment && Fragment->IsA<FragmentType>() ? 1 : 0;
			}
		}
		return Count;
	}
}

bool URpgInventoryFragment_StorageProfile::IsStructurallyValid() const
{
	const URpgInventoryItemDefinition* ItemDefinition =
		GetTypedOuter<URpgInventoryItemDefinition>();
	if (!RpgInventoryStorageProfilePrivate::IsStrictChildOf(
			StorageDomainTag,
			RpgGameplayTags::Storage_Domain) ||
		!RpgInventoryStorageProfilePrivate::AreStrictChildrenOf(
			RequiredStorageCapabilityTags,
			RpgGameplayTags::Storage_Capability))
	{
		return false;
	}

	if (StorageMode != ERpgInventoryStorageMode::BulkResource)
	{
		if (StorageMode ==
				ERpgInventoryStorageMode::SpecialContainedItem &&
			ItemDefinition && ItemDefinition->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			return false;
		}
		return !bCanAutoDeposit && !bCanCraftFromNetwork &&
			(StorageMode != ERpgInventoryStorageMode::SpecialContainedItem ||
				StorageDomainTag ==
					RpgGameplayTags::Storage_Domain_RiftContainment) &&
			(StorageMode != ERpgInventoryStorageMode::GridItem ||
				StorageDomainTag !=
					RpgGameplayTags::Storage_Domain_Materials) &&
			(StorageMode != ERpgInventoryStorageMode::GridItem ||
				StorageDomainTag !=
					RpgGameplayTags::Storage_Domain_RiftContainment);
	}

	return BulkCapacityCost > 0 &&
		StorageDomainTag == RpgGameplayTags::Storage_Domain_Materials &&
		(!ItemDefinition || IsDefinitionIntrinsicallyCollapsible(
			ItemDefinition->GetClass()));
}

bool URpgInventoryFragment_StorageProfile::CanDepositAsBulk() const
{
	return IsBulkResource() && IsStructurallyValid();
}

bool URpgInventoryFragment_StorageProfile::CanDepositAsBulkWithCapabilities(
	const FGameplayTagContainer& NetworkCapabilities) const
{
	return CanDepositAsBulk() &&
		NetworkCapabilities.HasAllExact(RequiredStorageCapabilityTags);
}

bool URpgInventoryFragment_StorageProfile::CanAutoDeposit(
	const FGameplayTagContainer& NetworkCapabilities) const
{
	return CanDepositAsBulkWithCapabilities(NetworkCapabilities) && bCanAutoDeposit &&
		NetworkCapabilities.HasTagExact(
			RpgGameplayTags::Storage_Capability_AutoDepositBulk);
}

const URpgInventoryFragment_StorageProfile*
URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
		? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
		: nullptr;
	return ItemCDO
		? Cast<URpgInventoryFragment_StorageProfile>(
			ItemCDO->FindFragmentByClass(StaticClass()))
		: nullptr;
}

bool URpgInventoryFragment_StorageProfile::
IsDefinitionIntrinsicallyCollapsible(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
		? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
		: nullptr;
	if (!ItemCDO || ItemCDO->FindFragmentByClass(
			URpgInventoryFragment_ItemContainer::StaticClass()))
	{
		return false;
	}

	for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
	{
		if (Fragment && !Fragment->GetRuntimeStateIdentifier().IsNone())
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_StorageProfile::IsDataValid(
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

	if (!RpgInventoryStorageProfilePrivate::IsStrictChildOf(
			StorageDomainTag,
			RpgGameplayTags::Storage_Domain))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryStorageProfile",
			"InvalidStorageDomain",
			"Storage Profile requires a StorageDomainTag that is a strict child of Storage.Domain."));
	}

	if (!RpgInventoryStorageProfilePrivate::AreStrictChildrenOf(
			RequiredStorageCapabilityTags,
			RpgGameplayTags::Storage_Capability))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryStorageProfile",
			"InvalidRequiredCapabilities",
			"Every RequiredStorageCapabilityTags entry must be a strict child of Storage.Capability."));
	}

	if (StorageMode == ERpgInventoryStorageMode::BulkResource)
	{
		if (BulkCapacityCost <= 0)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"InvalidBulkCapacityCost",
				"BulkResource profiles require BulkCapacityCost to be at least one capacity point per stored unit."));
		}
		if (StorageDomainTag != RpgGameplayTags::Storage_Domain_Materials)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"BulkOutsideMaterials",
				"BulkResource profiles must target Storage.Domain.Materials."));
		}
		const URpgInventoryItemDefinition* ItemDefinition =
			GetTypedOuter<URpgInventoryItemDefinition>();
		if (ItemDefinition && !IsDefinitionIntrinsicallyCollapsible(
				ItemDefinition->GetClass()))
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"BulkDefinitionHasRuntimeIdentity",
				"BulkResource definitions cannot provide item-owned containers or fragment runtime-state payloads because definition/count storage would discard them."));
		}
	}
	else if (bCanAutoDeposit || bCanCraftFromNetwork)
	{
		AddError(NSLOCTEXT(
			"RpgInventoryStorageProfile",
			"InstanceModeUsesBulkConvenience",
			"Only BulkResource profiles may enable auto-deposit or definition/count crafting from the network. Grid and contained items require concrete instance handling."));
	}

	if (StorageMode == ERpgInventoryStorageMode::SpecialContainedItem &&
		StorageDomainTag != RpgGameplayTags::Storage_Domain_RiftContainment)
	{
		AddError(NSLOCTEXT(
			"RpgInventoryStorageProfile",
			"ContainedOutsideRift",
			"SpecialContainedItem profiles must target Storage.Domain.RiftContainment."));
	}
	if (StorageMode == ERpgInventoryStorageMode::GridItem &&
		(StorageDomainTag == RpgGameplayTags::Storage_Domain_Materials ||
		 StorageDomainTag == RpgGameplayTags::Storage_Domain_RiftContainment))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryStorageProfile",
			"GridUsesReservedDomain",
			"GridItem profiles cannot target the bulk Materials domain or the special RiftContainment domain."));
	}

	const URpgInventoryItemDefinition* ItemDefinition =
		GetTypedOuter<URpgInventoryItemDefinition>();
	if (ItemDefinition)
	{
		if (RequiresContainment() &&
			ItemDefinition->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"ContainedItemOwnsContainer",
				"SpecialContainedItem definitions cannot own item containers in Rift Containment V1 because stabilization/extraction operates on exactly one sealed leaf instance."));
		}

		if (RpgInventoryStorageProfilePrivate::CountFragments<
				URpgInventoryFragment_StorageProfile>(ItemDefinition) != 1)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"DuplicateStorageProfiles",
				"An item definition containing storage metadata must contain exactly one Storage Profile fragment."));
		}

		const bool bHasContainmentProfile =
			RpgInventoryStorageProfilePrivate::CountFragments<
				URpgInventoryFragment_ContainmentProfile>(ItemDefinition) > 0;
		if (RequiresContainment() && !bHasContainmentProfile)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"MissingContainmentProfile",
				"SpecialContainedItem storage mode requires exactly one Containment Profile fragment."));
		}
		else if (!RequiresContainment() && bHasContainmentProfile)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryStorageProfile",
				"UnexpectedContainmentProfile",
				"Containment Profile is only valid when StorageMode is SpecialContainedItem."));
		}
	}

	return Result;
}
#endif
