#include "RpgItemizationProfile.h"

#include "RpgItemizationGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgItemizationProfile)

namespace RpgItemizationProfile
{
	bool ValidateRange(
		const FScalableFloat& Minimum,
		const FScalableFloat& Maximum,
		float Level)
	{
		const float MinValue = Minimum.GetValueAtLevel(Level);
		const float MaxValue = Maximum.GetValueAtLevel(Level);
		return FMath::IsFinite(MinValue) && FMath::IsFinite(MaxValue) &&
			MinValue <= MaxValue;
	}

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}
}

bool FRpgItemAffixDefinition::IsEligibleFor(
	const FGameplayTagContainer& ItemTags) const
{
	return ItemTags.HasAll(RequiredItemTags) &&
		!ItemTags.HasAny(BlockedItemTags);
}

void URpgItemAffixPool::GetEligibleAffixes(
	const FGameplayTagContainer& ItemTags,
	TArray<const FRpgItemAffixDefinition*>& OutAffixes) const
{
	OutAffixes.Reset();
	OutAffixes.Reserve(Affixes.Num());
	for (const FRpgItemAffixDefinition& Affix : Affixes)
	{
		if (Affix.IsEligibleFor(ItemTags))
		{
			OutAffixes.Add(&Affix);
		}
	}
}

bool URpgItemAffixPool::HasValidConfiguration(FString* OutError) const
{
	if (Affixes.IsEmpty())
	{
		RpgItemizationProfile::SetError(OutError, TEXT("Affixes must contain at least one entry."));
		return false;
	}

	TSet<FName> SeenIds;
	for (int32 Index = 0; Index < Affixes.Num(); ++Index)
	{
		const FRpgItemAffixDefinition& Affix = Affixes[Index];
		if (Affix.AffixId.IsNone() || SeenIds.Contains(Affix.AffixId))
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(TEXT("Affixes[%d] has a missing or duplicate AffixId."), Index));
			return false;
		}
		SeenIds.Add(Affix.AffixId);

		if (!RpgItemizationGameplayTags::IsSupportedItemStat(Affix.StatTag) ||
			!FMath::IsFinite(Affix.Weight) || Affix.Weight <= 0.0f)
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(TEXT("Affixes[%d] requires a supported Item.Stat tag and positive finite Weight."), Index));
			return false;
		}
		for (int32 ItemLevel = 1; ItemLevel <= 100; ++ItemLevel)
		{
			if (!RpgItemizationProfile::ValidateRange(
				Affix.MinimumValue,
				Affix.MaximumValue,
				static_cast<float>(ItemLevel)))
			{
				RpgItemizationProfile::SetError(
					OutError,
					FString::Printf(
						TEXT("Affixes[%d] has a non-finite or inverted value range at item level %d."),
						Index,
						ItemLevel));
				return false;
			}
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgItemAffixPool::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	FString Error;
	if (!HasValidConfiguration(&Error))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

URpgItemizationProfile::URpgItemizationProfile(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	auto AddRarityWeight = [this](ERpgItemRarity Rarity, float Weight)
	{
		FRpgItemRarityWeight& Row = RarityWeights.AddDefaulted_GetRef();
		Row.Rarity = Rarity;
		Row.Weight = Weight;
	};
	AddRarityWeight(ERpgItemRarity::Common, 60.0f);
	AddRarityWeight(ERpgItemRarity::Uncommon, 28.0f);
	AddRarityWeight(ERpgItemRarity::Rare, 10.0f);
	AddRarityWeight(ERpgItemRarity::Epic, 2.0f);
}

int32 URpgItemizationProfile::ResolveItemLevel(int32 SourceLevel) const
{
	return FMath::Clamp(
		SourceLevel + ItemLevelOffset,
		MinimumItemLevel,
		MaximumItemLevel);
}

bool URpgItemizationProfile::HasValidConfiguration(FString* OutError) const
{
	if (MinimumItemLevel <= 0 || MaximumItemLevel < MinimumItemLevel || MaximumItemLevel > 100)
	{
		RpgItemizationProfile::SetError(
			OutError,
			TEXT("Item-level bounds must be within 1..100 and MaximumItemLevel must not be below MinimumItemLevel."));
		return false;
	}

	TSet<FGameplayTag> SeenBaseStats;
	for (int32 Index = 0; Index < BaseStats.Num(); ++Index)
	{
		const FRpgItemStatRollDefinition& Stat = BaseStats[Index];
		if (!RpgItemizationGameplayTags::IsSupportedItemStat(Stat.StatTag) ||
			SeenBaseStats.Contains(Stat.StatTag))
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(TEXT("BaseStats[%d] has an unsupported or duplicate Item.Stat tag."), Index));
			return false;
		}
		SeenBaseStats.Add(Stat.StatTag);
		for (int32 ItemLevel = MinimumItemLevel; ItemLevel <= MaximumItemLevel; ++ItemLevel)
		{
			if (!RpgItemizationProfile::ValidateRange(
				Stat.MinimumValue,
				Stat.MaximumValue,
				static_cast<float>(ItemLevel)))
			{
				RpgItemizationProfile::SetError(
					OutError,
					FString::Printf(
						TEXT("BaseStats[%d] has a non-finite or inverted value range at item level %d."),
						Index,
						ItemLevel));
				return false;
			}
		}
	}

	if (RarityWeights.Num() != 4)
	{
		RpgItemizationProfile::SetError(OutError, TEXT("RarityWeights must contain exactly Common, Uncommon, Rare, and Epic."));
		return false;
	}

	TSet<ERpgItemRarity> SeenRarities;
	float TotalWeight = 0.0f;
	int32 MaximumRequiredAffixes = 0;
	for (int32 Index = 0; Index < RarityWeights.Num(); ++Index)
	{
		const FRpgItemRarityWeight& Row = RarityWeights[Index];
		if (static_cast<uint8>(Row.Rarity) > static_cast<uint8>(ERpgItemRarity::Epic) ||
			SeenRarities.Contains(Row.Rarity) || !FMath::IsFinite(Row.Weight) || Row.Weight < 0.0f)
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(TEXT("RarityWeights[%d] has a duplicate/unknown rarity or invalid weight."), Index));
			return false;
		}
		SeenRarities.Add(Row.Rarity);
		TotalWeight += Row.Weight;
		if (Row.Weight > 0.0f)
		{
			MaximumRequiredAffixes = FMath::Max(
				MaximumRequiredAffixes,
				GetRpgAffixCountForRarity(Row.Rarity));
		}
	}
	if (TotalWeight <= 0.0f)
	{
		RpgItemizationProfile::SetError(OutError, TEXT("At least one rarity must have positive weight."));
		return false;
	}

	if (MaximumRequiredAffixes > 0)
	{
		FString PoolError;
		if (!AffixPool || !AffixPool->HasValidConfiguration(&PoolError))
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(TEXT("AffixPool is required and valid for non-Common rarities: %s"), *PoolError));
			return false;
		}

		TArray<const FRpgItemAffixDefinition*> EligibleAffixes;
		AffixPool->GetEligibleAffixes(ItemTags, EligibleAffixes);
		if (EligibleAffixes.Num() < MaximumRequiredAffixes)
		{
			RpgItemizationProfile::SetError(
				OutError,
				FString::Printf(
					TEXT("The profile needs %d unique eligible affixes but its tags resolve only %d."),
					MaximumRequiredAffixes,
					EligibleAffixes.Num()));
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgItemizationProfile::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	FString Error;
	if (!HasValidConfiguration(&Error))
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
