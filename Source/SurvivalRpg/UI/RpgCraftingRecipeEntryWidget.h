#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "SurvivalRpg/UI/RpgCraftingActionButtonWidget.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingRecipeEntryWidget.generated.h"

class URpgCraftingRecipeViewModel;
class UCommonLazyImage;
class UCommonTextBlock;
class UTexture2D;

/**
 * Pooled recipe-row presenter with one exact optional manual MVVM source.
 *
 * The row owns no recipe selection or crafting command. UCommonListView reports selection to the screen presenter,
 * while this leaf only binds the supplied read-only recipe view model.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgCraftingRecipeEntryWidget
	: public URpgCraftingActionButtonWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Exact source name authored by CUI_CraftingRecipeEntrySpatial. */
	static const FName RecipeViewModelSourceName;

	/** Read-only row currently represented by this pooled widget. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Recipe")
	URpgCraftingRecipeViewModel* GetRecipeViewModel() const
	{
		return RecipeViewModel.Get();
	}

	/** MVVM destination for the recipe icon; keeps conversion logic out of the Widget Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Recipe")
	void SetRecipeIcon(TSoftObjectPtr<UTexture2D> InIcon);

	/** MVVM destination for the numeric recipe tier; formats presentation text natively. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Recipe")
	void SetRecipeTier(int32 InRecipeTier);

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	//~End of IUserListEntry interface

	virtual void NativeDestruct() override;

	/** Optional compact description authored by the canonical recipe row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DescriptionText = nullptr;

	/** Optional compact output summary authored by the canonical recipe row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> OutputSummaryText = nullptr;

	/** Optional tier badge text authored by the canonical recipe row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RecipeTierText = nullptr;

	/** Optional recipe icon authored by the canonical recipe row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> Icon = nullptr;

private:
	void SetRecipeViewModel(URpgCraftingRecipeViewModel* InViewModel);

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingRecipeViewModel> RecipeViewModel = nullptr;
};
