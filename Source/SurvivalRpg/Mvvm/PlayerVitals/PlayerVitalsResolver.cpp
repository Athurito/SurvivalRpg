// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerVitalsResolver.h"

#include "PlayerVitalsViewmodel.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "SurvivalRpg/Mvvm/Components/RpgVitalsViewModelComponent.h"

AActor* UPlayerVitalsResolver::ResolveContextActor(const UUserWidget* UserWidget) const
{
	if (!UserWidget) return nullptr;

	if (APlayerController* PC = UserWidget->GetOwningPlayer())
	{
		return PC;
	}

	if (const UWidgetComponent* WC = UserWidget->GetTypedOuter<UWidgetComponent>())
	{
		return WC->GetOwner();
	}

	return nullptr;
}

UObject* UPlayerVitalsResolver::CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const
{
	AActor* ContextActor = ResolveContextActor(UserWidget);
	if (!ContextActor)
	{
		return nullptr;
	}

	if (auto* VMComp = ContextActor->FindComponentByClass<URpgVitalsViewModelComponent>())
	{
		UPlayerVitalsViewmodel* VM = VMComp->GetViewModel();
		if (VM && VM->IsA(ExpectedType))
		{
			return VM;
		}
	}
	return nullptr;
}