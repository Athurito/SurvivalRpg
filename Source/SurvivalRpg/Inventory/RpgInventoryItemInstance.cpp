// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemDefinition.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemInstance)

class FLifetimeProperty;

namespace RpgInventoryItemInstance
{
	const FName CoreStatTagsPayloadId(TEXT("Inventory.Core.StatTags"));
	constexpr int32 LegacyCoreStatTagsPayloadVersion = 1;
	constexpr int32 CoreStatTagsPayloadVersion = 2;
	constexpr int32 MaxSavedStatTagCount = 4096;
}

URpgInventoryItemInstance::URpgInventoryItemInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InitializePersistentId();
}

void URpgInventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemId);
	DOREPLIFETIME(ThisClass, StatTags);
	DOREPLIFETIME(ThisClass, ItemDef);
}

void URpgInventoryItemInstance::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	using namespace UE::Net;

	// Build descriptors and allocate PropertyReplicationFragments for this object
	FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}

void URpgInventoryItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	if (!HasAuthorityForMutation())
	{
		return;
	}

	StatTags.AddStack(Tag, StackCount);
}

void URpgInventoryItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	if (!HasAuthorityForMutation())
	{
		return;
	}

	StatTags.RemoveStack(Tag, StackCount);
}

int32 URpgInventoryItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

bool URpgInventoryItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}

bool URpgInventoryItemInstance::InitializePersistentId()
{
	if (ItemId.IsValid())
	{
		return true;
	}

	if (!HasAuthorityForMutation())
	{
		return false;
	}

	ItemId = FRpgInventoryItemId::NewId();
	return ItemId.IsValid();
}

bool URpgInventoryItemInstance::RestoreItemId(const FRpgInventoryItemId& InItemId)
{
	if (!HasAuthorityForMutation() || !InItemId.IsValid())
	{
		return false;
	}

	ItemId = InItemId;
	return true;
}

bool URpgInventoryItemInstance::CopyRuntimeStateFrom(
	const URpgInventoryItemInstance* Source,
	bool bPreserveItemId)
{
	if (!HasAuthorityForMutation() || !Source || Source == this || !Source->ItemDef || Source->ItemDef != ItemDef)
	{
		return false;
	}

	if (bPreserveItemId && !RestoreItemId(Source->ItemId))
	{
		return false;
	}

	StatTags = Source->StatTags;
	StatTags.RebuildTagToCountMap();
	StatTags.MarkArrayDirty();

	const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef);
	if (!ItemCDO)
	{
		return false;
	}

	for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
	{
		if (Fragment)
		{
			Fragment->CopyRuntimeState(Source, this);
		}
	}

	return true;
}

bool URpgInventoryItemInstance::IsStackCompatibleWith(const URpgInventoryItemInstance* Other) const
{
	if (!Other || Other == this || !ItemDef || ItemDef != Other->ItemDef)
	{
		return false;
	}

	if (!StatTags.HasSameStacks(Other->StatTags))
	{
		return false;
	}

	const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef);
	if (!ItemCDO)
	{
		return false;
	}

	for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
	{
		if (Fragment && !Fragment->AreInstancesStackCompatible(this, Other))
		{
			return false;
		}
	}

	return true;
}

bool URpgInventoryItemInstance::ExportRuntimeState(
	TArray<FRpgInventoryFragmentStatePayload>& OutPayloads) const
{
	if (!ItemDef)
	{
		return false;
	}

	TArray<FRpgInventoryFragmentStatePayload> ExportedPayloads;
	TSet<FName> PayloadIds;

	FRpgInventoryFragmentStatePayload& CorePayload = ExportedPayloads.AddDefaulted_GetRef();
	CorePayload.FragmentId = RpgInventoryItemInstance::CoreStatTagsPayloadId;
	CorePayload.Version = RpgInventoryItemInstance::CoreStatTagsPayloadVersion;
	{
		FMemoryWriter Writer(CorePayload.Payload, true);
		TArray<TPair<FGameplayTag, int32>> SemanticStacks;
		StatTags.GetSemanticStacks(SemanticStacks);
		int32 StackCount = SemanticStacks.Num();
		Writer << StackCount;
		for (const TPair<FGameplayTag, int32>& Pair : SemanticStacks)
		{
			FString TagName = Pair.Key.ToString();
			int32 Count = Pair.Value;
			Writer << TagName;
			Writer << Count;
		}
		if (Writer.IsError())
		{
			return false;
		}
	}
	PayloadIds.Add(CorePayload.FragmentId);

	const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef);
	if (!ItemCDO)
	{
		return false;
	}

	for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
	{
		if (!Fragment)
		{
			continue;
		}

		const FName PayloadId = Fragment->GetRuntimeStateIdentifier();
		if (PayloadId.IsNone())
		{
			continue;
		}

		if (PayloadIds.Contains(PayloadId) || Fragment->GetRuntimeStateVersion() <= 0)
		{
			return false;
		}

		FRpgInventoryFragmentStatePayload FragmentPayload;
		FragmentPayload.FragmentId = PayloadId;
		FragmentPayload.Version = Fragment->GetRuntimeStateVersion();
		if (!Fragment->ExportRuntimeState(this, FragmentPayload))
		{
			return false;
		}

		if (FragmentPayload.FragmentId != PayloadId || FragmentPayload.Version <= 0)
		{
			return false;
		}

		PayloadIds.Add(PayloadId);
		ExportedPayloads.Add(MoveTemp(FragmentPayload));
	}

	OutPayloads = MoveTemp(ExportedPayloads);
	return true;
}

