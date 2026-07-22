// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryItemDefinition.h"

#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemDefinition)

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

