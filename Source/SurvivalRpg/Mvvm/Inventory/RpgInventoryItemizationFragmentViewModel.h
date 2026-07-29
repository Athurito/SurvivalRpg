#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationTypes.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"

#include "RpgInventoryItemizationFragmentViewModel.generated.h"

class URpgInventoryItemizationFragmentViewModel;

/** Broadcast when replicated generated-item presentation changes without replacing the view model. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInventoryItemizationPresentationChanged,
	URpgInventoryItemizationFragmentViewModel*,
	ViewModel);

/** One ordered, read-only row displayed in generated-item details. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemizationDisplayRow
{
	GENERATED_BODY()

	/** Localized stat or affix label. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Itemization")
	FText Label;

	/** Rolled numeric value; its units are defined by StatTag. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Itemization")
	float Value = 0.0f;

	/** Stable gameplay stat represented by this row. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Itemization")
	FGameplayTag StatTag;

	/** True for an affix row and false for a definition base-stat row. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Itemization")
	bool bAffix = false;
};

/** UI-only presenter for a concrete item's replicated Diablo-lite rolls. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryItemizationFragmentViewModel final
	: public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;
	virtual void BeginDestroy() override;

	/** Whether the represented concrete item contains an explicit server-generated roll. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	bool IsGenerated() const { return bGenerated; }

	/** Server-generated item level used for the displayed roll values. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	int32 GetItemLevel() const { return ItemLevel; }

	/** Generated four-tier rarity. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	ERpgItemRarity GetRarity() const { return Rarity; }

	/** Localized rarity label suitable for tooltips and detail panels. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	FText GetRarityLabel() const { return RarityLabel; }

	/** Presentation-only rarity color; gameplay must never read this value. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	FLinearColor GetRarityColor() const { return RarityColor; }

	/** Base stats followed by unique affixes in deterministic generated order. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Itemization")
	const TArray<FRpgItemizationDisplayRow>& GetStatRows() const { return StatRows; }

	/** Fired when a replicated itemization roll changes while this presenter remains bound. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Itemization")
	FRpgInventoryItemizationPresentationChanged OnPresentationChanged;

protected:
	/** Whether the concrete item was generated and should display randomized details. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	bool bGenerated = false;

	/** Server-generated item level used to evaluate its roll ranges. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	int32 ItemLevel = 0;

	/** Four-tier generated quality. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	ERpgItemRarity Rarity = ERpgItemRarity::Common;

	/** Localized rarity label for inventory details. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	FText RarityLabel;

	/** Presentation-only rarity color; gameplay never reads this value. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	FLinearColor RarityColor = FLinearColor::White;

	/** Base stats followed by unique affixes in deterministic generated order. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Itemization", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgItemizationDisplayRow> StatRows;

private:
	UFUNCTION()
	void HandleItemizationStateChanged(const FRpgItemizationState& NewState);

	void ApplyState(const FRpgItemizationState& NewState);
};
