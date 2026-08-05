#include "RpgInventoryFragment_ContainmentProfile.h"

#include "RpgInventoryFragment_StorageProfile.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_ContainmentProfile)

namespace RpgInventoryContainmentProfilePrivate
{
	const FName RuntimeStatePayloadId(
		TEXT("Inventory.Storage.ContainmentState"));
	constexpr int32 RuntimeStatePayloadVersion = 1;

	bool IsFiniteNonNegative(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f;
	}

	bool AreStrictCapabilityChildren(const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid() ||
				Tag == RpgGameplayTags::Storage_Capability ||
				!Tag.MatchesTag(RpgGameplayTags::Storage_Capability))
			{
				return false;
			}
		}
		return true;
	}

	bool AreStrictDomainChildren(const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid() ||
				Tag == RpgGameplayTags::Storage_Domain ||
				!Tag.MatchesTag(RpgGameplayTags::Storage_Domain) ||
				Tag == RpgGameplayTags::Storage_Domain_Materials ||
				Tag == RpgGameplayTags::Storage_Domain_RiftContainment)
			{
				return false;
			}
		}
		return true;
	}

	bool AreStabilizationCostsValid(
		const TArray<FRpgInventoryContainmentResourceCost>& Costs)
	{
		TSet<TSubclassOf<URpgInventoryItemDefinition>> SeenDefinitions;
		for (const FRpgInventoryContainmentResourceCost& Cost : Costs)
		{
			const URpgInventoryFragment_StorageProfile* Profile =
				Cost.ItemDefinition
					? URpgInventoryFragment_StorageProfile::
						ResolveStorageProfile(Cost.ItemDefinition)
					: nullptr;
			if (!Cost.IsValid() || SeenDefinitions.Contains(Cost.ItemDefinition) ||
				!Profile || !Profile->IsBulkResource() ||
				Profile->StorageDomainTag !=
					RpgGameplayTags::Storage_Domain_Materials)
			{
				return false;
			}
			SeenDefinitions.Add(Cost.ItemDefinition);
		}
		return true;
	}

	bool IsValidExtractionOutput(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		if (!ItemDefinition)
		{
			return false;
		}
		const URpgInventoryFragment_StorageProfile* Profile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				ItemDefinition);
		return Profile && Profile->IsBulkResource() &&
			Profile->StorageDomainTag ==
				RpgGameplayTags::Storage_Domain_Materials;
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

FName URpgInventoryFragment_ContainmentProfile::GetRuntimeStateIdentifier() const
{
	return RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadId;
}

int32 URpgInventoryFragment_ContainmentProfile::GetRuntimeStateVersion() const
{
	return RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadVersion;
}

bool URpgInventoryFragment_ContainmentProfile::ExportRuntimeState(
	const URpgInventoryItemInstance* Instance,
	FRpgInventoryFragmentStatePayload& OutPayload) const
{
	if (!Instance)
	{
		return false;
	}

	OutPayload.FragmentId =
		RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadId;
	OutPayload.Version =
		RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadVersion;
	OutPayload.Payload.Reset();
	FMemoryWriter Writer(OutPayload.Payload, true);
	uint8 SerializedState =
		static_cast<uint8>(Instance->GetContainmentState());
	Writer << SerializedState;
	return !Writer.IsError();
}

bool URpgInventoryFragment_ContainmentProfile::ValidateRuntimeState(
	const URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	if (!Instance ||
		Payload.FragmentId !=
			RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadId ||
		Payload.Version !=
			RpgInventoryContainmentProfilePrivate::RuntimeStatePayloadVersion)
	{
		return false;
	}

	TArray<uint8> PayloadCopy = Payload.Payload;
	FMemoryReader Reader(PayloadCopy, true);
	uint8 SerializedState = MAX_uint8;
	Reader << SerializedState;
	return !Reader.IsError() && Reader.Tell() == PayloadCopy.Num() &&
		SerializedState <= static_cast<uint8>(
			ERpgInventoryContainmentState::Stabilized);
}

bool URpgInventoryFragment_ContainmentProfile::ImportRuntimeState(
	URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	if (!ValidateRuntimeState(Instance, Payload))
	{
		return false;
	}

	TArray<uint8> PayloadCopy = Payload.Payload;
	FMemoryReader Reader(PayloadCopy, true);
	uint8 SerializedState = 0;
	Reader << SerializedState;
	return !Reader.IsError() && Instance->SetContainmentState(
		static_cast<ERpgInventoryContainmentState>(SerializedState));
}

