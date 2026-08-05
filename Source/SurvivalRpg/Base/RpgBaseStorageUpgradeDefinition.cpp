#include "RpgBaseStorageUpgradeDefinition.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseStorageUpgradeDefinition)

#if WITH_EDITOR
namespace RpgBaseStorageUpgradeDefinitionPrivate
{
	bool IsFiniteNonNegative(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f;
	}

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

	bool AreNetworkEffectsNonNegative(const URpgBaseStorageUpgradeDefinition& Upgrade)
	{
		return Upgrade.CapacityEffect.AdditionalCapacity >= 0 &&
			Upgrade.GridEffect.AdditionalColumns >= 0 &&
			Upgrade.GridEffect.AdditionalRows >= 0 &&
			Upgrade.ContainmentEffect.AdditionalSealedSlots >= 0 &&
			IsFiniteNonNegative(Upgrade.ContainmentEffect.ContainmentStrengthDelta) &&
			IsFiniteNonNegative(Upgrade.ContainmentEffect.CorruptionProtectionDelta) &&
			IsFiniteNonNegative(Upgrade.StrainEffect.AddedStrain) &&
			IsFiniteNonNegative(Upgrade.StrainEffect.StrainToleranceDelta) &&
			IsFiniteNonNegative(Upgrade.StrainEffect.StrainMitigation);
	}
}

EDataValidationResult URpgBaseStorageUpgradeDefinition::IsDataValid(
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

	if (!RpgBaseStorageUpgradeDefinitionPrivate::IsStrictChildOf(
			TargetDomainTag,
			RpgGameplayTags::Storage_Domain))
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"InvalidTargetDomain",
			"TargetDomainTag is required and must be a strict child of Storage.Domain."));
	}

	if (!RpgBaseStorageUpgradeDefinitionPrivate::AreStrictChildrenOf(
			AllowedStationTags,
			RpgGameplayTags::Base_Storage_Station))
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"InvalidAllowedStationTags",
			"Every AllowedStationTags entry must be a strict child of Base.Storage.Station."));
	}

	if (TargetAnchorId.IsNone())
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"MissingTargetAnchor",
			"TargetAnchorId is required so installed upgrade state can be rebuilt and persisted deterministically."));
	}

	if (!RpgBaseStorageUpgradeDefinitionPrivate::AreStrictChildrenOf(
			RequiredKnowledgeTags,
			RpgGameplayTags::Storage_Knowledge))
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"InvalidRequiredKnowledge",
			"Every RequiredKnowledgeTags entry must be a strict child of Storage.Knowledge."));
	}

	if (!RpgBaseStorageUpgradeDefinitionPrivate::AreStrictChildrenOf(
			RequiredInstalledCapabilityTags,
			RpgGameplayTags::Storage_Capability) ||
		!RpgBaseStorageUpgradeDefinitionPrivate::AreStrictChildrenOf(
			GrantedCapabilityTags,
			RpgGameplayTags::Storage_Capability))
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"InvalidCapabilityTags",
			"RequiredInstalledCapabilityTags and GrantedCapabilityTags must contain only strict children of Storage.Capability."));
	}

	if (!RpgBaseStorageUpgradeDefinitionPrivate::AreNetworkEffectsNonNegative(
			*this))
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"NegativeNetworkEffect",
			"Capacity and grid effects must be non-negative; containment and strain effects must also be finite. Decommission applies their inverse transactionally."));
	}

	if (!CapacityEffect.IsNeutral() &&
		TargetDomainTag != RpgGameplayTags::Storage_Domain_Materials)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"CapacityEffectOutsideMaterials",
			"CapacityEffect is implemented only for Storage.Domain.Materials upgrades."));
	}

	if (!GridEffect.IsNeutral() &&
		TargetDomainTag != RpgGameplayTags::Storage_Domain_Armory)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"GridEffectOutsideArmory",
			"GridEffect is implemented only for Storage.Domain.Armory upgrades."));
	}

	if (!ContainmentEffect.IsNeutral() &&
		TargetDomainTag != RpgGameplayTags::Storage_Domain_RiftContainment)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"ContainmentEffectOutsideRift",
			"ContainmentEffect is implemented only for Storage.Domain.RiftContainment upgrades."));
	}

	if (!StrainEffect.IsNeutral() &&
		TargetDomainTag != RpgGameplayTags::Storage_Domain_RiftContainment)
	{
		AddError(NSLOCTEXT(
			"RpgBaseStorageUpgradeDefinition",
			"StrainEffectOutsideRift",
			"Strain effects are implemented only for Storage.Domain.RiftContainment upgrades."));
	}

	const auto ValidateResourceRows =
		[&AddError](
			const TArray<FRpgBaseStorageUpgradeCost>& Rows,
			const TCHAR* ArrayLabel)
	{
		TSet<TSubclassOf<URpgInventoryItemDefinition>> SeenDefinitions;
		for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
		{
			const FRpgBaseStorageUpgradeCost& Row = Rows[RowIndex];
			if (!Row.ItemDefinition || Row.Count <= 0)
			{
				AddError(FText::FromString(FString::Printf(
					TEXT("%s[%d] requires an item definition and a positive count."),
					ArrayLabel,
					RowIndex)));
				continue;
			}

			if (SeenDefinitions.Contains(Row.ItemDefinition))
			{
				AddError(FText::FromString(FString::Printf(
					TEXT("%s[%d] repeats item definition '%s'; combine duplicate rows into one explicit count."),
					ArrayLabel,
					RowIndex,
					*GetPathNameSafe(Row.ItemDefinition.Get()))));
				continue;
			}
			SeenDefinitions.Add(Row.ItemDefinition);

			const URpgInventoryFragment_StorageProfile* Profile =
				URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
					Row.ItemDefinition);
			if (!Profile || !Profile->IsBulkResource() ||
				Profile->StorageDomainTag !=
					RpgGameplayTags::Storage_Domain_Materials)
			{
				AddError(FText::FromString(FString::Printf(
					TEXT("%s[%d] must reference an explicit Storage.Domain.Materials BulkResource definition."),
					ArrayLabel,
					RowIndex)));
			}
		}
	};
	ValidateResourceRows(Costs, TEXT("Costs"));
	ValidateResourceRows(
		DecommissionRefunds,
		TEXT("DecommissionRefunds"));

	return Result;
}
#endif
