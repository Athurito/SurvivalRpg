// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgIndicatorManagerComponent.h"

#include "GameFramework/PlayerController.h"
#include "IndicatorDescriptor.h"
#include "RpgIndicatorHostWidget.h"


URpgIndicatorManagerComponent::URpgIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bAutoActivate = true;
}

/*static*/ URpgIndicatorManagerComponent* URpgIndicatorManagerComponent::GetComponent(AController* Controller)
{
	if (Controller)
	{
		return Controller->FindComponentByClass<URpgIndicatorManagerComponent>();
	}

	return nullptr;
}

void URpgIndicatorManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (PlayerController && PlayerController->IsLocalController())
	{
		HostWidget = CreateWidget<URpgIndicatorHostWidget>(PlayerController, URpgIndicatorHostWidget::StaticClass());
		if (HostWidget)
		{
			HostWidget->AddToPlayerScreen(-10);
		}
	}
}

void URpgIndicatorManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HostWidget)
	{
		HostWidget->RemoveFromParent();
		HostWidget = nullptr;
	}

	Indicators.Reset();
	Super::EndPlay(EndPlayReason);
}

void URpgIndicatorManagerComponent::AddIndicator(UIndicatorDescriptor* IndicatorDescriptor)
{
	IndicatorDescriptor->SetIndicatorManagerComponent(this);
	OnIndicatorAdded.Broadcast(IndicatorDescriptor);
	Indicators.Add(IndicatorDescriptor);
}

void URpgIndicatorManagerComponent::RemoveIndicator(UIndicatorDescriptor* IndicatorDescriptor)
{
	if (IndicatorDescriptor)
	{
		ensure(IndicatorDescriptor->GetIndicatorManagerComponent() == this);
	
		OnIndicatorRemoved.Broadcast(IndicatorDescriptor);
		Indicators.Remove(IndicatorDescriptor);
	}
}
