#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemTypes.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"

#include "RpgInventoryTraitsFragmentViewModel.generated.h"

/**
 * Presenter for gameplay-facing item traits that UI may display or use for filters.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryTraitsFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Broad item category used for UI grouping and sorting. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	ERpgInventoryItemCategory ItemCategory = ERpgInventoryItemCategory::Misc;

	/** Gameplay tags exposed for UI filters and recipe previews. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ItemTags;

	/** Whether this item is treated as a material for UI grouping and death-drop previews. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Traits", meta = (AllowPrivateAccess = "true"))
	bool bIsMaterial = false;
};
