// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgUiSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "PlayerVitals/PlayerVitalsViewmodel.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

void URpgUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	VitalsVM = NewObject<UPlayerVitalsViewmodel>(this);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		PlayerControllerChanged(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void URpgUiSubsystem::Deinitialize()
{
	UnbindFromPlayerController();

	if (VitalsVM)
	{
		VitalsVM->UnbindASC();
	}

	VitalsVM = nullptr;

	Super::Deinitialize();
}

void URpgUiSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	BindToPlayerController(NewPlayerController);
}

void URpgUiSubsystem::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UnbindFromPawnExtension();
	BindToPawn(NewPawn);
}

void URpgUiSubsystem::BindToPlayerController(APlayerController* NewPlayerController)
{
	if (BoundPlayerController == NewPlayerController)
	{
		return;
	}

	UnbindFromPlayerController();

	if (!NewPlayerController || !NewPlayerController->IsLocalController())
	{
		return;
	}

	BoundPlayerController = NewPlayerController;
	BoundPlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePawnChanged);

	HandlePawnChanged(nullptr, BoundPlayerController->GetPawn());
}

void URpgUiSubsystem::UnbindFromPlayerController()
{
	if (BoundPlayerController)
	{
		BoundPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
		BoundPlayerController = nullptr;
	}

	UnbindFromPawnExtension();
}

void URpgUiSubsystem::BindToPawn(APawn* NewPawn)
{
	if (!NewPawn || !VitalsVM)
	{
		return;
	}

	BoundPawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(NewPawn);
	if (!BoundPawnExtension)
	{
		return;
	}

	BoundPawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
	BoundPawnExtension->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
}

void URpgUiSubsystem::UnbindFromPawnExtension(bool bResetViewModel)
{
	if (BoundPawnExtension)
	{
		BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
		BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
		BoundPawnExtension = nullptr;
	}

	if (bResetViewModel && VitalsVM)
	{
		VitalsVM->UnbindASC();
	}
}

void URpgUiSubsystem::HandleAbilitySystemInitialized()
{
	if (!VitalsVM || !BoundPawnExtension)
	{
		return;
	}

	if (URpgAbilitySystemComponent* ASC = BoundPawnExtension->GetRpgAbilitySystemComponent())
	{
		VitalsVM->BindASC(ASC);
	}
}

void URpgUiSubsystem::HandleAbilitySystemUninitialized()
{
	if (VitalsVM)
	{
		VitalsVM->UnbindASC();
	}
}
