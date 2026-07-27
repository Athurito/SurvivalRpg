// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"

#include "RpgInteractionReticleWidget.generated.h"

class UCommonUIActionRouterBase;
enum class ECommonInputMode : uint8;

/**
 * Local-only CommonUI base for the persistent interaction aiming reticle.
 *
 * The widget observes CommonUI's active input mode without ticking. It is hit-test invisible in
 * gameplay and collapsed while a menu or modal UI owns input; it never drives interaction state.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInteractionReticleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInteractionReticleWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgInteractionReticleInputModeTest;
#endif

	void BindCommonUiInputRouter();
	void UnbindCommonUiInputRouter();
	void HandleActiveInputModeChanged(ECommonInputMode ActiveInputMode);
	static bool ShouldShowForInputMode(ECommonInputMode ActiveInputMode);

	TWeakObjectPtr<UCommonUIActionRouterBase> ObservedCommonUiInputRouter;
	FDelegateHandle ActiveInputModeChangedHandle;
};
