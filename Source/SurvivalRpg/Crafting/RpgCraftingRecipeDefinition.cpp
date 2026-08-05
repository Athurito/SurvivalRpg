#include "RpgCraftingRecipeDefinition.h"

#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingRecipeDefinition)

#if WITH_EDITOR

EDataValidationResult URpgCraftingRecipeDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	const FGameplayTag KnowledgeRoot = FGameplayTag::RequestGameplayTag(
		TEXT("Storage.Knowledge"),
		false);
	for (const FGameplayTag KnowledgeTag : RequiredWorldKnowledgeTags)
	{
		if (!KnowledgeRoot.IsValid() || KnowledgeTag == KnowledgeRoot ||
			!KnowledgeTag.MatchesTag(KnowledgeRoot))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCraftingValidation", "InvalidWorldKnowledgeTag", "RequiredWorldKnowledgeTags contains invalid non-concrete tag {0}."),
				FText::FromName(KnowledgeTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (const FRpgCraftingResourceCost& Cost : RequiredResources)
	{
		if (!Cost.ItemDefinition || Cost.Count <= 0)
		{
			Context.AddError(NSLOCTEXT(
				"RpgCraftingValidation",
				"InvalidRecipeCost",
				"RequiredResources contains a null definition or non-positive count."));
			Result = EDataValidationResult::Invalid;
		}
	}
	if (OutputItems.IsEmpty())
	{
		Context.AddError(NSLOCTEXT(
			"RpgCraftingValidation",
			"MissingRecipeOutput",
			"A crafting recipe must define at least one deterministic output."));
		Result = EDataValidationResult::Invalid;
	}
	for (const FRpgCraftingOutputItem& Output : OutputItems)
	{
		if (!Output.ItemDefinition || Output.Count <= 0)
		{
			Context.AddError(NSLOCTEXT(
				"RpgCraftingValidation",
				"InvalidRecipeOutput",
				"OutputItems contains a null definition or non-positive count."));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}

EDataValidationResult URpgCraftingRecipeSet::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	TSet<const URpgCraftingRecipeDefinition*> SeenRecipes;
	for (const URpgCraftingRecipeDefinition* Recipe : Recipes)
	{
		if (!Recipe)
		{
			Context.AddError(NSLOCTEXT(
				"RpgCraftingValidation",
				"NullRecipeSetEntry",
				"Recipes contains a null entry."));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenRecipes.Contains(Recipe))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCraftingValidation", "DuplicateRecipeSetEntry", "Recipe {0} is referenced more than once."),
				FText::FromString(Recipe->GetPathName())));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			SeenRecipes.Add(Recipe);
		}
	}
	return Result;
}

#endif
