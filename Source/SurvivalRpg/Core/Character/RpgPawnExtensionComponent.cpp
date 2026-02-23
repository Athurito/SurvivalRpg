// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

URpgPawnExtensionComponent::URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	AbilitySystemComponent = nullptr;
}

void URpgPawnExtensionComponent::SetPawnData(const UBasePawnData* InPawnData)
{
	check(InPawnData)
	if (PawnData) return;
	PawnData = InPawnData;
}

void URpgPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();
}
