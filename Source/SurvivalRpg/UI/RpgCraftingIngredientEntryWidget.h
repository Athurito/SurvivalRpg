#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingIngredientEntryWidget.generated.h"

class URpgCraftingIngredientViewModel;
class UCommonLazyImage;
class UCommonTextBlock;
class UTexture2D;

/** Pooled read-only ingredient row backed by one exact optional manual MVVM source. */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgCraftingIngredientEntryWidget
	: public UCommonUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Exact source name authored by CUI_CraftingIngredientEntrySpatial. */
	static const FName IngredientViewModelSourceName;

	/** Read-only ingredient row currently represented by this pooled widget. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Ingredient")
	URpgCraftingIngredientViewModel* GetIngredientViewModel() const
	{
		return IngredientViewModel.Get();
	}

	/** MVVM destination for the ingredient icon. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Ingredient")
	void SetIngredientIcon(TSoftObjectPtr<UTexture2D> InIcon);

	/** MVVM destinations that format numeric counts without generated Blueprint conversion graphs. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Ingredient")
	void SetAvailableCount(int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Ingredient")
	void SetRequiredCount(int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Ingredient")
	void SetMissingCount(int32 InCount);

	/** Applies presentation-only missing-resource emphasis. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Ingredient")
	void SetHasEnough(bool bInHasEnough);

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	//~End of IUserListEntry interface

	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DisplayNameText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> AvailableCountText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RequiredCountText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> MissingCountText = nullptr;

private:
	void SetIngredientViewModel(
		URpgCraftingIngredientViewModel* InViewModel);

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingIngredientViewModel> IngredientViewModel = nullptr;
};
