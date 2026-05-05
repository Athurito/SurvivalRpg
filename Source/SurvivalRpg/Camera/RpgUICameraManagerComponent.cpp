// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgUICameraManagerComponent.h"

#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "RpgPlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgUICameraManagerComponent)

class AActor;
class FDebugDisplayInfo;

URpgUICameraManagerComponent* URpgUICameraManagerComponent::GetComponent(APlayerController* PC)
{
	if (PC != nullptr)
	{
		if (ARpgPlayerCameraManager* PCCamera = Cast<ARpgPlayerCameraManager>(PC->PlayerCameraManager))
		{
			return PCCamera->GetUICameraComponent();
		}
	}

	return nullptr;
}

URpgUICameraManagerComponent::URpgUICameraManagerComponent()
{
	bWantsInitializeComponent = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Register "showdebug" hook.
		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddUObject(this, &ThisClass::OnShowDebugInfo);
		}
	}
}

void URpgUICameraManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void URpgUICameraManagerComponent::SetViewTarget(AActor* InViewTarget, FViewTargetTransitionParams TransitionParams)
{
	TGuardValue<bool> UpdatingViewTargetGuard(bUpdatingViewTarget, true);

	ViewTarget = InViewTarget;
	CastChecked<ARpgPlayerCameraManager>(GetOwner())->SetViewTarget(ViewTarget, TransitionParams);
}

bool URpgUICameraManagerComponent::NeedsToUpdateViewTarget() const
{
	return false;
}

void URpgUICameraManagerComponent::UpdateViewTarget(struct FTViewTarget& OutVT, float DeltaTime)
{
}

void URpgUICameraManagerComponent::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
}
