// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractableComponent.h"

UInteractableComponent::UInteractableComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UInteractableComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery,
	FInteractionOptionBuilder& InteractionBuilder)
{

}

