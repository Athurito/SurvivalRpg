#include "RpgBaseResourceEntryWidget.h"

#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseResourceEntryWidget)

const FName URpgBaseResourceEntryWidget::BaseResourceEntryViewModelSourceName(
	TEXT("RpgBaseResourceEntryViewModel"));

void URpgBaseResourceEntryWidget::NativeOnListItemObjectSet(
	UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	SetBaseResourceEntryViewModel(
		Cast<URpgBaseResourceEntryViewModel>(ListItemObject));
}

void URpgBaseResourceEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	SetBaseResourceEntryViewModel(nullptr);
}

void URpgBaseResourceEntryWidget::NativeDestruct()
{
	SetBaseResourceEntryViewModel(nullptr);
	Super::NativeDestruct();
}

void URpgBaseResourceEntryWidget::SetBaseResourceEntryViewModel(
	URpgBaseResourceEntryViewModel* InViewModel)
{
	BaseResourceEntryViewModel = InViewModel;
	RpgMvvmWidgetUtils::SetOptionalManualViewModel(
		this,
		BaseResourceEntryViewModelSourceName,
		BaseResourceEntryViewModel,
		URpgBaseResourceEntryViewModel::StaticClass());
}
