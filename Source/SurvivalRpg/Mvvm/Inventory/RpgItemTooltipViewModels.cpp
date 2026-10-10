#include "RpgItemTooltipViewModels.h"

#include "Engine/Texture2D.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryItemizationFragmentViewModel.h"
#include "Templates/Identity.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgItemTooltipViewModels)

#define LOCTEXT_NAMESPACE "RpgItemTooltip"

namespace RpgItemTooltipViewModels
{
	using FFieldChanges = TArray<UE::FieldNotification::FFieldId, TInlineAllocator<24>>;

	constexpr ETextIdenticalModeFlags TextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	/** Differences below this are shown as equal, so rounding noise never reads as better or worse. */
	constexpr float ComparisonTolerance = 0.005f;

	template <typename T>
	bool AssignIfChanged(T& Field, const TIdentity_T<T>& NewValue)
	{
		if (Field == NewValue)
		{
			return false;
		}
		Field = NewValue;
		return true;
	}

	bool AssignIfChanged(FText& Field, const FText& NewValue)
	{
		if (Field.IdenticalTo(NewValue, TextIdentityFlags))
		{
			return false;
		}
		Field = NewValue;
		return true;
	}

	void BroadcastChanges(UMVVMViewModelBase& ViewModel, const FFieldChanges& Changes)
	{
		for (const UE::FieldNotification::FFieldId& FieldId : Changes)
		{
			ViewModel.BroadcastFieldValueChanged(FieldId);
		}
	}
}

#define RPG_TOOLTIP_SET(Changes, Member, NewValue) \
	if (RpgItemTooltipViewModels::AssignIfChanged(Member, NewValue)) { Changes.Add(ThisClass::FFieldNotificationClassDescriptor::Member); }

void URpgItemStatRowViewModel::InitializeRow(const FText& InLabel, float InValue, FGameplayTag InStatTag, bool bInAffix)
{
	Value = InValue;
	StatTag = InStatTag;

	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, Label, InLabel);
	RPG_TOOLTIP_SET(Changes, ValueText, URpgItemTooltipViewModel::FormatStatValue(InValue, bInAffix));
	RPG_TOOLTIP_SET(Changes, bAffix, bInAffix);
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
	SetComparison(TOptional<float>());
}

void URpgItemStatRowViewModel::SetComparison(TOptional<float> BaselineValue)
{
	ERpgItemStatComparison NewComparison = ERpgItemStatComparison::None;
	FText NewDeltaText;
	if (BaselineValue.IsSet())
	{
		const float Delta = Value - BaselineValue.GetValue();
		if (FMath::Abs(Delta) < RpgItemTooltipViewModels::ComparisonTolerance)
		{
			NewComparison = ERpgItemStatComparison::Equal;
		}
		else
		{
			NewComparison = Delta > 0.0f ? ERpgItemStatComparison::Better : ERpgItemStatComparison::Worse;
			NewDeltaText = URpgItemTooltipViewModel::FormatStatValue(Delta, true);
		}
	}

	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, Comparison, NewComparison);
	RPG_TOOLTIP_SET(Changes, DeltaText, NewDeltaText);
	RPG_TOOLTIP_SET(Changes, bHasComparison, NewComparison != ERpgItemStatComparison::None);
	RPG_TOOLTIP_SET(Changes, bIsBetter, NewComparison == ERpgItemStatComparison::Better);
	RPG_TOOLTIP_SET(Changes, bIsWorse, NewComparison == ERpgItemStatComparison::Worse);
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
}

FText URpgItemTooltipViewModel::FormatStatValue(float Value, bool bWithPlusSign)
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 0;
	Options.MaximumFractionalDigits = 2;
	const FText Number = FText::AsNumber(Value, &Options);
	return bWithPlusSign && Value >= 0.0f
		? FText::Format(LOCTEXT("PositiveStat", "+{0}"), Number)
		: Number;
}

