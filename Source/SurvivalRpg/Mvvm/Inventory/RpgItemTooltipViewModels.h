#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgItemTooltipViewModels.generated.h"

class UTexture2D;
class URpgInventoryEntryViewModel;

/** How one stat compares with the same stat on the equipped item. Presentation only. */
UENUM(BlueprintType)
enum class ERpgItemStatComparison : uint8
{
	/** No comparison is active, or the equipped item has no such stat. */
	None,

	/** The stat is higher than on the equipped item. */
	Better,

	/** The stat is lower than on the equipped item. */
	Worse,

	/** The stat matches the equipped item. */
	Equal
};

/** One stat line in an item tooltip: a base stat or an affix, optionally compared with the equipped item. */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgItemStatRowViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Fills the row from a rolled stat. Clears any comparison. */
	void InitializeRow(const FText& InLabel, float InValue, FGameplayTag InStatTag, bool bInAffix);

	/** Compares this row with the equipped item's value for the same stat; an unset baseline clears the comparison. */
	void SetComparison(TOptional<float> BaselineValue);

	FGameplayTag GetStatTag() const { return StatTag; }
	float GetValue() const { return Value; }
	ERpgItemStatComparison GetComparison() const { return Comparison; }
	FText GetDeltaText() const { return DeltaText; }

protected:
	/** Localized stat name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText Label;

	/** Formatted value with at most two decimals; affixes carry a leading plus. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText ValueText;

	/** True for an affix and false for a base stat. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bAffix = false;

	/** Comparison with the equipped item; None while no comparison is active. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	ERpgItemStatComparison Comparison = ERpgItemStatComparison::None;

	/** Signed difference to the equipped item, for example "+3,1"; empty while no comparison is active. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText DeltaText;

	/** True while Comparison is not None. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasComparison = false;

private:
	float Value = 0.0f;
	FGameplayTag StatTag;
};

/**
 * Read model for one item tooltip: header, base stats, affixes and the comparison with the equipped item.
 *
 * Built from an inventory entry view model and its itemization child; it never reads or changes gameplay state
 * directly. The tooltip widget owns one instance for the hovered item and one for the equipped counterpart.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgItemTooltipViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds the tooltip from an entry; null clears it. */
	void SetEntry(const URpgInventoryEntryViewModel* Entry);

	/**
	 * Compares this item's stats with Baseline, usually the equipped item in the same slot.
	 * Null clears the comparison deltas.
	 */
	void ApplyComparisonBaseline(const URpgItemTooltipViewModel* Baseline);

	/** Shows or hides the comparison presentation on the hovered tooltip, for example while Shift is held. */
	void SetComparisonShown(bool bInShown);

	/** Marks whether an equipped counterpart exists that the player can compare with. */
	void SetCanCompare(bool bInCanCompare);

	/** Marks this model as the equipped item shown beside the hovered one. */
	void SetIsEquippedComparison(bool bInIsEquipped);

	bool HasItem() const { return bHasItem; }
	bool CanCompare() const { return bCanCompare; }
	const TArray<TObjectPtr<URpgItemStatRowViewModel>>& GetBaseStats() const { return BaseStats; }
	const TArray<TObjectPtr<URpgItemStatRowViewModel>>& GetAffixes() const { return Affixes; }

	/** Formats a stat value with at most two decimals, as shown in tooltips. */
	static FText FormatStatValue(float Value, bool bWithPlusSign);

	/** Player-facing name of an item category, for example "Material". */
	static FText GetCategoryLabel(ERpgInventoryItemCategory ItemCategory);

protected:
	/** True while an item is shown. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasItem = false;

	/** Full item name. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Item icon from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** True for generated items with a rolled rarity and item level. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasRarity = false;

	/** Localized rarity, for example "Rare"; empty without a roll. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText RarityLabel;

	/** Presentation colour of the rarity; white without a roll. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FLinearColor RarityColor = FLinearColor::White;

	/** "Item Level N" for generated items, otherwise the item category, for example "Material". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText SubtitleText;

	/** "99 pieces" for stacks above one; empty otherwise. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText QuantityText;

	/** True while QuantityText is shown. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasQuantity = false;

	/** Optional flavour or usage text from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** True while Description is not empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasDescription = false;

	/** Rolled base stats in generated order, shown as the large values. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgItemStatRowViewModel>> BaseStats;

	/** True while BaseStats is not empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasBaseStats = false;

	/** Rolled affixes in generated order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgItemStatRowViewModel>> Affixes;

	/** True while Affixes is not empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bHasAffixes = false;

	/** True when an equipped counterpart exists, so the tooltip shows its compare hint. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bCanCompare = false;

	/** True while the comparison is shown: the deltas and the equipped item beside this tooltip. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bComparisonShown = false;

	/** True when this model shows the equipped item beside the hovered one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Tooltip", meta = (AllowPrivateAccess = "true"))
	bool bIsEquippedComparison = false;

private:
	void SetBaseStatsAndAffixes(
		TArray<TObjectPtr<URpgItemStatRowViewModel>>&& NewBaseStats,
		TArray<TObjectPtr<URpgItemStatRowViewModel>>&& NewAffixes);

	TArray<TObjectPtr<URpgItemStatRowViewModel>> AcquireRows(
		TArray<TObjectPtr<URpgItemStatRowViewModel>>& Pool,
		int32 Count);
};
