#pragma once

#include "CommonUserWidget.h"

#include "RpgBaseResourceListWidget.generated.h"

class UCommonListView;
class URpgBaseStorageViewModel;

/**
 * Typed authored presenter for one base-storage resource list.
 *
 * The parent screen owns and binds the read-only base-storage view model. This widget only mirrors its stable row
 * objects into the authored CommonListView and never mutates base storage.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgBaseResourceListWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Starts observing the supplied screen-owned VM, or releases the list when null. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Presentation")
	void SetBaseStorageViewModel(URpgBaseStorageViewModel* InViewModel);

	/** Idempotently removes only this presenter's listener and clears generated rows. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Presentation")
	void ReleaseBaseStoragePresentation();

	/** Current screen-owned read-only VM, or null after release. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Presentation")
	URpgBaseStorageViewModel* GetBaseStorageViewModel() const { return BaseStorageViewModel.Get(); }

	/** Authored CommonListView used for resource rows. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Presentation")
	UCommonListView* GetResourceList() const { return ResourceList.Get(); }

protected:
	virtual void NativeDestruct() override;

	/** Required authored list receiving the read-only base-resource projection. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonListView> ResourceList = nullptr;

private:
	UFUNCTION()
	void RefreshResourceItems();

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageViewModel> BaseStorageViewModel = nullptr;
};
