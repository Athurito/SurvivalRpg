#include "RpgLocalPlayerViewModelResolver.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Mvvm/RpgUiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLocalPlayerViewModelResolver)

UObject* URpgLocalPlayerViewModelResolver::CreateInstance(
	const UClass* ExpectedType,
	const UUserWidget* UserWidget,
	const UMVVMView* View) const
{
	ULocalPlayer* LocalPlayer = UserWidget ? UserWidget->GetOwningLocalPlayer() : nullptr;
	if (!LocalPlayer && UserWidget)
	{
		const APlayerController* PlayerController = UserWidget->GetOwningPlayer();
		LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	}

	const URpgUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<URpgUiSubsystem>() : nullptr;
	return UiSubsystem ? UiSubsystem->FindViewModel(ExpectedType) : nullptr;
}

#if WITH_EDITOR
bool URpgLocalPlayerViewModelResolver::DoesSupportViewModelClass(const UClass* Class) const
{
	return URpgUiSubsystem::ProvidesViewModelClass(Class);
}
#endif
