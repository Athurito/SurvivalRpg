// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgUiSubsystem.h"

#include "PlayerVitals/PlayerVitalsViewmodel.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

UPlayerVitalsViewmodel* URpgUiSubsystem::GetVitalsViewmodel()
{
	if (!VitalsVM)
	{
		VitalsVM = NewObject<UPlayerVitalsViewmodel>(this); // Outer = LocalPlayerSubsystem (stabil)
	}
	return VitalsVM;
}

void URpgUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// ViewModel persistent erzeugen
	VitalsVM = NewObject<UPlayerVitalsViewmodel>(this);

	// NICHT sofort binden!
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::BindToControllerSafe
			)
		);
	}
}

void URpgUiSubsystem::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// Unbind old
	if (BoundExt && H_AscInit.IsValid())
	{
		BoundExt->OnAbilitySystemInitialized.Remove(H_AscInit);
		H_AscInit.Reset();
	}
	BoundExt = nullptr;

	if (!NewPawn)
	{
		VitalsVM->UnInitialize();
		return;
	}

	URpgPawnExtensionComponent* Ext =
		URpgPawnExtensionComponent::FindPawnExtensionComponent(NewPawn);
	if (!Ext)
		return;

	BoundExt = Ext;

	// Bind delegate (no params)
	H_AscInit = BoundExt->OnAbilitySystemInitialized.AddUObject(
		this,
		&ThisClass::HandleAscReady
	);

	// Falls ASC schon bereit:
	if (UAbilitySystemComponent* ExistingASC = BoundExt->GetRpgAbilitySystemComponent())
	{
		VitalsVM->Initialize(ExistingASC);
	}
}

void URpgUiSubsystem::HandleAscReady()
{
	if (!BoundExt)
		return;

	if (UAbilitySystemComponent* ASC = BoundExt->GetRpgAbilitySystemComponent())
	{
		VitalsVM->Initialize(ASC);
	}
}

void URpgUiSubsystem::BindToControllerSafe()
{
	if (!GetLocalPlayer())
		return;

	APlayerController* PC = GetLocalPlayer()->GetPlayerController(GetWorld());
	if (!PC)
		return;

	PC->OnPossessedPawnChanged.AddDynamic(
		this,
		&ThisClass::HandlePawnChanged
	);

	// Initial Sync
	HandlePawnChanged(nullptr, PC->GetPawn());
}
