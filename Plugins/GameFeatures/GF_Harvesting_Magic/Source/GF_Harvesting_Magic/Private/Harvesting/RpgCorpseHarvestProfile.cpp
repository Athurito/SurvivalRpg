#include "Harvesting/RpgCorpseHarvestProfile.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCorpseHarvestProfile)

URpgCorpseHarvestProfile::URpgCorpseHarvestProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	CommitEventTag = RpgHarvestingMagicGameplayTags::GameplayEvent_Harvesting_Commit;
	ToolGameplayCue = RpgHarvestingMagicGameplayTags::GameplayCue_Harvesting_Skinning_Tool;
	CorpseCompletionTag = RpgHarvestingMagicGameplayTags::Rpg_Corpse_Completion_Harvest;
	InteractionPrompt.ActionText = NSLOCTEXT("RpgCorpseHarvest", "Action", "Skin");
	InteractionPrompt.TargetText = NSLOCTEXT("RpgCorpseHarvest", "Target", "Corpse");
	InteractionPrompt.AwarenessRange = 800.0f;
	InteractionPrompt.FocusRange = 500.0f;
	InteractionPrompt.InteractionRange = 350.0f;
	InteractionPrompt.InteractionPriority = 40;
	InteractionPrompt.bShowNearbyIndicator = true;
	InteractionPrompt.bRequiresLineOfSight = true;
	MissingToolReason = NSLOCTEXT("RpgCorpseHarvest", "MissingTool", "A skinning knife is required.");
	InsufficientSkillReason = NSLOCTEXT("RpgCorpseHarvest", "InsufficientSkill", "Your Skinning skill is too low.");
}

FPrimaryAssetId URpgCorpseHarvestProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("RpgCorpseHarvestProfile"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult URpgCorpseHarvestProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto MarkInvalid = [&Result, &Context](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	const FGameplayTag ToolRoot = FGameplayTag::RequestGameplayTag(TEXT("Tool.Harvesting"), false);
	if (!RequiredToolTag.IsValid() || !ToolRoot.IsValid() || !RequiredToolTag.MatchesTag(ToolRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidToolTag", "RequiredToolTag must be a registered Tool.Harvesting.* tag."));
	}
	const FGameplayTag HarvestEventRoot = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Harvesting"), false);
	if (!CommitEventTag.IsValid() || !HarvestEventRoot.IsValid() || !CommitEventTag.MatchesTag(HarvestEventRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidCommitTag", "CommitEventTag must be a registered GameplayEvent.Harvesting.* tag."));
	}
	const FGameplayTag GameplayCueRoot = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue"), false);
	if (!ToolGameplayCue.IsValid() || !GameplayCueRoot.IsValid() || !ToolGameplayCue.MatchesTag(GameplayCueRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidToolCue", "ToolGameplayCue must be a registered GameplayCue.* tag."));
	}
	const FGameplayTag CompletionRoot = FGameplayTag::RequestGameplayTag(TEXT("Rpg.Corpse.Completion"), false);
	if (!CorpseCompletionTag.IsValid() || !CompletionRoot.IsValid() || !CorpseCompletionTag.MatchesTag(CompletionRoot))
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidCompletionTag", "CorpseCompletionTag must be a registered Rpg.Corpse.Completion.* tag."));
	}
	if (!HarvestMontage)
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "MissingMontage", "A corpse harvest profile requires a montage."));
	}
	else if (CommitEventTag.IsValid())
	{
		const FString ExpectedNotifyName = CommitEventTag.ToString();
		const bool bHasCommitNotify = HarvestMontage->Notifies.ContainsByPredicate(
			[&ExpectedNotifyName](const FAnimNotifyEvent& NotifyEvent)
			{
				return NotifyEvent.Notify && NotifyEvent.Notify->GetNotifyName() == ExpectedNotifyName;
			});
		if (!bHasCommitNotify)
		{
			MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "MissingCommitNotify", "HarvestMontage must contain an RPG Gameplay Event notify for CommitEventTag."));
		}
	}
	if (HarvestMontage && MontageStartSection != NAME_None &&
		!HarvestMontage->IsValidSectionName(MontageStartSection))
	{
		MarkInvalid(NSLOCTEXT(
			"RpgCorpseHarvestProfile",
			"InvalidMontageSection",
			"MontageStartSection must name an existing section in HarvestMontage."));
	}
	if (!FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.0f)
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidMontageRate", "MontagePlayRate must be finite and greater than zero."));
	}
	if (!FMath::IsFinite(ReservationTimeoutSeconds) || ReservationTimeoutSeconds <= 0.0f)
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidReservationTimeout", "ReservationTimeoutSeconds must be finite and greater than zero."));
	}

	FRpgInteractionPromptDefinition SanitizedPrompt = InteractionPrompt;
	SanitizedPrompt.SanitizeRanges();
	if (SanitizedPrompt.InteractionRange <= 0.0f)
	{
		MarkInvalid(NSLOCTEXT("RpgCorpseHarvestProfile", "InvalidInteractionRange", "InteractionPrompt.InteractionRange must be greater than zero."));
	}
	return Result;
}
#endif
