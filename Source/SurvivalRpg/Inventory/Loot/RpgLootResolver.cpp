#include "RpgLootResolver.h"

#include "SurvivalRpg/Inventory/Itemization/RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationGenerator.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

namespace RpgLootResolver
{
	bool RollChance(float ChancePercent, FRandomStream& RandomStream)
	{
		return RandomStream.FRandRange(0.0f, 100.0f) <
			FMath::Clamp(ChancePercent, 0.0f, 100.0f);
	}

	int32 RollScaledQuantity(
		const FRpgLootEntry& Entry,
		const FRpgLootRollContext& Context,
		FRandomStream& RandomStream)
	{
		const int32 BaseQuantity = RandomStream.RandRange(
			Entry.MinimumQuantity,
			Entry.MaximumQuantity);
		if (!Entry.bScaleQuantityWithYield)
		{
			return BaseQuantity;
		}

		const double ScaledQuantity = FMath::Clamp(
			static_cast<double>(BaseQuantity) *
				static_cast<double>(Context.YieldMultiplier) *
				static_cast<double>(Context.HarvestPower),
			0.0,
			static_cast<double>(RpgLootMaximumQuantityPerRoll));
		int32 WholeQuantity = FMath::FloorToInt(ScaledQuantity);
		const double Fraction = ScaledQuantity - WholeQuantity;
		if (Fraction > 0.0 && RandomStream.FRand() < Fraction)
		{
			++WholeQuantity;
		}
		return FMath::Min(WholeQuantity, RpgLootMaximumQuantityPerRoll);
	}

	void AppendSuccessfulEntry(
		const FRpgLootEntry& Entry,
		const FRpgLootRollContext& Context,
		FRandomStream& RandomStream,
		FRpgLootRollResult& Result)
	{
		const int32 Quantity = RollScaledQuantity(Entry, Context, RandomStream);
		if (Quantity <= 0)
		{
			return;
		}

		FRpgLootItemRoll& Item = Result.Items.AddDefaulted_GetRef();
		Item.ItemDefinition = Entry.ItemDefinition;
		Item.Quantity = Quantity;
		Item.SourceLevel = Context.SourceLevel;
		Item.ItemizationSeed = static_cast<int32>(RandomStream.GetUnsignedInt());
	}

	int32 SelectWeightedEntry(
		const FRpgLootGroup& Group,
		const TArray<int32>& CandidateIndices,
		FRandomStream& RandomStream)
	{
		float TotalWeight = 0.0f;
		for (const int32 EntryIndex : CandidateIndices)
		{
			TotalWeight += Group.Entries[EntryIndex].Weight;
		}
		if (!FMath::IsFinite(TotalWeight) || TotalWeight <= 0.0f)
		{
			return INDEX_NONE;
		}

		const float Selection = RandomStream.FRandRange(0.0f, TotalWeight);
		float RunningWeight = 0.0f;
		for (const int32 EntryIndex : CandidateIndices)
		{
			RunningWeight += Group.Entries[EntryIndex].Weight;
			if (Selection < RunningWeight)
			{
				return EntryIndex;
			}
		}
		return CandidateIndices.IsEmpty() ? INDEX_NONE : CandidateIndices.Last();
	}
}

bool FRpgLootRollContext::IsValid() const
{
	return SourceLevel > 0 && SkillLevel >= 0 &&
		FMath::IsFinite(HarvestPower) && HarvestPower >= 0.0f &&
		FMath::IsFinite(YieldMultiplier) && YieldMultiplier >= 0.0f &&
		FMath::IsFinite(RareFindMultiplier) && RareFindMultiplier >= 0.0f;
}

bool FRpgLootRollResult::ToInventoryPickup(
	UObject* InstanceOuter,
	FInventoryPickup& OutPickup) const
{
	return FRpgLootResolver::MaterializeLoot(InstanceOuter, *this, OutPickup);
}

bool FRpgLootResolver::RollLoot(
	const URpgLootTable* LootTable,
	const FRpgLootRollContext& Context,
	FRpgLootRollResult& OutResult)
{
	OutResult = FRpgLootRollResult();
	if (!LootTable || !LootTable->HasValidConfiguration() || !Context.IsValid())
	{
		return false;
	}

	FRpgLootRollResult Result;
	Result.Seed = Context.Seed;
	FRandomStream RandomStream(Context.Seed);
	for (const FRpgLootGroup& Group : LootTable->Groups)
	{
		if (!RpgLootResolver::RollChance(Group.GroupChancePercent, RandomStream))
		{
			continue;
		}

		if (Group.Mode == ERpgLootGroupMode::Independent)
		{
			for (const FRpgLootEntry& Entry : Group.Entries)
			{
				const float EffectiveChance = Entry.bScaleChanceWithRareFind
					? Entry.ChancePercent * Context.RareFindMultiplier
					: Entry.ChancePercent;
				if (RpgLootResolver::RollChance(EffectiveChance, RandomStream))
				{
					RpgLootResolver::AppendSuccessfulEntry(
						Entry,
						Context,
						RandomStream,
						Result);
				}
			}
			continue;
		}

		TArray<int32> CandidateIndices;
		CandidateIndices.Reserve(Group.Entries.Num());
		for (int32 EntryIndex = 0; EntryIndex < Group.Entries.Num(); ++EntryIndex)
		{
			CandidateIndices.Add(EntryIndex);
		}
		for (int32 RollIndex = 0; RollIndex < Group.WeightedRollCount; ++RollIndex)
		{
			const int32 SelectedEntry = RpgLootResolver::SelectWeightedEntry(
				Group,
				CandidateIndices,
				RandomStream);
			const int32 CandidatePosition = CandidateIndices.Find(SelectedEntry);
			if (!Group.Entries.IsValidIndex(SelectedEntry) || CandidatePosition == INDEX_NONE)
			{
				return false;
			}
			CandidateIndices.RemoveAt(CandidatePosition, 1, EAllowShrinking::No);
			RpgLootResolver::AppendSuccessfulEntry(
				Group.Entries[SelectedEntry],
				Context,
				RandomStream,
				Result);
		}
	}

	OutResult = MoveTemp(Result);
	return true;
}

