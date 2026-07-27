// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ArrowComponent.h"

#include "RpgInteractionPromptAnchorComponent.generated.h"

class URpgInteractionPromptAnchorComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(
	FRpgInteractionPromptAnchorDestroyedNative,
	URpgInteractionPromptAnchorComponent*);

/**
 * Designer-placeable cosmetic projection point for an interaction prompt.
 *
 * This component has no collision, tick, replication, or gameplay authority. Interaction
 * distance, visibility, and execution continue to use the option's target reference.
 */
UCLASS(
	BlueprintType,
	ClassGroup = (Interaction),
	meta = (BlueprintSpawnableComponent, DisplayName = "RPG Interaction Prompt Anchor"))
class SURVIVALRPG_API URpgInteractionPromptAnchorComponent final : public UArrowComponent
{
	GENERATED_BODY()

public:
	explicit URpgInteractionPromptAnchorComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Stable designer id selected by PromptDefinition.PromptAnchorId; cosmetic and never replicated or saved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Prompt")
	FName AnchorId = TEXT("Default");

	/** Returns the cosmetic id used while resolving a prompt anchor on the owning actor. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Interaction|Prompt")
	FName GetAnchorId() const { return AnchorId; }

	/** Returns false as soon as component destruction begins, allowing projection to fall back safely. */
	bool IsAvailableForPromptPlacement() const { return !bIsBeingDestroyed; }

	/** Native local-only signal used to re-resolve a live descriptor before this component disappears. */
	FRpgInteractionPromptAnchorDestroyedNative& OnPromptAnchorDestroyedNative()
	{
		return PromptAnchorDestroyedNative;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	bool bIsBeingDestroyed = false;
	FRpgInteractionPromptAnchorDestroyedNative PromptAnchorDestroyedNative;
};
