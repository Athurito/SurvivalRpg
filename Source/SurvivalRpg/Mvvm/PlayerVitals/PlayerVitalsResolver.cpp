// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsResolver.h"

#include "PlayerVitalsViewmodel.h"
#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/Mvvm/RpgUiSubsystem.h"

UObject* UPlayerVitalsResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget,
                                               const UMVVMView* View) const
{
	
	if (!UserWidget) return nullptr;

	if (ExpectedType && !ExpectedType->IsChildOf(UPlayerVitalsViewmodel::StaticClass()))
	{
		return nullptr;
	}

	ULocalPlayer* LP = UserWidget->GetOwningLocalPlayer();
	if (!LP) return nullptr;

	URpgUiSubsystem* UISub = LP->GetSubsystem<URpgUiSubsystem>();
	if (!UISub) return nullptr;

	UPlayerVitalsViewmodel* VM = UISub->GetOrCreateVitalsVM();
	if (!VM) return nullptr;

	// Darf früh sein: VM bindet später via PawnExtension->OnAscReady
	if (APlayerController* PC = UserWidget->GetOwningPlayer())
	{
		VM->BindToPlayer(PC);
	}

	return VM;
}