void URpgInventoryFragment_ContainmentProfile::CopyRuntimeState(
	const URpgInventoryItemInstance* Source,
	URpgInventoryItemInstance* Target) const
{
	if (Source && Target)
	{
		Target->SetContainmentState(Source->GetContainmentState());
	}
}

bool URpgInventoryFragment_ContainmentProfile::IsStructurallyValid() const
{
	const URpgInventoryItemDefinition* ItemDefinition =
		GetTypedOuter<URpgInventoryItemDefinition>();
	return RequiredSealedSlots == 1 &&
		!bRequiresQuarantine &&
		(!ItemDefinition || !ItemDefinition->FindFragmentByClass(
			URpgInventoryFragment_ItemContainer::StaticClass())) &&
		RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			RequiredContainmentStrength) &&
		RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			RequiredCorruptionProtection) &&
		RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			InstabilityValue) &&
		RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			ContainmentStrain) &&
		RpgInventoryContainmentProfilePrivate::AreStrictCapabilityChildren(
			RequiredContainmentCapabilityTags) &&
		RpgInventoryContainmentProfilePrivate::AreStrictDomainChildren(
			AllowedStabilizedDestinationDomains) &&
		RpgInventoryContainmentProfilePrivate::AreStabilizationCostsValid(
			StabilizationCosts) &&
		ExtractionStrain >= 0 && ExtractionStrain <= 100 &&
		((ExtractionOutputDefinition != nullptr && ExtractionOutputCount > 0 &&
			RpgInventoryContainmentProfilePrivate::IsValidExtractionOutput(
				ExtractionOutputDefinition)) ||
			(ExtractionOutputDefinition == nullptr &&
				ExtractionOutputCount == 0 && ExtractionStrain == 0));
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_ContainmentProfile::IsDataValid(
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

	if (RequiredSealedSlots != 1)
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"InvalidSealedSlots",
			"Rift Containment V1 requires exactly one sealed slot per non-stackable 1x1 item instance."));
	}

	if (bRequiresQuarantine)
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"UnsupportedQuarantine",
			"Quarantine placement is not implemented in Rift Containment V1; leave bRequiresQuarantine disabled."));
	}

	if (!RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			RequiredContainmentStrength) ||
		!RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			RequiredCorruptionProtection) ||
		!RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			InstabilityValue) ||
		!RpgInventoryContainmentProfilePrivate::IsFiniteNonNegative(
			ContainmentStrain))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"NegativeContainmentValues",
			"Containment strength, corruption protection, instability, and strain values must be finite and non-negative."));
	}

	if (!RpgInventoryContainmentProfilePrivate::AreStrictCapabilityChildren(
			RequiredContainmentCapabilityTags))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"InvalidContainmentCapabilities",
			"Every RequiredContainmentCapabilityTags entry must be a strict child of Storage.Capability."));
	}

	if (!RpgInventoryContainmentProfilePrivate::AreStrictDomainChildren(
			AllowedStabilizedDestinationDomains))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"InvalidStabilizedDestinationDomains",
			"Every stabilized destination must be an instance-preserving Storage.Domain child; Materials and RiftContainment are not valid exit destinations."));
	}

	TSet<TSubclassOf<URpgInventoryItemDefinition>> SeenStabilizationDefinitions;
	for (int32 CostIndex = 0; CostIndex < StabilizationCosts.Num(); ++CostIndex)
	{
		const FRpgInventoryContainmentResourceCost& Cost =
			StabilizationCosts[CostIndex];
		if (!Cost.IsValid())
		{
			AddError(FText::Format(
				NSLOCTEXT(
					"RpgInventoryContainmentProfile",
					"InvalidStabilizationCost",
					"StabilizationCosts[{0}] requires an item definition and a positive count."),
				FText::AsNumber(CostIndex)));
			continue;
		}

		if (SeenStabilizationDefinitions.Contains(Cost.ItemDefinition))
		{
			AddError(FText::Format(
				NSLOCTEXT(
					"RpgInventoryContainmentProfile",
					"DuplicateStabilizationCost",
					"StabilizationCosts[{0}] repeats item definition '{1}'. Combine duplicate rows into one deterministic count."),
				FText::AsNumber(CostIndex),
				FText::FromString(GetPathNameSafe(Cost.ItemDefinition.Get()))));
			continue;
		}
		SeenStabilizationDefinitions.Add(Cost.ItemDefinition);
		const URpgInventoryFragment_StorageProfile* CostProfile =
			URpgInventoryFragment_StorageProfile::ResolveStorageProfile(
				Cost.ItemDefinition);
		if (!CostProfile || !CostProfile->IsBulkResource() ||
			CostProfile->StorageDomainTag !=
				RpgGameplayTags::Storage_Domain_Materials)
		{
			AddError(FText::Format(
				NSLOCTEXT(
					"RpgInventoryContainmentProfile",
					"NonBulkStabilizationCost",
					"StabilizationCosts[{0}] must reference an explicit Storage.Domain.Materials BulkResource definition."),
				FText::AsNumber(CostIndex)));
		}
	}

	const bool bHasExtractionDefinition = ExtractionOutputDefinition != nullptr;
	if (ExtractionStrain < 0 || ExtractionStrain > 100)
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"InvalidExtractionStrain",
			"ExtractionStrain must be an integer percentage in the inclusive range 0..100."));
	}

	if (bHasExtractionDefinition != (ExtractionOutputCount > 0) ||
		(!bHasExtractionDefinition && ExtractionStrain != 0))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"IncompleteExtractionContract",
			"Extraction must configure both ExtractionOutputDefinition and a positive ExtractionOutputCount. Disabled extraction requires no output and zero ExtractionStrain."));
	}
	else if (bHasExtractionDefinition &&
		!RpgInventoryContainmentProfilePrivate::IsValidExtractionOutput(
			ExtractionOutputDefinition))
	{
		AddError(NSLOCTEXT(
			"RpgInventoryContainmentProfile",
			"ExtractionOutputNotBulk",
			"ExtractionOutputDefinition must be an explicit Storage.Domain.Materials BulkResource so the deterministic essence can enter the normal material flow."));
	}

	const URpgInventoryItemDefinition* ItemDefinition =
		GetTypedOuter<URpgInventoryItemDefinition>();
	if (ItemDefinition)
	{
		if (ItemDefinition->FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			AddError(NSLOCTEXT(
				"RpgInventoryContainmentProfile",
				"ContainedItemOwnsContainer",
				"Rift Containment V1 accepts only sealed leaf instances. A contained definition cannot own nested item containers whose contents extraction would otherwise destroy."));
		}

		if (RpgInventoryContainmentProfilePrivate::CountFragments<
				URpgInventoryFragment_ContainmentProfile>(ItemDefinition) != 1)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryContainmentProfile",
				"DuplicateContainmentProfiles",
				"A contained-item definition must contain exactly one Containment Profile fragment."));
		}

		const URpgInventoryFragment_StorageProfile* StorageProfile =
			Cast<URpgInventoryFragment_StorageProfile>(
				ItemDefinition->FindFragmentByClass(
					URpgInventoryFragment_StorageProfile::StaticClass()));
		if (!StorageProfile || !StorageProfile->RequiresContainment())
		{
			AddError(NSLOCTEXT(
				"RpgInventoryContainmentProfile",
				"MissingSpecialStorageMode",
				"Containment Profile requires exactly one Storage Profile configured as SpecialContainedItem."));
		}

		const URpgInventoryFragment_SpatialItem* SpatialProfile =
			Cast<URpgInventoryFragment_SpatialItem>(
				ItemDefinition->FindFragmentByClass(
					URpgInventoryFragment_SpatialItem::StaticClass()));
		if (!SpatialProfile || SpatialProfile->Footprint.Width != 1 ||
			SpatialProfile->Footprint.Height != 1)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryContainmentProfile",
				"ContainedItemNotOneByOne",
				"Rift Containment V1 only accepts an explicit 1x1 Spatial Item footprint."));
		}

		const URpgInventoryFragment_ItemTraits* ItemTraits =
			Cast<URpgInventoryFragment_ItemTraits>(
				ItemDefinition->FindFragmentByClass(
					URpgInventoryFragment_ItemTraits::StaticClass()));
		if (!ItemTraits || ItemTraits->bCanStack ||
			ItemTraits->GetMaxStackSize() != 1)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryContainmentProfile",
				"ContainedItemStackable",
				"Rift Containment V1 requires explicit non-stackable Item Traits with a maximum stack size of one."));
		}
	}

	return Result;
}
#endif
