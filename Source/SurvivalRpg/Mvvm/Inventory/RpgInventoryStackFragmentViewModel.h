#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"

#include "RpgInventoryStackFragmentViewModel.generated.h"

/**
 * Presenter for replicated stack data.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryStackFragmentViewModel : public URpgInventoryFragmentViewModel
{
	GENERATED_BODY()

public:
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry) override;

protected:
	/** Current replicated stack count. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Stack", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;
};
