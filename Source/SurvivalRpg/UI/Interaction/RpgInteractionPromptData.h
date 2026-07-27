// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Interaction/InteractionTypes.h"
#include "UObject/Object.h"

#include "RpgInteractionPromptData.generated.h"

struct FInteractionOption;
class UTexture2D;
class URpgInteractionPromptData;

/** Blueprint signal emitted after the prompt's presentation state changes semantically. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInteractionPromptDataChanged,
	URpgInteractionPromptData*,
	PromptData);

/** Native counterpart used by pooled C++ indicator widgets without per-frame polling. */
DECLARE_MULTICAST_DELEGATE_OneParam(
	FRpgInteractionPromptDataChangedNative,
	URpgInteractionPromptData*);

/**
 * Stable, local-only presentation model for one world interaction indicator.
 *
 * The interaction ability updates this object in place so pooled widgets can remain bound while
 * focus, range, availability, or localized prompt content changes. It never owns gameplay truth.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInteractionPromptData : public UObject
{
	GENERATED_BODY()

public:
	/** Copies presentation data from an option and broadcasts only when its semantic value changed. */
	bool UpdateFromOption(const FInteractionOption& Option, ERpgInteractionPromptState NewState);

	/** Clears transient presentation state and hides the prompt. Repeated clears are silent. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Interaction|Prompt")
	void Clear();

	/** Native event for event-driven C++ presentation consumers. */
	FRpgInteractionPromptDataChangedNative& OnPromptChangedNative() { return PromptChangedNative; }

	/** Current locally derived prompt state; gameplay authority never reads this value. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	ERpgInteractionPromptState State = ERpgInteractionPromptState::Hidden;

	/** Stable semantic action represented by this prompt. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	FGameplayTag InteractionTag;

	/** Weak actor/component/instance context used only for local presentation. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	FRpgInteractionTargetRef TargetRef;

	/** Localized action verb, such as Open, Revive, or Harvest. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	FText ActionText;

	/** Localized display name of the interaction target. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	FText TargetText;

	/** Localized explanation shown while the prompt is blocked. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	FText BlockedReason;

	/** Optional asynchronously loaded presentation icon; empty uses the authored widget fallback. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Target-authored semantic availability copied for UI styling and diagnostics. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Rpg|Interaction|Prompt")
	ERpgInteractionAvailability Availability = ERpgInteractionAvailability::Hidden;

	/** Blueprint presentation signal; it is emitted only for semantic or state changes. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Interaction|Prompt")
	FRpgInteractionPromptDataChanged OnPromptChanged;

private:
	void BroadcastChanged();

	FRpgInteractionPromptDataChangedNative PromptChangedNative;
};
