// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsResolver.h"

#include "PlayerVitalsViewmodel.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Mvvm/RpgUiSubsystem.h"

ULocalPlayer* UPlayerVitalsResolver::ResolveLocalPlayer(const UUserWidget* UserWidget) const
{
	if (!UserWidget)
	{
		return nullptr;
	}

	if (ULocalPlayer* LocalPlayer = UserWidget->GetOwningLocalPlayer())
	{
		return LocalPlayer;
	}

	if (APlayerController* PC = UserWidget->GetOwningPlayer())
	{
		return PC->GetLocalPlayer();
	}

	return nullptr;
}

UObject* UPlayerVitalsResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const
{
	ULocalPlayer* LocalPlayer = ResolveLocalPlayer(UserWidget);
	if (!LocalPlayer)
	{
		return nullptr;
	}

	if (URpgUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<URpgUiSubsystem>())
	{
		UPlayerVitalsViewmodel* VM = UiSubsystem->GetVitalsViewmodel();
		if (VM && VM->IsA(ExpectedType))
		{
			return VM;
		}
	}

	return nullptr;
}