bool URpgInventoryItemInstance::ImportRuntimeState(
	const TArray<FRpgInventoryFragmentStatePayload>& Payloads)
{
	if (!HasAuthorityForMutation() || !ItemDef || Payloads.IsEmpty())
	{
		return false;
	}

	const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef);
	if (!ItemCDO)
	{
		return false;
	}

	auto FindStateFragment = [ItemCDO](FName PayloadId) -> const URpgInventoryItemFragment*
	{
		for (const URpgInventoryItemFragment* Fragment : ItemCDO->Fragments)
		{
			if (Fragment && Fragment->GetRuntimeStateIdentifier() == PayloadId)
			{
				return Fragment;
			}
		}
		return nullptr;
	};

	TSet<FName> PayloadIds;
	FGameplayTagStackContainer StagedStatTags;
	bool bHasCorePayload = false;
	for (const FRpgInventoryFragmentStatePayload& Payload : Payloads)
	{
		if (Payload.FragmentId.IsNone() || Payload.Version <= 0 || PayloadIds.Contains(Payload.FragmentId))
		{
			return false;
		}
		PayloadIds.Add(Payload.FragmentId);

		if (Payload.FragmentId == RpgInventoryItemInstance::CoreStatTagsPayloadId)
		{
			if (Payload.Version == RpgInventoryItemInstance::LegacyCoreStatTagsPayloadVersion)
			{
				TArray<uint8> PayloadCopy = Payload.Payload;
				FMemoryReader Reader(PayloadCopy, true);
				FGameplayTagStackContainer::StaticStruct()->SerializeItem(Reader, &StagedStatTags, nullptr);
				if (Reader.IsError() || Reader.Tell() != PayloadCopy.Num())
				{
					return false;
				}
			}
			else if (Payload.Version == RpgInventoryItemInstance::CoreStatTagsPayloadVersion)
			{
				TArray<uint8> PayloadCopy = Payload.Payload;
				FMemoryReader Reader(PayloadCopy, true);
				int32 StackCount = 0;
				Reader << StackCount;
				if (Reader.IsError() || StackCount < 0 || StackCount > RpgInventoryItemInstance::MaxSavedStatTagCount)
				{
					return false;
				}

				TSet<FGameplayTag> ImportedTags;
				for (int32 Index = 0; Index < StackCount; ++Index)
				{
					FString TagName;
					int32 Count = 0;
					Reader << TagName;
					Reader << Count;
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
					if (Reader.IsError() || !Tag.IsValid() || Count <= 0 || ImportedTags.Contains(Tag))
					{
						return false;
					}

					ImportedTags.Add(Tag);
					StagedStatTags.AddStack(Tag, Count);
				}
				if (Reader.IsError() || Reader.Tell() != PayloadCopy.Num())
				{
					return false;
				}
			}
			else
			{
				return false;
			}

			StagedStatTags.RebuildTagToCountMap();
			bHasCorePayload = true;
			continue;
		}

		const URpgInventoryItemFragment* Fragment = FindStateFragment(Payload.FragmentId);
		if (!Fragment || !Fragment->ValidateRuntimeState(this, Payload))
		{
			return false;
		}
	}

	if (!bHasCorePayload)
	{
		return false;
	}

	StatTags = MoveTemp(StagedStatTags);
	StatTags.RebuildTagToCountMap();
	StatTags.MarkArrayDirty();

	for (const FRpgInventoryFragmentStatePayload& Payload : Payloads)
	{
		if (Payload.FragmentId == RpgInventoryItemInstance::CoreStatTagsPayloadId)
		{
			continue;
		}

		const URpgInventoryItemFragment* Fragment = FindStateFragment(Payload.FragmentId);
		if (!Fragment || !Fragment->ImportRuntimeState(this, Payload))
		{
			return false;
		}
	}

	return true;
}

void URpgInventoryItemInstance::SetItemDef(TSubclassOf<URpgInventoryItemDefinition> InDef)
{
	if (!HasAuthorityForMutation())
	{
		return;
	}

	ItemDef = InDef;
	InitializePersistentId();
}

bool URpgInventoryItemInstance::HasAuthorityForMutation() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	if (const AActor* OwningActor = GetTypedOuter<AActor>())
	{
		return OwningActor->HasAuthority();
	}

	// Transient instances without an actor outer are permitted for deterministic automation tests and import staging.
	return GetWorld() == nullptr;
}

const URpgInventoryItemFragment* URpgInventoryItemInstance::FindFragmentByClass(TSubclassOf<URpgInventoryItemFragment> FragmentClass) const
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		return GetDefault<URpgInventoryItemDefinition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}

	return nullptr;
}


