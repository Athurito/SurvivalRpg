#include "RpgFrontendWidgets.h"

#include "CommonActivatableWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgFrontendWidgets, Log, All);

URpgFrontendScreenWidget::URpgFrontendScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputConfig = ERpgWidgetInputMode::Menu;
}

URpgBootScreenWidget::URpgBootScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PageDisplayDurations = {1.0f, 2.0f, 2.0f};
	DestinationMap = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/SurvivalRpg/Maps/Menu/MainMenu.MainMenu")));
}

void URpgBootScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	ClearBootSequenceTimer();
	CurrentPageIndex = INDEX_NONE;
	bTravelRequested = false;

	if (!WidgetSwitcher)
	{
		UE_LOG(LogRpgFrontendWidgets, Error,
			TEXT("Boot screen [%s] requires its authored WidgetSwitcher binding."),
			*GetNameSafe(this));
		return;
	}

	const int32 PageCount = WidgetSwitcher->GetNumWidgets();
	if (PageCount == 0 || PageDisplayDurations.Num() != PageCount)
	{
		UE_LOG(LogRpgFrontendWidgets, Error,
			TEXT("Boot screen [%s] has [%d] pages but [%d] display durations. Configure exactly one positive duration per page."),
			*GetNameSafe(this),
			PageCount,
			PageDisplayDurations.Num());
		return;
	}

	CurrentPageIndex = 0;
	WidgetSwitcher->SetActiveWidgetIndex(CurrentPageIndex);
	ScheduleCurrentPageAdvance();
}

void URpgBootScreenWidget::NativeOnDeactivated()
{
	ClearBootSequenceTimer();
	Super::NativeOnDeactivated();
}

void URpgBootScreenWidget::ScheduleCurrentPageAdvance()
{
	if (!GetWorld() || !PageDisplayDurations.IsValidIndex(CurrentPageIndex))
	{
		return;
	}

	const float DisplayDuration = FMath::Max(PageDisplayDurations[CurrentPageIndex], 0.01f);
	GetWorld()->GetTimerManager().SetTimer(
		BootSequenceTimerHandle,
		this,
		&ThisClass::AdvanceBootSequence,
		DisplayDuration,
		false);
}

void URpgBootScreenWidget::AdvanceBootSequence()
{
	if (!IsActivated() || !WidgetSwitcher)
	{
		return;
	}

	++CurrentPageIndex;
	if (PageDisplayDurations.IsValidIndex(CurrentPageIndex))
	{
		WidgetSwitcher->SetActiveWidgetIndex(CurrentPageIndex);
		ScheduleCurrentPageAdvance();
		return;
	}

	if (bTravelRequested)
	{
		return;
	}
	bTravelRequested = true;

	if (DestinationMap.IsNull())
	{
		UE_LOG(LogRpgFrontendWidgets, Error,
			TEXT("Boot screen [%s] cannot travel because DestinationMap is unset."),
			*GetNameSafe(this));
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationMap);
}

void URpgBootScreenWidget::ClearBootSequenceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BootSequenceTimerHandle);
	}
}

void URpgMainMenuStackWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (MenuStack && MenuStack->GetNumWidgets() == 0 && InitialMenuClass)
	{
		PushToMainStack(InitialMenuClass);
	}
}

UCommonActivatableWidget* URpgMainMenuStackWidget::PushToMainStack(
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	return MenuStack && ActivatableWidgetClass
		? MenuStack->AddWidget<UCommonActivatableWidget>(ActivatableWidgetClass)
		: nullptr;
}

UCommonActivatableWidget* URpgMainMenuStackWidget::PushToOptionStack(
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	return OptionStack && ActivatableWidgetClass
		? OptionStack->AddWidget<UCommonActivatableWidget>(ActivatableWidgetClass)
		: nullptr;
}

UCommonActivatableWidget* URpgMainMenuStackWidget::PushToPopupStack(
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	return PopupStack && ActivatableWidgetClass
		? PopupStack->AddWidget<UCommonActivatableWidget>(ActivatableWidgetClass)
		: nullptr;
}

UCommonActivatableWidget* URpgMainMenuStackWidget::PushToOption1Stack(
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	return OptionStack_1 && ActivatableWidgetClass
		? OptionStack_1->AddWidget<UCommonActivatableWidget>(ActivatableWidgetClass)
		: nullptr;
}

UCommonActivatableWidget* URpgMainMenuStackWidget::PushToOption2Stack(
	TSubclassOf<UCommonActivatableWidget> ActivatableWidgetClass)
{
	return OptionStack_2 && ActivatableWidgetClass
		? OptionStack_2->AddWidget<UCommonActivatableWidget>(ActivatableWidgetClass)
		: nullptr;
}
