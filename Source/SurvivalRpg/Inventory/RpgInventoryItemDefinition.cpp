// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemDefinition)

#define LOCTEXT_NAMESPACE "RpgInventoryItemDefinition"

namespace
{
#if WITH_EDITOR
	FText GetInventoryDefinitionValidationPath(const UObject* Definition)
	{
		const FString ObjectPath = GetPathNameSafe(Definition);
		const FString PackagePath =
			Definition && Definition->GetOutermost()
				? Definition->GetOutermost()->GetName()
				: FString();
		return FText::FromString(
			PackagePath.IsEmpty() || PackagePath == TEXT("/Engine/Transient")
				? ObjectPath
				: PackagePath);
	}
#endif
}

//////////////////////////////////////////////////////////////////////
// URpgInventoryItemFragment

FName URpgInventoryItemFragment::GetRuntimeStateIdentifier() const
{
	return NAME_None;
}

int32 URpgInventoryItemFragment::GetRuntimeStateVersion() const
{
	return 1;
}

bool URpgInventoryItemFragment::ExportRuntimeState(
	const URpgInventoryItemInstance* Instance,
	FRpgInventoryFragmentStatePayload& OutPayload) const
{
	return false;
}

bool URpgInventoryItemFragment::ValidateRuntimeState(
	const URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	return false;
}

bool URpgInventoryItemFragment::ImportRuntimeState(
	URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	return false;
}

void URpgInventoryItemFragment::CopyRuntimeState(
	const URpgInventoryItemInstance* Source,
	URpgInventoryItemInstance* Target) const
{
}

//////////////////////////////////////////////////////////////////////
// URpgInventoryItemDefinition

