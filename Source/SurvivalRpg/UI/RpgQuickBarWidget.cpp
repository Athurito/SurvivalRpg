#include "RpgQuickBarWidget.h"

#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickBarWidget)

URpgQuickBarWidget::URpgQuickBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgQuickBarWidget::BindPlayerController(APlayerController* InPlayerController)
{
	if (!QuickBarViewModel)
	{
		QuickBarViewModel = NewObject<URpgQuickBarViewModel>(this);
	}

	if (QuickBarViewModel)
	{
		QuickBarViewModel->BindPlayerController(InPlayerController);
		BP_OnQuickBarViewModelReady(QuickBarViewModel);
	}
}

void URpgQuickBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!QuickBarViewModel)
	{
		QuickBarViewModel = NewObject<URpgQuickBarViewModel>(this);
	}
}

void URpgQuickBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bAutoBindToOwningPlayer)
	{
		BindPlayerController(GetOwningPlayer());
	}
	else
	{
		BP_OnQuickBarViewModelReady(QuickBarViewModel);
	}
}
