#include "RpgInventoryFragment_Rune.h"

#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryFragment_Rune)

namespace RpgInventoryFragmentRunePrivate
{
	const FName RuntimeStatePayloadId(TEXT("Inventory.Rune.Rank"));
	constexpr int32 RuntimeStatePayloadVersion = 1;
}

int32 URpgInventoryFragment_Rune::GetMaxRank() const
{
	return FMath::Clamp(Ranks.Num(), 1, RpgRuneMaxRank);
}

const FRpgRuneRank* URpgInventoryFragment_Rune::FindRank(const int32 Rank) const
{
	return Rank >= 1 && Rank <= GetMaxRank() && Ranks.IsValidIndex(Rank - 1) ? &Ranks[Rank - 1] : nullptr;
}

const URpgInventoryFragment_Rune* URpgInventoryFragment_Rune::FindRuneOfItem(const UObject* ItemInstance)
{
	const URpgInventoryItemInstance* Item = Cast<URpgInventoryItemInstance>(ItemInstance);
	return Item ? Item->FindFragmentByClass<URpgInventoryFragment_Rune>() : nullptr;
}

FGameplayTagContainer URpgInventoryFragment_Rune::GetSocketedRuneTags(const URpgInventoryItemInstance* SourceItem)
{
	FGameplayTagContainer RuneTags;
	if (const URpgInventoryManagerComponent* Inventory = URpgInventoryManagerComponent::FindInventoryOfItem(SourceItem))
	{
		for (const URpgInventoryItemInstance* Socketed : Inventory->GetSocketedItems(SourceItem))
		{
			const URpgInventoryFragment_Rune* Rune = FindRuneOfItem(Socketed);
			if (Rune && Rune->RuneTag.IsValid())
			{
				RuneTags.AddTag(Rune->RuneTag);
			}
		}
	}
	return RuneTags;
}

void URpgInventoryFragment_Rune::AccumulateSocketedTunings(
	const URpgInventoryItemInstance& SourceItem,
	const FGameplayTagContainer& AbilityIds,
	const FGameplayTag TuningTag,
	FRpgAbilityTuningAccumulator& Accumulator)
{
	const URpgInventoryManagerComponent* Inventory = URpgInventoryManagerComponent::FindInventoryOfItem(&SourceItem);
	if (!Inventory)
	{
		return;
	}

	for (const URpgInventoryItemInstance* Socketed : Inventory->GetSocketedItems(&SourceItem))
	{
		const URpgInventoryFragment_Rune* Rune = FindRuneOfItem(Socketed);
		const FRpgRuneRank* Rank = Rune ? Rune->FindRank(Socketed->GetRuneRank()) : nullptr;
		if (!Rank)
		{
			continue;
		}

		for (const FRpgSkillTreeAbilityTuning& Tuning : Rank->AbilityTunings)
		{
			Accumulator.Apply(Tuning, AbilityIds, TuningTag);
		}
	}
}

FName URpgInventoryFragment_Rune::GetRuntimeStateIdentifier() const
{
	return RpgInventoryFragmentRunePrivate::RuntimeStatePayloadId;
}

int32 URpgInventoryFragment_Rune::GetRuntimeStateVersion() const
{
	return RpgInventoryFragmentRunePrivate::RuntimeStatePayloadVersion;
}

bool URpgInventoryFragment_Rune::ExportRuntimeState(
	const URpgInventoryItemInstance* Instance,
	FRpgInventoryFragmentStatePayload& OutPayload) const
{
	if (!Instance)
	{
		return false;
	}

	OutPayload.FragmentId = RpgInventoryFragmentRunePrivate::RuntimeStatePayloadId;
	OutPayload.Version = RpgInventoryFragmentRunePrivate::RuntimeStatePayloadVersion;
	OutPayload.Payload.Reset();
	FMemoryWriter Writer(OutPayload.Payload, true);
	uint8 SerializedRank = static_cast<uint8>(Instance->GetRuneRank());
	Writer << SerializedRank;
	return !Writer.IsError();
}

bool URpgInventoryFragment_Rune::ReadRank(const FRpgInventoryFragmentStatePayload& Payload, uint8& OutRank) const
{
	OutRank = 0;
	if (Payload.FragmentId != RpgInventoryFragmentRunePrivate::RuntimeStatePayloadId ||
		Payload.Version != RpgInventoryFragmentRunePrivate::RuntimeStatePayloadVersion)
	{
		return false;
	}

	TArray<uint8> PayloadCopy = Payload.Payload;
	FMemoryReader Reader(PayloadCopy, true);
	Reader << OutRank;
	return !Reader.IsError() && Reader.Tell() == PayloadCopy.Num() && OutRank >= 1 && OutRank <= GetMaxRank();
}

bool URpgInventoryFragment_Rune::ValidateRuntimeState(
	const URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	uint8 Rank = 0;
	return Instance && ReadRank(Payload, Rank);
}

bool URpgInventoryFragment_Rune::ImportRuntimeState(
	URpgInventoryItemInstance* Instance,
	const FRpgInventoryFragmentStatePayload& Payload) const
{
	uint8 Rank = 0;
	return Instance && ReadRank(Payload, Rank) && Instance->SetRuneRank(Rank);
}

void URpgInventoryFragment_Rune::CopyRuntimeState(
	const URpgInventoryItemInstance* Source,
	URpgInventoryItemInstance* Target) const
{
	if (Source && Target)
	{
		Target->SetRuneRank(Source->GetRuneRank());
	}
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryFragment_Rune::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	const auto AddError = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (!RuneTag.IsValid())
	{
		AddError(NSLOCTEXT("RpgInventoryFragmentRune", "MissingRuneTag", "A rune needs a RuneTag."));
	}
	if (Ranks.IsEmpty() || Ranks.Num() > RpgRuneMaxRank)
	{
		AddError(FText::Format(
			NSLOCTEXT("RpgInventoryFragmentRune", "InvalidRankCount", "A rune needs one to {0} ranks."),
			RpgRuneMaxRank));
	}

	if (const URpgInventoryItemDefinition* ItemDefinition = GetTypedOuter<URpgInventoryItemDefinition>())
	{
		const URpgInventoryFragment_ItemTraits* Traits = Cast<URpgInventoryFragment_ItemTraits>(
			ItemDefinition->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass()));
		if (!Traits || Traits->bCanStack || Traits->ItemCategory != ERpgInventoryItemCategory::Rune)
		{
			AddError(NSLOCTEXT(
				"RpgInventoryFragmentRune",
				"InvalidTraits",
				"A rune needs Item Traits with the category Rune and stacking off, because every rune keeps its own rank."));
		}
		if (ItemDefinition->FindFragmentByClass(URpgInventoryFragment_ItemContainer::StaticClass()))
		{
			AddError(NSLOCTEXT("RpgInventoryFragmentRune", "RuneOwnsContainer", "A rune cannot provide containers."));
		}
	}
	return Result;
}
#endif
