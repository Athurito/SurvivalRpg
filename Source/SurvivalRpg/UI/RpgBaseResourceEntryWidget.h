#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"

#include "RpgBaseResourceEntryWidget.generated.h"

class URpgBaseResourceEntryViewModel;

/**
 * Pooled read-only base-resource row backed by one exact optional manual MVVM source.
 *
 * The shared base storage remains the authoritative gameplay state. This leaf only exposes the row view model
 * supplied by its owning CommonListView and clears that presentation state whenever the entry returns to the pool.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgBaseResourceEntryWidget
	: public UCommonUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Exact optional manual source name authored by the canonical base-resource entry. */
	static const FName BaseResourceEntryViewModelSourceName;

	/** Read-only base-resource row currently represented by this pooled widget. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Resource")
	URpgBaseResourceEntryViewModel* GetBaseResourceEntryViewModel() const
	{
		return BaseResourceEntryViewModel.Get();
	}

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	//~End of IUserListEntry interface

	virtual void NativeDestruct() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgBaseResourceEntryPoolingTest;
#endif

	void SetBaseResourceEntryViewModel(
		URpgBaseResourceEntryViewModel* InViewModel);

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseResourceEntryViewModel> BaseResourceEntryViewModel = nullptr;
};