URpgInventoryItemDefinition::URpgInventoryItemDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const URpgInventoryItemFragment* URpgInventoryItemDefinition::FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const
{
	if (FragmentClass != nullptr)
	{
		for (URpgInventoryItemFragment* Fragment : Fragments)
		{
			if (Fragment && Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}

	return nullptr;
}

const URpgInventoryFragment_SpatialItem*
URpgInventoryItemDefinition::FindValidSpatialItemFragment() const
{
	const URpgInventoryFragment_SpatialItem* ResolvedFragment = nullptr;
	for (const URpgInventoryItemFragment* Fragment : Fragments)
	{
		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			Cast<URpgInventoryFragment_SpatialItem>(Fragment);
		if (!SpatialFragment)
		{
			continue;
		}

		if (ResolvedFragment || !SpatialFragment->Footprint.IsValid())
		{
			return nullptr;
		}
		ResolvedFragment = SpatialFragment;
	}

	return ResolvedFragment;
}

const URpgInventoryFragment_SpatialItem*
URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	const URpgInventoryItemDefinition* ItemCDO = ItemDefinition
		? GetDefault<URpgInventoryItemDefinition>(ItemDefinition)
		: nullptr;
	return ItemCDO ? ItemCDO->FindValidSpatialItemFragment() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryItemDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const FText DefinitionPath = GetInventoryDefinitionValidationPath(this);

	TArray<int32> SpatialFragmentIndices;
	TMap<FName, int32> FirstRuntimeStateFragmentIndices;
	for (int32 FragmentIndex = 0; FragmentIndex < Fragments.Num(); ++FragmentIndex)
	{
		const URpgInventoryItemFragment* Fragment = Fragments[FragmentIndex];
		if (!Fragment)
		{
			continue;
		}

		// Validate every fragment rather than only the first fragment of a known type.
		// This keeps runtime-state providers extensible and catches every itemization fragment.
		Result = CombineDataValidationResults(
			Result,
			Fragment->IsDataValid(Context));

		const FName RuntimeStateIdentifier = Fragment->GetRuntimeStateIdentifier();
		if (!RuntimeStateIdentifier.IsNone())
		{
			const int32 RuntimeStateVersion = Fragment->GetRuntimeStateVersion();
			if (RuntimeStateVersion <= 0)
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"InvalidRuntimeStateFragmentVersion",
							"Item definition '{0}' has runtime-state fragment '{1}' at Fragments[{2}] with schema version {3}. "
							"Runtime-state fragments must publish a stable positive version."),
						DefinitionPath,
						FText::FromName(RuntimeStateIdentifier),
						FText::AsNumber(FragmentIndex),
						FText::AsNumber(RuntimeStateVersion)));
			}

			if (const int32* FirstFragmentIndex =
				FirstRuntimeStateFragmentIndices.Find(RuntimeStateIdentifier))
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"DuplicateRuntimeStateFragmentIdentifier",
							"Item definition '{0}' repeats runtime-state identifier '{1}' at Fragments[{2}]; it was first "
							"declared at Fragments[{3}]. Runtime-state identifiers must be unique because import/export "
							"rejects ambiguous payload ownership."),
						DefinitionPath,
						FText::FromName(RuntimeStateIdentifier),
						FText::AsNumber(FragmentIndex),
						FText::AsNumber(*FirstFragmentIndex)));
			}
			else
			{
				FirstRuntimeStateFragmentIndices.Add(
					RuntimeStateIdentifier,
					FragmentIndex);
			}
		}

		const URpgInventoryFragment_SpatialItem* SpatialFragment =
			Cast<URpgInventoryFragment_SpatialItem>(Fragment);
		if (SpatialFragment)
		{
			SpatialFragmentIndices.Add(FragmentIndex);
			if (!SpatialFragment->Footprint.IsValid())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"InvalidSpatialFootprint",
							"Item definition '{0}' has an invalid SpatialItem footprint {1} x {2} at Fragments[{3}]. "
							"Set both grid dimensions to a positive number of cells."),
						DefinitionPath,
						FText::AsNumber(SpatialFragment->Footprint.Width),
						FText::AsNumber(SpatialFragment->Footprint.Height),
						FText::AsNumber(FragmentIndex)));
			}
		}
	}

	if (SpatialFragmentIndices.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(
			FText::Format(
				LOCTEXT(
					"MissingSpatialFragment",
					"Item definition '{0}' has no SpatialItem fragment. Add exactly one SpatialItem fragment with a "
					"positive grid footprint; runtime placement otherwise fails closed."),
				DefinitionPath));
	}
	else if (SpatialFragmentIndices.Num() > 1)
	{
		TArray<FString> IndexStrings;
		IndexStrings.Reserve(SpatialFragmentIndices.Num());
		for (const int32 FragmentIndex : SpatialFragmentIndices)
		{
			IndexStrings.Add(FString::FromInt(FragmentIndex));
		}

		Result = EDataValidationResult::Invalid;
		Context.AddError(
			FText::Format(
				LOCTEXT(
					"DuplicateSpatialFragments",
					"Item definition '{0}' has {1} SpatialItem fragments at indices [{2}]. Keep exactly one; runtime "
					"placement rejects ambiguous footprints."),
				DefinitionPath,
				FText::AsNumber(SpatialFragmentIndices.Num()),
				FText::FromString(FString::Join(IndexStrings, TEXT(", ")))));
	}

	if (FindValidSpatialItemFragment() == nullptr)
	{
		Result = EDataValidationResult::Invalid;
	}

	const URpgInventoryFragment_ItemContainer* ContainerFragment =
		Cast<URpgInventoryFragment_ItemContainer>(
			FindFragmentByClass(
				URpgInventoryFragment_ItemContainer::StaticClass()));
	if (ContainerFragment &&
		!ContainerFragment->HasStructurallyValidProvidedContainers())
	{
		Result = EDataValidationResult::Invalid;
		const int32 FragmentIndex =
			Fragments.IndexOfByKey(ContainerFragment);
		TArray<FRpgInventoryItemContainerDefinition>
			ProvidedContainers;
		ContainerFragment->GetProvidedContainers(
			ProvidedContainers);
		TMap<FName, int32> FirstContainerIndices;
		for (int32 ContainerIndex = 0;
			ContainerIndex < ProvidedContainers.Num();
			++ContainerIndex)
		{
			const FRpgInventoryItemContainerDefinition& Container =
				ProvidedContainers[ContainerIndex];
			if (Container.ContainerId.IsNone())
			{
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"MissingProvidedContainerId",
							"Item definition '{0}' has no ContainerId at "
							"Fragments[{1}].ProvidedContainers[{2}]. Set a definition-local id."),
						DefinitionPath,
						FText::AsNumber(FragmentIndex),
						FText::AsNumber(ContainerIndex)));
			}
			else if (const int32* FirstIndex =
				FirstContainerIndices.Find(Container.ContainerId))
			{
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"DuplicateProvidedContainerId",
							"Item definition '{0}' repeats ContainerId '{1}' at "
							"Fragments[{2}].ProvidedContainers[{3}]; it was first declared at "
							"ProvidedContainers[{4}]. Keep ids unique within the ItemContainer fragment."),
						DefinitionPath,
						FText::FromName(Container.ContainerId),
						FText::AsNumber(FragmentIndex),
						FText::AsNumber(ContainerIndex),
						FText::AsNumber(*FirstIndex)));
			}
			else
			{
				FirstContainerIndices.Add(
					Container.ContainerId,
					ContainerIndex);
			}

			if (!Container.GridSize.IsValid())
			{
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"InvalidProvidedContainerGrid",
							"Item definition '{0}' has an invalid item-container grid {1} x {2} at "
							"Fragments[{3}].ProvidedContainers[{4}]. Set both dimensions to a positive number of cells."),
						DefinitionPath,
						FText::AsNumber(Container.GridSize.Width),
						FText::AsNumber(Container.GridSize.Height),
						FText::AsNumber(FragmentIndex),
						FText::AsNumber(ContainerIndex)));
			}
		}
	}

	const URpgInventoryFragment_EquippableItem* EquippableFragment =
		Cast<URpgInventoryFragment_EquippableItem>(
			FindFragmentByClass(
				URpgInventoryFragment_EquippableItem::StaticClass()));
	if (EquippableFragment)
	{
		const int32 EquippableFragmentIndex =
			Fragments.IndexOfByKey(EquippableFragment);
		const TSubclassOf<URpgEquipmentDefinition>
			EquipmentDefinitionClass =
				EquippableFragment->GetEquipmentDefinition();
		if (!EquipmentDefinitionClass)
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(
				FText::Format(
					LOCTEXT(
						"MissingEquipmentDefinition",
						"Item definition '{0}' has an effective EquippableItem fragment at Fragments[{1}] without an "
						"EquipmentDefinition. Assign the equipment definition or remove the fragment."),
					DefinitionPath,
					FText::AsNumber(EquippableFragmentIndex)));
		}
		else
		{
			const URpgEquipmentDefinition* EquipmentDefinition =
				GetDefault<URpgEquipmentDefinition>(
					EquipmentDefinitionClass);
			if (EquipmentDefinition &&
				EquipmentDefinition->AllowedSlots.IsEmpty())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(
					FText::Format(
						LOCTEXT(
							"EmptyEquippableAllowedSlots",
							"Item definition '{0}' has an effective EquippableItem fragment at Fragments[{1}] whose "
							"EquipmentDefinition '{2}' has empty AllowedSlots. Add at least one explicit destination "
							"slot or remove the EquippableItem fragment; runtime equipment placement fails closed."),
						DefinitionPath,
						FText::AsNumber(EquippableFragmentIndex),
						FText::FromString(
							GetPathNameSafe(
								EquipmentDefinitionClass.Get()))));
			}
		}
	}

	if (ContainerFragment && !EquippableFragment)
	{
		Context.AddWarning(
			FText::Format(
				LOCTEXT(
					"DefinitionlessContainerProvider",
					"Item definition '{0}' has an effective ItemContainer fragment at Fragments[{1}] but no effective "
					"EquippableItem fragment. It remains a valid portable or nested container, but is no longer "
					"eligible for a Gear provider slot. If it is intended to be worn, add an EquippableItem fragment "
					"with an EquipmentDefinition that explicitly allows the provider slot."),
				DefinitionPath,
				FText::AsNumber(
					Fragments.IndexOfByKey(ContainerFragment))));
	}

	return Result;
}
#endif

//////////////////////////////////////////////////////////////////////
// URpgInventoryItemDefinition

const URpgInventoryItemFragment* URpgInventoryFunctionLibrary::FindItemDefinitionFragment(TSubclassOf<URpgInventoryItemDefinition> ItemDef, TSubclassOf<URpgInventoryItemFragment> FragmentClass)
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		return GetDefault<URpgInventoryItemDefinition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
