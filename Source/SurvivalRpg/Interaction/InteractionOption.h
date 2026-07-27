// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "InteractionTypes.h"
#include "InteractionOption.generated.h"

class IInteractableTarget;
class UUserWidget;

/** Complete semantic, execution, and presentation contract for one target-authored interaction. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FInteractionOption
{
	GENERATED_BODY()

public:
	/** Runtime interface that produced this option. Authority always re-gathers it before mutation. */
	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IInteractableTarget> InteractableTarget;

	/** Stable semantic id used to match a locally presented option with an authoritative server option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Rpg.Interaction.Action"))
	FGameplayTag InteractionTag;

	/** Concrete actor/component/instance selected by the query that produced this option. */
	UPROPERTY(BlueprintReadWrite)
	FRpgInteractionTargetRef TargetRef;

	/** Designer-facing text, range, priority, icon, and prompt-widget configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRpgInteractionPromptDefinition Prompt;

	/** Target-authored semantic availability. Spatial state is derived separately by the interaction ability. */
	UPROPERTY(BlueprintReadWrite)
	ERpgInteractionAvailability Availability = ERpgInteractionAvailability::Available;

	/** Runtime presentation state derived by the active focus or nearby query. */
	UPROPERTY(BlueprintReadOnly)
	ERpgInteractionPromptState PromptState = ERpgInteractionPromptState::Hidden;

	/** Deprecated localized action text retained only while legacy producers migrate to Prompt.ActionText. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DeprecatedProperty, DeprecationMessage = "Use Prompt.ActionText."))
	FText Text;

	/** Deprecated localized target text retained only while legacy producers migrate to Prompt.TargetText. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DeprecatedProperty, DeprecationMessage = "Use Prompt.TargetText."))
	FText SubText;

	// METHODS OF INTERACTION
	//--------------------------------------------------------------

	// 1) Place an ability on the avatar that they can activate when they perform interaction.

	/** Target-authored ability class temporarily granted once per class by the authority-side nearby cache. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> InteractionAbilityToGrant;

	// - OR -

	// 2) Allow the object we're interacting with to have its own ability system and interaction ability, that we can activate instead.

	/** Runtime-resolved ability system that owns TargetInteractionAbilityHandle; never trusted from client presentation. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAbilitySystemComponent> TargetAbilitySystem = nullptr;

	/** Runtime-resolved ability spec selected only after current GAS activation checks. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayAbilitySpecHandle TargetInteractionAbilityHandle;

	// UI
	//--------------------------------------------------------------

	/** Deprecated focused widget override retained for one migration cycle; use Prompt.FocusWidgetClass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DeprecatedProperty, DeprecationMessage = "Use Prompt.FocusWidgetClass."))
	TSoftClassPtr<UUserWidget> InteractionWidgetClass;

	//--------------------------------------------------------------

public:
	FORCEINLINE bool operator==(const FInteractionOption& Other) const
	{
		return InteractableTarget == Other.InteractableTarget &&
			InteractionTag == Other.InteractionTag &&
			TargetRef.IsSemanticallyEqual(Other.TargetRef) &&
			Prompt.IsSemanticallyEqual(Other.Prompt) &&
			Availability == Other.Availability &&
			PromptState == Other.PromptState &&
			InteractionAbilityToGrant == Other.InteractionAbilityToGrant&&
			TargetAbilitySystem == Other.TargetAbilitySystem &&
			TargetInteractionAbilityHandle == Other.TargetInteractionAbilityHandle &&
			InteractionWidgetClass == Other.InteractionWidgetClass &&
			Text.IdenticalTo(Other.Text) &&
			SubText.IdenticalTo(Other.SubText);
	}

	FORCEINLINE bool operator!=(const FInteractionOption& Other) const
	{
		return !operator==(Other);
	}

	FORCEINLINE bool operator<(const FInteractionOption& Other) const
	{
		const FString TargetKey = GetPathNameSafe(InteractableTarget.GetObject());
		const FString OtherTargetKey = GetPathNameSafe(Other.InteractableTarget.GetObject());
		if (TargetKey != OtherTargetKey)
		{
			return TargetKey < OtherTargetKey;
		}
		const FString ComponentKey = GetPathNameSafe(TargetRef.TargetComponent.Get());
		const FString OtherComponentKey = GetPathNameSafe(Other.TargetRef.TargetComponent.Get());
		if (ComponentKey != OtherComponentKey)
		{
			return ComponentKey < OtherComponentKey;
		}
		if (TargetRef.InstanceIndex != Other.TargetRef.InstanceIndex)
		{
			return TargetRef.InstanceIndex < Other.TargetRef.InstanceIndex;
		}
		return InteractionTag.ToString() < Other.InteractionTag.ToString();
	}

	/** Applies deprecated Lyra fields as fallbacks and clamps invalid range ordering. */
	void NormalizeLegacyFields();

	/** Returns the best world point for distance checks and projected indicators. */
	FVector GetInteractionWorldLocation() const;
};
