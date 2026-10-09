#include "RpgCraftingCategoryCatalog.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingCategoryCatalog)

namespace RpgCraftingCategoryCatalog
{
	/** Levels of a category below Crafting.Category: 1 for a group, 2 for a subcategory, 0 for the root or foreign tags. */
	int32 GetCategoryDepth(const FGameplayTag& Category)
	{
		const FGameplayTag Root = URpgCraftingCategoryCatalog::GetCategoryRoot();
		if (!Root.IsValid() || !Category.IsValid() || Category == Root || !Category.MatchesTag(Root))
		{
			return 0;
		}
		TArray<FString> RootParts;
		TArray<FString> Parts;
		Root.GetTagName().ToString().ParseIntoArray(RootParts, TEXT("."));
		Category.GetTagName().ToString().ParseIntoArray(Parts, TEXT("."));
		return Parts.Num() - RootParts.Num();
	}
}

const FRpgCraftingCategoryDisplay* URpgCraftingCategoryCatalog::FindCategory(const FGameplayTag& Category) const
{
	return Categories.FindByPredicate([&Category](const FRpgCraftingCategoryDisplay& Row) { return Row.Category == Category; });
}

const FRpgCraftingTierDisplay* URpgCraftingCategoryCatalog::FindTier(int32 Tier) const
{
	return Tiers.FindByPredicate([Tier](const FRpgCraftingTierDisplay& Row) { return Row.Tier == Tier; });
}

FGameplayTag URpgCraftingCategoryCatalog::GetCategoryRoot()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category"), false);
}

FGameplayTag URpgCraftingCategoryCatalog::ResolveCategoryGroup(const FGameplayTag& Category)
{
	FGameplayTag Group = Category;
	while (RpgCraftingCategoryCatalog::GetCategoryDepth(Group) > 1)
	{
		Group = Group.RequestDirectParent();
	}
	return RpgCraftingCategoryCatalog::GetCategoryDepth(Group) == 1 ? Group : FGameplayTag();
}

FText URpgCraftingCategoryCatalog::MakeTierNumeral(int32 Tier)
{
	static const TCHAR* Numerals[] = {
		TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X"),
		TEXT("XI"), TEXT("XII"), TEXT("XIII"), TEXT("XIV"), TEXT("XV"), TEXT("XVI"), TEXT("XVII"), TEXT("XVIII"), TEXT("XIX"), TEXT("XX")};
	return Tier >= 1 && Tier <= UE_ARRAY_COUNT(Numerals)
		? FText::FromString(Numerals[Tier - 1])
		: FText::AsNumber(Tier);
}

FText URpgCraftingCategoryCatalog::MakeFallbackCategoryName(const FGameplayTag& Category)
{
	FString Name = Category.GetTagName().ToString();
	int32 LastDot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), LastDot))
	{
		Name.RightChopInline(LastDot + 1);
	}
	return FText::FromString(Name);
}

#if WITH_EDITOR

EDataValidationResult URpgCraftingCategoryCatalog::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	TSet<FGameplayTag> SeenCategories;
	for (const FRpgCraftingCategoryDisplay& Row : Categories)
	{
		const int32 Depth = RpgCraftingCategoryCatalog::GetCategoryDepth(Row.Category);
		if (Depth < 1 || Depth > 2)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCraftingValidation", "InvalidCatalogCategory", "Category {0} must be a group or subcategory below Crafting.Category."),
				FText::FromName(Row.Category.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		if (SeenCategories.Contains(Row.Category))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCraftingValidation", "DuplicateCatalogCategory", "Category {0} is listed more than once."),
				FText::FromName(Row.Category.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		SeenCategories.Add(Row.Category);
	}
	TSet<int32> SeenTiers;
	for (const FRpgCraftingTierDisplay& Row : Tiers)
	{
		if (Row.Tier < 1 || SeenTiers.Contains(Row.Tier))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCraftingValidation", "InvalidCatalogTier", "Tier {0} is invalid or listed more than once."),
				FText::AsNumber(Row.Tier)));
			Result = EDataValidationResult::Invalid;
		}
		SeenTiers.Add(Row.Tier);
	}
	return Result;
}

#endif