bool FRpgLootResolver::MaterializeLoot(
	UObject* InstanceOuter,
	const FRpgLootRollResult& RollResult,
	FInventoryPickup& OutPickup)
{
	OutPickup = FInventoryPickup();
	if (!InstanceOuter)
	{
		return false;
	}

	FInventoryPickup StagedPickup;
	for (const FRpgLootItemRoll& Roll : RollResult.Items)
	{
		if (!Roll.ItemDefinition || Roll.Quantity <= 0 || Roll.SourceLevel <= 0 ||
			Roll.Quantity > RpgLootMaximumQuantityPerRoll)
		{
			return false;
		}

		const URpgInventoryItemDefinition* DefinitionCDO =
			GetDefault<URpgInventoryItemDefinition>(Roll.ItemDefinition);
		if (!DefinitionCDO)
		{
			return false;
		}

		const URpgInventoryFragment_Itemization* ItemizationFragment =
			Cast<URpgInventoryFragment_Itemization>(DefinitionCDO->FindFragmentByClass(
				URpgInventoryFragment_Itemization::StaticClass()));
		if (!ItemizationFragment)
		{
			FPickupTemplate* ExistingTemplate = StagedPickup.Templates.FindByPredicate(
				[&Roll](const FPickupTemplate& Candidate)
				{
					return Candidate.ItemDef == Roll.ItemDefinition;
				});
			if (ExistingTemplate)
			{
				if (ExistingTemplate->StackCount > MAX_int32 - Roll.Quantity)
				{
					return false;
				}
				ExistingTemplate->StackCount += Roll.Quantity;
			}
			else
			{
				FPickupTemplate& Template = StagedPickup.Templates.AddDefaulted_GetRef();
				Template.ItemDef = Roll.ItemDefinition;
				Template.StackCount = Roll.Quantity;
			}
			continue;
		}

		if (!ItemizationFragment->ItemizationProfile ||
			!ItemizationFragment->ItemizationProfile->HasValidConfiguration())
		{
			return false;
		}

		FRandomStream ItemizationStream(Roll.ItemizationSeed);
		for (int32 ItemIndex = 0; ItemIndex < Roll.Quantity; ++ItemIndex)
		{
			FRpgItemizationState GeneratedState;
			if (!FRpgItemizationGenerator::GenerateItemization(
				ItemizationFragment->ItemizationProfile,
				Roll.SourceLevel,
				ItemizationStream,
				GeneratedState))
			{
				return false;
			}

			URpgInventoryItemInstance* Instance =
				NewObject<URpgInventoryItemInstance>(InstanceOuter);
			if (!Instance)
			{
				return false;
			}
			Instance->SetItemDef(Roll.ItemDefinition);
			for (const URpgInventoryItemFragment* Fragment : DefinitionCDO->Fragments)
			{
				if (Fragment)
				{
					Fragment->OnInstanceCreated(Instance);
				}
			}
			if (!Instance->GetItemId().IsValid() ||
				!Instance->ApplyItemizationState(GeneratedState))
			{
				return false;
			}

			FPickupInstance& PickupInstance =
				StagedPickup.Instances.AddDefaulted_GetRef();
			PickupInstance.Item = Instance;
		}
	}

	OutPickup = MoveTemp(StagedPickup);
	return true;
}

bool FRpgLootResolver::RollAndMaterialize(
	const URpgLootTable* LootTable,
	const FRpgLootRollContext& Context,
	UObject* InstanceOuter,
	FInventoryPickup& OutPickup,
	FRpgLootRollResult* OutRollResult)
{
	FRpgLootRollResult RollResult;
	if (!RollLoot(LootTable, Context, RollResult) ||
		!MaterializeLoot(InstanceOuter, RollResult, OutPickup))
	{
		OutPickup = FInventoryPickup();
		if (OutRollResult)
		{
			*OutRollResult = FRpgLootRollResult();
		}
		return false;
	}

	if (OutRollResult)
	{
		*OutRollResult = MoveTemp(RollResult);
	}
	return true;
}
