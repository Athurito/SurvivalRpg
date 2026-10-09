#include "RpgInventoryItemizationFragmentViewModel.h"

#include "SurvivalRpg/Inventory/Itemization/RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryItemizationFragmentViewModel)

namespace
{
	FText MakeRarityLabel(const ERpgItemRarity Rarity)
	{
		switch (Rarity)
		{
		case ERpgItemRarity::Uncommon:
			return NSLOCTEXT("RpgItemization", "RarityUncommon", "Uncommon");
		case ERpgItemRarity::Rare:
			return NSLOCTEXT("RpgItemization", "RarityRare", "Rare");
		case ERpgItemRarity::Epic:
			return NSLOCTEXT("RpgItemization", "RarityEpic", "Epic");
		default:
			return NSLOCTEXT("RpgItemization", "RarityCommon", "Common");
		}
	}

	FLinearColor MakeRarityColor(const ERpgItemRarity Rarity)
	{
		switch (Rarity)
		{
		case ERpgItemRarity::Uncommon:
			return FLinearColor(0.20f, 0.80f, 0.25f);
		case ERpgItemRarity::Rare:
			return FLinearColor(0.20f, 0.45f, 1.00f);
		case ERpgItemRarity::Epic:
			return FLinearColor(0.65f, 0.25f, 0.95f);
		default:
			return FLinearColor::White;
		}
	}

	FText NormalizeAffixLabel(const FText& Label)
	{
		FString LabelString = Label.ToString();
		LabelString.TrimStartAndEndInline();
		if (!LabelString.RemoveFromStart(TEXT("+")))
		{
			return Label;
		}

		LabelString.TrimStartInline();
		return FText::FromString(LabelString);
	}
}

void URpgInventoryItemizationFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	if (ItemInstance)
	{
		ItemInstance->OnItemizationStateChanged.RemoveDynamic(this, &ThisClass::HandleItemizationStateChanged);
	}

	Super::InitializeFromEntry(Entry);
	if (ItemInstance)
	{
		ItemInstance->OnItemizationStateChanged.AddUniqueDynamic(this, &ThisClass::HandleItemizationStateChanged);
		ApplyState(ItemInstance->GetItemizationStateRef());
	}
	else
	{
		ApplyState(FRpgItemizationState());
	}
}

void URpgInventoryItemizationFragmentViewModel::BeginDestroy()
{
	if (ItemInstance)
	{
		ItemInstance->OnItemizationStateChanged.RemoveDynamic(this, &ThisClass::HandleItemizationStateChanged);
	}

	Super::BeginDestroy();
}

void URpgInventoryItemizationFragmentViewModel::HandleItemizationStateChanged(const FRpgItemizationState& NewState)
{
	ApplyState(NewState);
}

void URpgInventoryItemizationFragmentViewModel::ApplyState(const FRpgItemizationState& NewState)
{
	const bool bNewGenerated = NewState.bGenerated;
	const int32 NewItemLevel = NewState.ItemLevel;
	const ERpgItemRarity NewRarity = NewState.Rarity;
	const FText NewRarityLabel = MakeRarityLabel(NewRarity);
	const FLinearColor NewRarityColor = MakeRarityColor(NewRarity);
	TArray<FRpgItemizationDisplayRow> NewRows;
	NewRows.Reserve(NewState.BaseStats.Num() + NewState.Affixes.Num());
	for (const FRpgRolledItemStat& Stat : NewState.BaseStats)
	{
		FRpgItemizationDisplayRow& Row = NewRows.AddDefaulted_GetRef();
		Row.Label = GetRpgItemStatDisplayName(Stat.StatTag);
		Row.Value = Stat.Value;
		Row.StatTag = Stat.StatTag;
	}

	const URpgInventoryFragment_Itemization* Fragment = ItemInstance
		? ItemInstance->FindFragmentByClass<URpgInventoryFragment_Itemization>()
		: nullptr;
	const URpgItemAffixPool* AffixPool = Fragment && Fragment->ItemizationProfile
		? Fragment->ItemizationProfile->AffixPool
		: nullptr;
	for (const FRpgRolledItemAffix& Affix : NewState.Affixes)
	{
		FRpgItemizationDisplayRow& Row = NewRows.AddDefaulted_GetRef();
		Row.Label = GetRpgItemStatDisplayName(Affix.StatTag);
		if (AffixPool)
		{
			if (const FRpgItemAffixDefinition* Definition = AffixPool->Affixes.FindByPredicate(
				[&Affix](const FRpgItemAffixDefinition& Candidate) { return Candidate.AffixId == Affix.AffixId; }))
			{
				Row.Label = Definition->DisplayName.IsEmpty() ? Row.Label : Definition->DisplayName;
			}
		}
		// The tooltip owns the numeric '+' prefix. Normalize legacy authored labels
		// such as "+ Strength" so migrated assets do not render "+2 + Strength".
		Row.Label = NormalizeAffixLabel(Row.Label);
		Row.Value = Affix.Value;
		Row.StatTag = Affix.StatTag;
		Row.bAffix = true;
	}

	const bool bGeneratedChanged = bGenerated != bNewGenerated;
	const bool bItemLevelChanged = ItemLevel != NewItemLevel;
	const bool bRarityChanged = Rarity != NewRarity;
	const bool bRarityLabelChanged = !RarityLabel.IdenticalTo(NewRarityLabel);
	const bool bRarityColorChanged = RarityColor != NewRarityColor;
	bGenerated = bNewGenerated;
	ItemLevel = NewItemLevel;
	Rarity = NewRarity;
	RarityLabel = NewRarityLabel;
	RarityColor = NewRarityColor;
	StatRows = MoveTemp(NewRows);

	if (bGeneratedChanged) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bGenerated); }
	if (bItemLevelChanged) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemLevel); }
	if (bRarityChanged) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Rarity); }
	if (bRarityLabelChanged) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RarityLabel); }
	if (bRarityColorChanged) { UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RarityColor); }
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StatRows);
	OnPresentationChanged.Broadcast(this);
}
