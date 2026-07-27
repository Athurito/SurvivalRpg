#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryUiDataFragmentViewModel.generated.h"

class UTexture2D;

/**
 * Presenter for static UI data such as icon and tooltip text.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryUiDataFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Optional item icon used by inventory-style widgets. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Compact display name for slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** Tooltip text shown in inventory panels. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** UI-only presentation tags such as rarity or item family. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|UI", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer PresentationTags;
};
