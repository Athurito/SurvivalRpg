#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventorySplitViewModel.generated.h"

class UTexture2D;

/**
 * Read model of one open split dialog: the stack being split and the chosen amount.
 * The split dialog widget owns it and assigns it as a manual MVVM source; it never touches inventory state.
 */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgInventorySplitViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Shows the stack being split. TotalCount is its current size. */
	void SetStack(const FText& InDisplayName, const TSoftObjectPtr<UTexture2D>& InIcon, int32 InTotalCount);

	/** Sets the legal split range, normally 1 to TotalCount - 1. */
	void SetRange(int32 InMinimumCount, int32 InMaximumCount);

	/** Sets the chosen amount that moves into the new stack. */
	void SetSelectedCount(int32 InSelectedCount);

	/** Clears everything after the dialog closes. */
	void Clear();

protected:
	/** Item name of the stack. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Item icon of the stack. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** "99 total". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText TotalText;

	/** Smallest legal amount, as text for the slider's left end. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText MinimumText;

	/** Largest legal amount, as text for the slider's right end. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText MaximumText;

	/** Amount that moves into the new stack. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	int32 SelectedCount = 0;

	/** Amount that stays in the original stack. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	int32 RemainingCount = 0;

	/** "49 split · 50 remain". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText SummaryText;

	/** Label of the confirm button, "Split 49". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Inventory|Split", meta = (AllowPrivateAccess = "true"))
	FText ConfirmText;

private:
	void RefreshAmounts();

	int32 TotalCount = 0;
};
