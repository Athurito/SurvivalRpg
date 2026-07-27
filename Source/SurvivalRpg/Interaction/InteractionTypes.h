// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "InteractionTypes.generated.h"

class AActor;
class UPrimitiveComponent;
class UTexture2D;
class UUserWidget;

/** Presentation state derived locally from focus, distance, and option availability. */
UENUM(BlueprintType)
enum class ERpgInteractionPromptState : uint8
{
	Hidden,
	Nearby,
	FocusedOutOfRange,
	Ready,
	Blocked
};

/** Semantic availability supplied by the target before spatial checks are applied. */
UENUM(BlueprintType)
enum class ERpgInteractionAvailability : uint8
{
	Available,
	Blocked,
	Hidden
};

/** Identifies why interaction options are being gathered. */
UENUM(BlueprintType)
enum class ERpgInteractionQueryMode : uint8
{
	Focus,
	Nearby,
	AuthorityValidation
};

/**
 * Identifies the concrete actor, component, and optional instanced-mesh item used by an interaction.
 * References are weak and local; authoritative code always reconstructs this data from a server query.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInteractionTargetRef
{
	GENERATED_BODY()

	/** Actor that owns the interaction target. Runtime-derived and never authoritative when supplied by a client. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> TargetActor;

	/** Primitive hit or used as the projected prompt anchor. Runtime-derived and weakly referenced. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<UPrimitiveComponent> TargetComponent;

	/** ISM/HISM instance index, or INDEX_NONE for normal actors and components. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 InstanceIndex = INDEX_NONE;

	/** World-space interaction point in centimeters, normally copied from the authoritative hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector WorldLocation = FVector::ZeroVector;

	/** World-space surface normal associated with WorldLocation. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FVector WorldNormal = FVector::UpVector;

	/** Optional target-owned revision used to reject stale instance or state requests. INDEX_NONE disables the check. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Revision = INDEX_NONE;

	bool IsValid() const;
	bool IsSemanticallyEqual(const FRpgInteractionTargetRef& Other) const;
};

/** Designer-authored prompt presentation and spatial tuning for one interaction option. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInteractionPromptDefinition
{
	GENERATED_BODY()

	/** Localized verb displayed for the action, such as Open, Revive, or Harvest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	FText ActionText;

	/** Localized target name displayed beneath or beside the action text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	FText TargetText;

	/** Localized explanation shown when Availability is Blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	FText BlockedReason;

	/** Optional lazily loaded world-interaction icon. Empty uses the widget's generic fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Maximum distance in centimeters at which a compact nearby marker may be shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ranges", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AwarenessRange = 800.0f;

	/** Maximum distance in centimeters at which this option may win camera focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ranges", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float FocusRange = 500.0f;

	/** Maximum authoritative execution distance in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Ranges", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float InteractionRange = 350.0f;

	/** Primary deterministic focus rank; larger values win before direction and distance are compared. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Focus")
	int32 InteractionPriority = 0;

	/** Whether this target may produce a compact marker while it is not focused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	bool bShowNearbyIndicator = true;

	/** Whether focus and authoritative execution require an unobstructed visibility trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Focus")
	bool bRequiresLineOfSight = true;

	/** Optional focused-prompt widget override. Empty uses GA_Interaction's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	TSoftClassPtr<UUserWidget> FocusWidgetClass;

	/** Optional nearby-marker widget override. Empty uses GA_Interaction's default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Prompt")
	TSoftClassPtr<UUserWidget> NearbyWidgetClass;

	void SanitizeRanges();
	bool IsSemanticallyEqual(const FRpgInteractionPromptDefinition& Other) const;
};
