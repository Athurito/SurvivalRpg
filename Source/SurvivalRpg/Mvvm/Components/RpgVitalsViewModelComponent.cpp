// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgVitalsViewModelComponent.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Mvvm/PlayerVitals/PlayerVitalsViewmodel.h"


void URpgVitalsViewModelComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer) return;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) return;

	ViewModel = NewObject<UPlayerVitalsViewmodel>(this);

	PC->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePawnChanged);

	// direkt initial binden, falls Pawn schon existiert
	HandlePawnChanged(nullptr, PC->GetPawn());
}

void URpgVitalsViewModelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ViewModel)
	{
		ViewModel->UnbindASC();
	}
	Super::EndPlay(EndPlayReason);
}

void URpgVitalsViewModelComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (ViewModel)
	{
		ViewModel->UnbindASC();
	}
	BindFromPawn(NewPawn);
}

void URpgVitalsViewModelComponent::BindFromPawn(APawn* Pawn)
{
	if (!Pawn || !ViewModel) return;

	if (auto* PawnExt = Pawn->FindComponentByClass<URpgPawnExtensionComponent>())
	{
		// Wenn ASC schon ready:
		if (UAbilitySystemComponent* ASC = PawnExt->GetRpgAbilitySystemComponent())
		{
			ViewModel->BindASC(ASC);
			return;
		}

		// Sonst auf ASC-ready warten (Lyra lifecycle)
		PawnExt->OnAbilitySystemInitialized.AddWeakLambda(this, [this, PawnExt]()
		{
			if (ViewModel)
			{
				if (UAbilitySystemComponent* ASC = PawnExt->GetRpgAbilitySystemComponent())
				{
					ViewModel->BindASC(ASC);
				}
			}
		});

		PawnExt->OnAbilitySystemUninitialized.AddWeakLambda(this, [this]()
		{
			if (ViewModel) ViewModel->UnbindASC();
		});
	}
}
