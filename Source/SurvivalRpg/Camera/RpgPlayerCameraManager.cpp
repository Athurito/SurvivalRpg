// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgPlayerCameraManager.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "RpgCameraComponent.h"
#include "RpgUICameraManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerCameraManager)

class FDebugDisplayInfo;

static FName UICameraComponentName(TEXT("UICamera"));

ARpgPlayerCameraManager::ARpgPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultFOV = Rpg_CAMERA_DEFAULT_FOV;
	ViewPitchMin = Rpg_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = Rpg_CAMERA_DEFAULT_PITCH_MAX;

	UICamera = CreateDefaultSubobject<URpgUICameraManagerComponent>(UICameraComponentName);
}

URpgUICameraManagerComponent* ARpgPlayerCameraManager::GetUICameraComponent() const
{
	return UICamera;
}

void ARpgPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	// If the UI Camera is looking at something, let it have priority.
	if (UICamera->NeedsToUpdateViewTarget())
	{
		Super::UpdateViewTarget(OutVT, DeltaTime);
		UICamera->UpdateViewTarget(OutVT, DeltaTime);
		return;
	}

	Super::UpdateViewTarget(OutVT, DeltaTime);
}

void ARpgPlayerCameraManager::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("RpgPlayerCameraManager: %s"), *GetNameSafe(this)));

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	const APawn* Pawn = (PCOwner ? PCOwner->GetPawn() : nullptr);

	if (const URpgCameraComponent* CameraComponent = URpgCameraComponent::FindCameraComponent(Pawn))
	{
		CameraComponent->DrawDebug(Canvas);
	}
}

