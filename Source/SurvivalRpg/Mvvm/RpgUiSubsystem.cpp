// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgUiSubsystem.h"

#include "Character/RpgCharacterStatsViewModel.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "PlayerVitals/PlayerVitalsViewmodel.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

UObject* URpgUiSubsystem::FindViewModel(const UClass* ExpectedType) const
{
	if (!ExpectedType)
	{
		return nullptr;
	}
	if (CharacterStatsVM && CharacterStatsVM->GetClass() == ExpectedType)
	{
		return CharacterStatsVM;
	}
	if (VitalsVM && VitalsVM->GetClass() == ExpectedType)
	{
		return VitalsVM;
	}
	return nullptr;
}

bool URpgUiSubsystem::ProvidesViewModelClass(const UClass* Class)
{
	return Class == URpgCharacterStatsViewModel::StaticClass() || Class == UPlayerVitalsViewmodel::StaticClass();
}

void URpgUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	VitalsVM = NewObject<UPlayerVitalsViewmodel>(this);
	CharacterStatsVM = NewObject<URpgCharacterStatsViewModel>(this);

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
	CharacterStatsVM = nullptr;

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
	if (CharacterStatsVM)
	{
		CharacterStatsVM->BindPlayerController(BoundPlayerController);
	}

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
	if (CharacterStatsVM)
	{
		CharacterStatsVM->Unbind();
	}
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
	if (bResetViewModel && CharacterStatsVM)
	{
		CharacterStatsVM->UnbindAbilitySystem();
	}
}

void URpgUiSubsystem::HandleAbilitySystemInitialized()
{
	if (!BoundPawnExtension)
	{
		return;
	}

	if (URpgAbilitySystemComponent* ASC = BoundPawnExtension->GetRpgAbilitySystemComponent())
	{
		if (VitalsVM)
		{
			VitalsVM->BindASC(ASC);
		}
		if (CharacterStatsVM)
		{
			CharacterStatsVM->BindAbilitySystem(ASC);
		}
	}
}

void URpgUiSubsystem::HandleAbilitySystemUninitialized()
{
	if (VitalsVM)
	{
		VitalsVM->UnbindASC();
	}
	if (CharacterStatsVM)
	{
		CharacterStatsVM->UnbindAbilitySystem();
	}
}