FText URpgItemTooltipViewModel::GetCategoryLabel(ERpgInventoryItemCategory ItemCategory)
{
	switch (ItemCategory)
	{
	case ERpgInventoryItemCategory::Material:
		return LOCTEXT("CategoryMaterial", "Material");
	case ERpgInventoryItemCategory::Weapon:
		return LOCTEXT("CategoryWeapon", "Weapon");
	case ERpgInventoryItemCategory::Shield:
		return LOCTEXT("CategoryShield", "Shield");
	case ERpgInventoryItemCategory::Armor:
		return LOCTEXT("CategoryArmor", "Armour");
	case ERpgInventoryItemCategory::Consumable:
		return LOCTEXT("CategoryConsumable", "Consumable");
	case ERpgInventoryItemCategory::Tool:
		return LOCTEXT("CategoryTool", "Tool");
	case ERpgInventoryItemCategory::Rune:
		return LOCTEXT("CategoryRune", "Rune");
	case ERpgInventoryItemCategory::Quest:
		return LOCTEXT("CategoryQuest", "Quest Item");
	case ERpgInventoryItemCategory::Misc:
		return LOCTEXT("CategoryMisc", "Miscellaneous");
	case ERpgInventoryItemCategory::None:
	default:
		return FText::GetEmpty();
	}
}

void URpgItemTooltipViewModel::SetEntry(const URpgInventoryEntryViewModel* Entry)
{
	const bool bNewHasItem = Entry && Entry->GetItemInstance() && !Entry->IsEmptySlot();
	const URpgInventoryItemizationFragmentViewModel* Itemization = bNewHasItem
		? Entry->GetItemizationViewModel()
		: nullptr;
	const bool bGenerated = Itemization && Itemization->IsGenerated();
	const int32 StackCount = bNewHasItem ? Entry->GetStackCount() : 0;
	const FText NewDescription = bNewHasItem ? Entry->GetDescription() : FText::GetEmpty();

	FText NewSubtitle;
	if (bGenerated)
	{
		NewSubtitle = FText::Format(
			LOCTEXT("ItemLevel", "Item Level {0}"),
			FText::AsNumber(Itemization->GetItemLevel()));
	}
	else if (bNewHasItem)
	{
		NewSubtitle = GetCategoryLabel(Entry->GetItemCategory());
	}

	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, bHasItem, bNewHasItem);
	RPG_TOOLTIP_SET(Changes, DisplayName, bNewHasItem ? Entry->GetDisplayName() : FText::GetEmpty());
	RPG_TOOLTIP_SET(Changes, Icon, bNewHasItem ? Entry->GetIcon() : TSoftObjectPtr<UTexture2D>());
	RPG_TOOLTIP_SET(Changes, bHasRarity, bGenerated);
	RPG_TOOLTIP_SET(Changes, RarityLabel, bGenerated ? Itemization->GetRarityLabel() : FText::GetEmpty());
	RPG_TOOLTIP_SET(Changes, RarityColor, bGenerated ? Itemization->GetRarityColor() : FLinearColor::White);
	RPG_TOOLTIP_SET(Changes, SubtitleText, NewSubtitle);
	RPG_TOOLTIP_SET(Changes, QuantityText, StackCount > 1
		? FText::Format(LOCTEXT("Quantity", "{0} pieces"), FText::AsNumber(StackCount))
		: FText::GetEmpty());
	RPG_TOOLTIP_SET(Changes, bHasQuantity, StackCount > 1);
	RPG_TOOLTIP_SET(Changes, Description, NewDescription);
	RPG_TOOLTIP_SET(Changes, bHasDescription, !NewDescription.IsEmpty());
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);

	int32 BaseCount = 0;
	int32 AffixCount = 0;
	if (bGenerated)
	{
		for (const FRpgItemizationDisplayRow& Row : Itemization->GetStatRows())
		{
			++(Row.bAffix ? AffixCount : BaseCount);
		}
	}

	TArray<TObjectPtr<URpgItemStatRowViewModel>> NewBaseStats = AcquireRows(BaseStats, BaseCount);
	TArray<TObjectPtr<URpgItemStatRowViewModel>> NewAffixes = AcquireRows(Affixes, AffixCount);
	if (bGenerated)
	{
		int32 BaseIndex = 0;
		int32 AffixIndex = 0;
		for (const FRpgItemizationDisplayRow& Row : Itemization->GetStatRows())
		{
			URpgItemStatRowViewModel* RowViewModel = Row.bAffix
				? NewAffixes[AffixIndex++].Get()
				: NewBaseStats[BaseIndex++].Get();
			RowViewModel->InitializeRow(Row.Label, Row.Value, Row.StatTag, Row.bAffix);
		}
	}
	SetBaseStatsAndAffixes(MoveTemp(NewBaseStats), MoveTemp(NewAffixes));
}

