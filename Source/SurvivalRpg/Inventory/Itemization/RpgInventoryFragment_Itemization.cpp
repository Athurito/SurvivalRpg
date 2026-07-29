#include "RpgInventoryFragment_Itemization.h"

#include "RpgItemizationGameplayTags.h"
#include "RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_Itemization)

namespace RpgInventoryFragmentItemization
{
	const FName PayloadId(TEXT("Inventory.Itemization.State"));
	constexpr int32 PayloadVersion = 1;
	constexpr int32 MaximumSavedRolls = 128;

	void WriteTag(FArchive& Archive, const FGameplayTag& Tag)
	{
		FString TagName = Tag.ToString();
		Archive << TagName;
	}

	bool ReadTag(FArchive& Archive, FGameplayTag& OutTag)
	{
		FString TagName;
		Archive << TagName;
		OutTag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
		return !Archive.IsError() && OutTag.IsValid();
	}
}

bool URpgInventoryFragment_Itemization::IsItemizationStateCompatible(
	const FRpgItemizationState& State) const
{
	if (!State.IsStructurallyValid())
	{
		return false;
	}
	if (!State.bGenerated)
	{
		return true;
	}
	if (!ItemizationProfile || !ItemizationProfile->HasValidConfiguration() ||
		State.ItemLevel < ItemizationProfile->MinimumItemLevel ||
		State.ItemLevel > ItemizationProfile->MaximumItemLevel ||
		State.BaseStats.Num() != ItemizationProfile->BaseStats.Num())
	{
		return false;
	}

	TSet<FGameplayTag> ProfileBaseStats;
	for (const FRpgItemStatRollDefinition& Definition : ItemizationProfile->BaseStats)
	{
		ProfileBaseStats.Add(Definition.StatTag);
	}
	for (const FRpgRolledItemStat& Stat : State.BaseStats)
	{
		const FRpgItemStatRollDefinition* Definition = ItemizationProfile->BaseStats.FindByPredicate(
			[&Stat](const FRpgItemStatRollDefinition& Candidate)
			{
				return Candidate.StatTag == Stat.StatTag;
			});
		if (!Definition || !ProfileBaseStats.Contains(Stat.StatTag))
		{
			return false;
		}
		const float Minimum = Definition->MinimumValue.GetValueAtLevel(State.ItemLevel);
		const float Maximum = Definition->MaximumValue.GetValueAtLevel(State.ItemLevel);
		if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum > Maximum ||
			Stat.Value < Minimum - KINDA_SMALL_NUMBER || Stat.Value > Maximum + KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	if (State.Affixes.IsEmpty())
	{
		return true;
	}

	if (!ItemizationProfile->AffixPool)
	{
		return false;
	}
	TArray<const FRpgItemAffixDefinition*> EligibleAffixes;
	ItemizationProfile->AffixPool->GetEligibleAffixes(
		ItemizationProfile->ItemTags,
		EligibleAffixes);
	for (const FRpgRolledItemAffix& Roll : State.Affixes)
	{
		const FRpgItemAffixDefinition* Match = nullptr;
		for (const FRpgItemAffixDefinition* Definition : EligibleAffixes)
		{
			if (Definition && Definition->AffixId == Roll.AffixId)
			{
				Match = Definition;
				break;
			}
		}
		if (!Match || Match->StatTag != Roll.StatTag)
		{
			return false;
		}
		const float Minimum = Match->MinimumValue.GetValueAtLevel(State.ItemLevel);
		const float Maximum = Match->MaximumValue.GetValueAtLevel(State.ItemLevel);
		if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum > Maximum ||
			Roll.Value < Minimum - KINDA_SMALL_NUMBER || Roll.Value > Maximum + KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}
	return true;
}

bool URpgInventoryFragment_Itemization::IsPersistedItemizationStateCompatible(
	const FRpgItemizationState& State) const
{
	if (!State.IsStructurallyValid())
	{
		return false;
	}
	if (!State.bGenerated)
	{
		return true;
	}
	if (State.BaseStats.Num() > RpgInventoryFragmentItemization::MaximumSavedRolls ||
		State.Affixes.Num() > RpgInventoryFragmentItemization::MaximumSavedRolls)
	{
		return false;
	}

	for (const FRpgRolledItemStat& Stat : State.BaseStats)
	{
		if (!RpgItemizationGameplayTags::IsSupportedItemStat(Stat.StatTag))
		{
			return false;
		}
	}
	for (const FRpgRolledItemAffix& Affix : State.Affixes)
	{
		if (!RpgItemizationGameplayTags::IsSupportedItemStat(Affix.StatTag))
		{
			return false;
		}
	}
	return true;
}

FName URpgInventoryFragment_Itemization::GetRuntimeStateIdentifier() const
{
	return RpgInventoryFragmentItemization::PayloadId;
}

int32 URpgInventoryFragment_Itemization::GetRuntimeStateVersion() const
{
	return RpgInventoryFragmentItemization::PayloadVersion;
}

bool URpgInventoryFragment_Itemization::ExportRuntimeState(
	const URpgInventoryItemInstance* Instance,
	FRpgInventoryFragmentStatePayload& OutPayload) const
{
	if (!Instance || !IsPersistedItemizationStateCompatible(Instance->GetItemizationStateRef()))
	{
		return false;
	}

	OutPayload.FragmentId = RpgInventoryFragmentItemization::PayloadId;
	OutPayload.Version = RpgInventoryFragmentItemization::PayloadVersion;
	OutPayload.Payload.Reset();
	FMemoryWriter Writer(OutPayload.Payload, true);
	const FRpgItemizationState& State = Instance->GetItemizationStateRef();
	uint8 bGenerated = State.bGenerated ? 1 : 0;
	int32 ItemLevel = State.ItemLevel;
	uint8 Rarity = static_cast<uint8>(State.Rarity);
	Writer << bGenerated;
	Writer << ItemLevel;
	Writer << Rarity;

	int32 BaseStatCount = State.BaseStats.Num();
	Writer << BaseStatCount;
	for (const FRpgRolledItemStat& Stat : State.BaseStats)
	{
		RpgInventoryFragmentItemization::WriteTag(Writer, Stat.StatTag);
		float Value = Stat.Value;
		Writer << Value;
	}

	int32 AffixCount = State.Affixes.Num();
	Writer << AffixCount;
	for (const FRpgRolledItemAffix& Affix : State.Affixes)
	{
		FName AffixId = Affix.AffixId;
		Writer << AffixId;
		RpgInventoryFragmentItemization::WriteTag(Writer, Affix.StatTag);
		float Value = Affix.Value;
		Writer << Value;
	}
	return !Writer.IsError();
}

bool URpgInventoryFragment_Itemization::DeserializeState(
	const FRpgInventoryFragmentStatePayload& Payload,
	FRpgItemizationState& OutState) const
{
	OutState = FRpgItemizationState();
	if (Payload.FragmentId != RpgInventoryFragmentItemization::PayloadId ||
		Payload.Version != RpgInventoryFragmentItemization::PayloadVersion)
	{
		return false;
	}

	TArray<uint8> PayloadCopy = Payload.Payload;
	FMemoryReader Reader(PayloadCopy, true);
	uint8 bGenerated = 0;
	uint8 Rarity = 0;
	Reader << bGenerated;
	Reader << OutState.ItemLevel;
	Reader << Rarity;
	if (Reader.IsError() || bGenerated > 1 ||
		Rarity > static_cast<uint8>(ERpgItemRarity::Epic))
	{
		return false;
	}
	OutState.bGenerated = bGenerated != 0;
	OutState.Rarity = static_cast<ERpgItemRarity>(Rarity);

	int32 BaseStatCount = 0;
	Reader << BaseStatCount;
	if (Reader.IsError() || BaseStatCount < 0 ||
		BaseStatCount > RpgInventoryFragmentItemization::MaximumSavedRolls)
	{
		return false;
	}
	OutState.BaseStats.Reserve(BaseStatCount);
	for (int32 Index = 0; Index < BaseStatCount; ++Index)
	{
		FRpgRolledItemStat& Stat = OutState.BaseStats.AddDefaulted_GetRef();
		if (!RpgInventoryFragmentItemization::ReadTag(Reader, Stat.StatTag))
		{
			return false;
		}
		Reader << Stat.Value;
	}

	int32 AffixCount = 0;
	Reader << AffixCount;
	if (Reader.IsError() || AffixCount < 0 ||
		AffixCount > RpgInventoryFragmentItemization::MaximumSavedRolls)
	{
		return false;
	}
	OutState.Affixes.Reserve(AffixCount);
	for (int32 Index = 0; Index < AffixCount; ++Index)
	{
		FRpgRolledItemAffix& Affix = OutState.Affixes.AddDefaulted_GetRef();
		Reader << Affix.AffixId;
		if (!RpgInventoryFragmentItemization::ReadTag(Reader, Affix.StatTag))
		{
			return false;
		}
		Reader << Affix.Value;
	}

	return !Reader.IsError() && Reader.Tell() == PayloadCopy.Num() &&
		IsPersistedItemizationStateCompatible(OutState);
}

bool URpgInventoryFragment_Itemization::ValidateRuntimeState(
	const URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	FRpgItemizationState StagedState;
	return Instance && DeserializeState(Payload, StagedState);
}

bool URpgInventoryFragment_Itemization::ImportRuntimeState(
	URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	FRpgItemizationState StagedState;
	return Instance && DeserializeState(Payload, StagedState) &&
		Instance->RestorePersistedItemizationState(StagedState);
}

void URpgInventoryFragment_Itemization::CopyRuntimeState(
	const URpgInventoryItemInstance* Source,
	URpgInventoryItemInstance* Target) const
{
	if (Source && Target)
	{
		const FRpgItemizationState& State = Source->GetItemizationStateRef();
		if (IsPersistedItemizationStateCompatible(State))
		{
			Target->RestorePersistedItemizationState(State);
		}
	}
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_Itemization::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	FString Error;
	if (!ItemizationProfile || !ItemizationProfile->HasValidConfiguration(&Error))
	{
		Context.AddError(FText::FromString(
			ItemizationProfile
				? FString::Printf(TEXT("ItemizationProfile is invalid: %s"), *Error)
				: TEXT("ItemizationProfile is required.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
