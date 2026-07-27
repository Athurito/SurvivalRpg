// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionGrantAutomationTestTypes.h"

#include "Components/SceneComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionGrantAutomationTestTypes)

URpgInteractionGrantAutomationGrantedAbility::
	URpgInteractionGrantAutomationGrantedAbility()
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

ARpgInteractionGrantAutomationPawn::
	ARpgInteractionGrantAutomationPawn(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	AbilitySystemComponent =
		CreateDefaultSubobject<URpgAbilitySystemComponent>(
			TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);
}

UAbilitySystemComponent*
	ARpgInteractionGrantAutomationPawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARpgInteractionGrantAutomationPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}
