#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "RpgQuickBarWidget.generated.h"

class APlayerController;
class URpgQuickBarViewModel;

/**
 * Native base for HUD quickbar widgets.
 *
 * Blueprint children such as CUI_Hotbar can use this to get a bound URpgQuickBarViewModel without
 * owning gameplay state. The view model observes the local player's replicated quickbar component.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgQuickBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgQuickBarWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Binds the internal quickbar view model to the supplied player controller. */
	UFUNCTION(BlueprintCallable, Category = "QuickBar|Widget")
	void BindPlayerController(APlayerController* InPlayerController);

	/** View model used by this HUD widget to render quickbar slots. */
	UFUNCTION(BlueprintPure, Category = "QuickBar|Widget")
	URpgQuickBarViewModel* GetQuickBarViewModel() const { return QuickBarViewModel; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	/** Called after the native quickbar view model exists and has been bound or refreshed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "QuickBar|Widget", meta = (DisplayName = "On QuickBar ViewModel Ready"))
	void BP_OnQuickBarViewModelReady(URpgQuickBarViewModel* ViewModel);

	/** If true, NativeConstruct binds the VM to this widget's owning player controller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "QuickBar|Widget")
	bool bAutoBindToOwningPlayer = true;

private:
	UPROPERTY(Transient)
	TObjectPtr<URpgQuickBarViewModel> QuickBarViewModel = nullptr;
};
