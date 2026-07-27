// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"

#include "RpgInteractionGrantAutomationTestTypes.generated.h"

class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class USceneComponent;

/** Inert ability used to identify temporary nearby-interaction specs in automation tests. */
UCLASS(NotBlueprintable, Transient)
class URpgInteractionGrantAutomationGrantedAbility final : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URpgInteractionGrantAutomationGrantedAbility();
};

/** Minimal authoritative pawn fixture that owns the ASC used by the grant task. */
UCLASS(NotBlueprintable, Transient)
class ARpgInteractionGrantAutomationPawn final
	: public APawn
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	explicit ARpgInteractionGrantAutomationPawn(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PostInitializeComponents() override;

	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const
	{
		return AbilitySystemComponent;
	}

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;
};
