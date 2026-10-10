// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgUiSubsystem.h"

#include "Character/RpgCharacterStatsViewModel.h"
#include "Inventory/RpgPickupFeedViewModels.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
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
	if (PickupFeedVM && PickupFeedVM->GetClass() == ExpectedType)
	{
		return PickupFeedVM;
	}
	return nullptr;
}

bool URpgUiSubsystem::ProvidesViewModelClass(const UClass* Class)
{
	return Class == URpgCharacterStatsViewModel::StaticClass() ||
		Class == URpgPickupFeedViewModel::StaticClass();
}

void URpgUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CharacterStatsVM = NewObject<URpgCharacterStatsViewModel>(this);
	PickupFeedVM = NewObject<URpgPickupFeedViewModel>(this);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		PlayerControllerChanged(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void URpgUiSubsystem::Deinitialize()
{
	UnbindFromPlayerController();

	CharacterStatsVM = nullptr;
	PickupFeedVM = nullptr;

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
	if (PickupFeedVM)
	{
		PickupFeedVM->BindPlayerController(BoundPlayerController);
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
	if (PickupFeedVM)
	{
		PickupFeedVM->Unbind();
	}
}

void URpgUiSubsystem::BindToPawn(APawn* NewPawn)
{
	if (!NewPawn || !CharacterStatsVM)
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
		if (CharacterStatsVM)
		{
			CharacterStatsVM->BindAbilitySystem(ASC);
		}
	}
}

void URpgUiSubsystem::HandleAbilitySystemUninitialized()
{
	if (CharacterStatsVM)
	{
		CharacterStatsVM->UnbindAbilitySystem();
	}
}
