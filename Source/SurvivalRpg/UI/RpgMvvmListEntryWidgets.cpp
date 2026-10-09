#include "RpgMvvmListEntryWidgets.h"

#include "CommonLazyImage.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/UI/RpgMvvmWidgetUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMvvmListEntryWidgets)

namespace RpgMvvmListEntryWidgets
{
	void AssignSource(UUserWidget* Widget, FName SourceName, TSubclassOf<UMVVMViewModelBase> ViewModelClass, UObject* Item)
	{
		if (SourceName.IsNone())
		{
			return;
		}
		UClass* ExpectedClass = ViewModelClass ? ViewModelClass.Get() : UMVVMViewModelBase::StaticClass();
		UMVVMViewModelBase* ViewModel = Item && Item->IsA(ExpectedClass) ? Cast<UMVVMViewModelBase>(Item) : nullptr;
		RpgMvvmWidgetUtils::SetOptionalManualViewModel(Widget, SourceName, ViewModel, ExpectedClass);
	}

	void ApplyIcon(UCommonLazyImage* Image, const TSoftObjectPtr<UTexture2D>& InIcon)
	{
		if (!Image)
		{
			return;
		}
		if (InIcon.IsNull())
		{
			Image->SetBrushFromTexture(nullptr);
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Image->SetBrushFromLazyTexture(InIcon, /*bMatchSize=*/ false);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URpgMvvmListEntryWidget::SetEntryIcon(TSoftObjectPtr<UTexture2D> InIcon)
{
	RpgMvvmListEntryWidgets::ApplyIcon(Icon, InIcon);
}

void URpgMvvmListEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	AssignEntryItem(ListItemObject);
}

void URpgMvvmListEntryWidget::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	AssignEntryItem(nullptr);
}

void URpgMvvmListEntryWidget::NativeDestruct()
{
	AssignEntryItem(nullptr);
	Super::NativeDestruct();
}

void URpgMvvmListEntryWidget::AssignEntryItem(UObject* Item)
{
	EntryItem = Item;
	RpgMvvmListEntryWidgets::AssignSource(this, ViewModelSourceName, ViewModelClass, Item);
}

void URpgMvvmListEntryButton::SetEntryIcon(TSoftObjectPtr<UTexture2D> InIcon)
{
	RpgMvvmListEntryWidgets::ApplyIcon(Icon, InIcon);
}

void URpgMvvmListEntryButton::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	AssignEntryItem(ListItemObject);
}

void URpgMvvmListEntryButton::NativeOnEntryReleased()
{
	IUserListEntry::NativeOnEntryReleased();
	StopAllAnimations();
	AssignEntryItem(nullptr);
}

void URpgMvvmListEntryButton::NativeDestruct()
{
	AssignEntryItem(nullptr);
	Super::NativeDestruct();
}

void URpgMvvmListEntryButton::AssignEntryItem(UObject* Item)
{
	EntryItem = Item;
	RpgMvvmListEntryWidgets::AssignSource(this, ViewModelSourceName, ViewModelClass, Item);
}