void URpgItemTooltipViewModel::ApplyComparisonBaseline(const URpgItemTooltipViewModel* Baseline)
{
	TMap<FGameplayTag, float> BaselineValues;
	if (Baseline && Baseline->HasItem())
	{
		for (const TArray<TObjectPtr<URpgItemStatRowViewModel>>* Rows : {&Baseline->BaseStats, &Baseline->Affixes})
		{
			for (const URpgItemStatRowViewModel* Row : *Rows)
			{
				if (Row && Row->GetStatTag().IsValid())
				{
					BaselineValues.FindOrAdd(Row->GetStatTag()) += Row->GetValue();
				}
			}
		}
	}

	const bool bCompare = Baseline && Baseline->HasItem();
	for (TArray<TObjectPtr<URpgItemStatRowViewModel>>* Rows : {&BaseStats, &Affixes})
	{
		for (URpgItemStatRowViewModel* Row : *Rows)
		{
			if (!Row)
			{
				continue;
			}
			// A stat the equipped item lacks counts as a gain over zero.
			const float* BaselineValue = BaselineValues.Find(Row->GetStatTag());
			Row->SetComparison(bCompare
				? TOptional<float>(BaselineValue ? *BaselineValue : 0.0f)
				: TOptional<float>());
		}
	}
}

void URpgItemTooltipViewModel::SetComparisonShown(bool bInShown)
{
	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, bComparisonShown, bInShown);
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
}

void URpgItemTooltipViewModel::SetCanCompare(bool bInCanCompare)
{
	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, bCanCompare, bInCanCompare);
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
}

void URpgItemTooltipViewModel::SetIsEquippedComparison(bool bInIsEquipped)
{
	RpgItemTooltipViewModels::FFieldChanges Changes;
	RPG_TOOLTIP_SET(Changes, bIsEquippedComparison, bInIsEquipped);
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
}

void URpgItemTooltipViewModel::SetBaseStatsAndAffixes(
	TArray<TObjectPtr<URpgItemStatRowViewModel>>&& NewBaseStats,
	TArray<TObjectPtr<URpgItemStatRowViewModel>>&& NewAffixes)
{
	RpgItemTooltipViewModels::FFieldChanges Changes;
	if (BaseStats != NewBaseStats)
	{
		BaseStats = MoveTemp(NewBaseStats);
		Changes.Add(FFieldNotificationClassDescriptor::BaseStats);
	}
	if (Affixes != NewAffixes)
	{
		Affixes = MoveTemp(NewAffixes);
		Changes.Add(FFieldNotificationClassDescriptor::Affixes);
	}
	RPG_TOOLTIP_SET(Changes, bHasBaseStats, !BaseStats.IsEmpty());
	RPG_TOOLTIP_SET(Changes, bHasAffixes, !Affixes.IsEmpty());
	RpgItemTooltipViewModels::BroadcastChanges(*this, Changes);
}

TArray<TObjectPtr<URpgItemStatRowViewModel>> URpgItemTooltipViewModel::AcquireRows(
	TArray<TObjectPtr<URpgItemStatRowViewModel>>& Pool,
	int32 Count)
{
	// Reuse row objects so list entries keep their bindings while the same item refreshes.
	TArray<TObjectPtr<URpgItemStatRowViewModel>> Rows;
	Rows.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		URpgItemStatRowViewModel* Row = Pool.IsValidIndex(Index) ? Pool[Index].Get() : nullptr;
		Rows.Add(Row ? Row : NewObject<URpgItemStatRowViewModel>(this));
	}
	return Rows;
}

#undef RPG_TOOLTIP_SET
#undef LOCTEXT_NAMESPACE
