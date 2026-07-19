#include "RpgBaseResourceListWidget.h"

#include "CommonListView.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseResourceListWidget)

void URpgBaseResourceListWidget::SetBaseStorageViewModel(
	URpgBaseStorageViewModel* InViewModel)
{
	if (BaseStorageViewModel == InViewModel)
	{
		RefreshResourceItems();
		return;
	}

	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->OnResourcesChanged.RemoveDynamic(
			this,
			&ThisClass::RefreshResourceItems);
	}

	BaseStorageViewModel = InViewModel;
	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->OnResourcesChanged.AddUniqueDynamic(
			this,
			&ThisClass::RefreshResourceItems);
	}

	RefreshResourceItems();
}

void URpgBaseResourceListWidget::ReleaseBaseStoragePresentation()
{
	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->OnResourcesChanged.RemoveDynamic(
			this,
			&ThisClass::RefreshResourceItems);
	}
	BaseStorageViewModel = nullptr;

	if (ResourceList)
	{
		ResourceList->ClearListItems();
	}
}

void URpgBaseResourceListWidget::NativeDestruct()
{
	ReleaseBaseStoragePresentation();
	Super::NativeDestruct();
}

void URpgBaseResourceListWidget::RefreshResourceItems()
{
	if (!ResourceList)
	{
		return;
	}

	const TArray<URpgBaseResourceEntryViewModel*> Resources =
		BaseStorageViewModel
			? BaseStorageViewModel->GetResources()
			: TArray<URpgBaseResourceEntryViewModel*>();
	ResourceList->SetListItems(Resources);
}
