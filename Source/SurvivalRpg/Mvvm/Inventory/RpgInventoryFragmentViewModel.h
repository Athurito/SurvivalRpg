#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "RpgInventoryFragmentViewModel.generated.h"

class URpgInventoryItemInstance;

/**
 * Base class for optional item-fragment presenters used by inventory widgets.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryFragmentViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Initializes this presenter from one replicated inventory entry. UI-only; never mutates gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|ViewModel")
	virtual void InitializeFromEntry(const FRpgInventoryEntryView& Entry);

protected:
	/** Replicated item instance this presenter reads from. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Entry id represented by this presenter. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGuid EntryId;
};
