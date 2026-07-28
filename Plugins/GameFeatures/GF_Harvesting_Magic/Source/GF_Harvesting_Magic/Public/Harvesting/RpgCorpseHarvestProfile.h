#pragma once

#include "Harvesting/RpgHarvestRewardProfile.h"
#include "SurvivalRpg/Interaction/InteractionTypes.h"

#include "RpgCorpseHarvestProfile.generated.h"

class UAnimMontage;

/** Static interaction, tool, animation, and reward rules for processing one actor-backed corpse. */
UCLASS(BlueprintType, Const)
class GF_HARVESTING_MAGIC_API URpgCorpseHarvestProfile final : public URpgHarvestRewardProfile
{
	GENERATED_BODY()

public:
	explicit URpgCorpseHarvestProfile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Tool category an owned inventory item must provide throughout the harvesting montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Tool", meta = (Categories = "Tool.Harvesting"))
	FGameplayTag RequiredToolTag;

	/** Server-played montage containing one RPG Gameplay Event notify for CommitEventTag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Animation")
	TObjectPtr<UAnimMontage> HarvestMontage;

	/** Positive server-authored montage playback rate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Animation", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Optional montage section used as the harvesting sequence entry point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Animation")
	FName MontageStartSection = NAME_None;

	/** Gameplay event emitted by the montage notify at the sole authoritative reward commit point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Animation", meta = (Categories = "GameplayEvent.Harvesting"))
	FGameplayTag CommitEventTag;

	/** Optional replicated cue that presents the selected knife/tool while the montage is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag ToolGameplayCue;

	/** Interaction prompt, ranges, visibility policy, and UI presentation for this corpse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Interaction")
	FRpgInteractionPromptDefinition InteractionPrompt;

	/** Explanation shown when the requesting player does not own a matching tool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Interaction")
	FText MissingToolReason;

	/** Explanation shown when the requesting player is below MinimumSkillLevel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Interaction")
	FText InsufficientSkillReason;

	/** Server reservation expiry in seconds; protects a corpse from permanent lock after disconnect or missing notify. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Reservation", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float ReservationTimeoutSeconds = 15.0f;

	/** Corpse-lifecycle requirement completed after reward delivery and the single XP award. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse Harvesting|Lifecycle", meta = (Categories = "Rpg.Corpse.Completion"))
	FGameplayTag CorpseCompletionTag;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
