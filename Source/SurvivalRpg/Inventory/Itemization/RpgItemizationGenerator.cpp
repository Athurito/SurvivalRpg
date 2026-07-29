#include "RpgItemizationGenerator.h"

#include "RpgItemizationProfile.h"

namespace RpgItemizationGenerator
{
	bool RollRange(
		const FScalableFloat& Minimum,
		const FScalableFloat& Maximum,
		int32 ItemLevel,
		FRandomStream& RandomStream,
		float& OutValue)
	{
		const float MinValue = Minimum.GetValueAtLevel(ItemLevel);
		const float MaxValue = Maximum.GetValueAtLevel(ItemLevel);
		if (!FMath::IsFinite(MinValue) || !FMath::IsFinite(MaxValue) || MinValue > MaxValue)
		{
			return false;
		}
		OutValue = MinValue == MaxValue
			? MinValue
			: RandomStream.FRandRange(MinValue, MaxValue);
		return FMath::IsFinite(OutValue);
	}

	bool RollRarity(
		const TArray<FRpgItemRarityWeight>& Weights,
		FRandomStream& RandomStream,
		ERpgItemRarity& OutRarity)
	{
		float TotalWeight = 0.0f;
		for (const FRpgItemRarityWeight& Row : Weights)
		{
			TotalWeight += Row.Weight;
		}
		if (!FMath::IsFinite(TotalWeight) || TotalWeight <= 0.0f)
		{
			return false;
		}

		const float Selection = RandomStream.FRandRange(0.0f, TotalWeight);
		float RunningWeight = 0.0f;
		for (const FRpgItemRarityWeight& Row : Weights)
		{
			RunningWeight += Row.Weight;
			if (Row.Weight > 0.0f && Selection < RunningWeight)
			{
				OutRarity = Row.Rarity;
				return true;
			}
		}

		for (int32 Index = Weights.Num() - 1; Index >= 0; --Index)
		{
			if (Weights[Index].Weight > 0.0f)
			{
				OutRarity = Weights[Index].Rarity;
				return true;
			}
		}
		return false;
	}

	int32 SelectWeightedAffixIndex(
		const TArray<const FRpgItemAffixDefinition*>& Candidates,
		FRandomStream& RandomStream)
	{
		float TotalWeight = 0.0f;
		for (const FRpgItemAffixDefinition* Candidate : Candidates)
		{
			TotalWeight += Candidate ? Candidate->Weight : 0.0f;
		}
		if (TotalWeight <= 0.0f || !FMath::IsFinite(TotalWeight))
		{
			return INDEX_NONE;
		}

		const float Selection = RandomStream.FRandRange(0.0f, TotalWeight);
		float RunningWeight = 0.0f;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FRpgItemAffixDefinition* Candidate = Candidates[Index];
			RunningWeight += Candidate ? Candidate->Weight : 0.0f;
			if (Candidate && Selection < RunningWeight)
			{
				return Index;
			}
		}
		return Candidates.IsEmpty() ? INDEX_NONE : Candidates.Num() - 1;
	}
}

bool FRpgItemizationGenerator::GenerateItemization(
	const URpgItemizationProfile* Profile,
	int32 SourceLevel,
	FRandomStream& RandomStream,
	FRpgItemizationState& OutState)
{
	OutState = FRpgItemizationState();
	if (!Profile || !Profile->HasValidConfiguration())
	{
		return false;
	}

	FRpgItemizationState GeneratedState;
	GeneratedState.bGenerated = true;
	GeneratedState.ItemLevel = Profile->ResolveItemLevel(SourceLevel);
	if (!RpgItemizationGenerator::RollRarity(
		Profile->RarityWeights,
		RandomStream,
		GeneratedState.Rarity))
	{
		return false;
	}

	GeneratedState.BaseStats.Reserve(Profile->BaseStats.Num());
	for (const FRpgItemStatRollDefinition& Definition : Profile->BaseStats)
	{
		FRpgRolledItemStat& Roll = GeneratedState.BaseStats.AddDefaulted_GetRef();
		Roll.StatTag = Definition.StatTag;
		if (!RpgItemizationGenerator::RollRange(
			Definition.MinimumValue,
			Definition.MaximumValue,
			GeneratedState.ItemLevel,
			RandomStream,
			Roll.Value))
		{
			return false;
		}
	}

	const int32 AffixCount = GetRpgAffixCountForRarity(GeneratedState.Rarity);
	if (AffixCount > 0)
	{
		TArray<const FRpgItemAffixDefinition*> Candidates;
		Profile->AffixPool->GetEligibleAffixes(Profile->ItemTags, Candidates);
		if (Candidates.Num() < AffixCount)
		{
			return false;
		}

		GeneratedState.Affixes.Reserve(AffixCount);
		for (int32 RollIndex = 0; RollIndex < AffixCount; ++RollIndex)
		{
			const int32 SelectedIndex =
				RpgItemizationGenerator::SelectWeightedAffixIndex(Candidates, RandomStream);
			if (!Candidates.IsValidIndex(SelectedIndex) || !Candidates[SelectedIndex])
			{
				return false;
			}

			const FRpgItemAffixDefinition* Definition = Candidates[SelectedIndex];
			Candidates.RemoveAtSwap(SelectedIndex, 1, EAllowShrinking::No);
			FRpgRolledItemAffix& Roll = GeneratedState.Affixes.AddDefaulted_GetRef();
			Roll.AffixId = Definition->AffixId;
			Roll.StatTag = Definition->StatTag;
			if (!RpgItemizationGenerator::RollRange(
				Definition->MinimumValue,
				Definition->MaximumValue,
				GeneratedState.ItemLevel,
				RandomStream,
				Roll.Value))
			{
				return false;
			}
		}
	}

	if (!GeneratedState.IsStructurallyValid())
	{
		return false;
	}

	OutState = MoveTemp(GeneratedState);
	return true;
}
