#include "Portals/RpgPortalEncounterDefinition.h"

#include "Portals/RpgPortalStorageProgressionHook.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalEncounterDefinition)

#define LOCTEXT_NAMESPACE "RpgPortalEncounterDefinition"

URpgPortalEncounterDefinition::URpgPortalEncounterDefinition()
{
	EnterInteractionText = LOCTEXT("EnterPortal", "Enter Portal");
	EnterInteractionSubText = LOCTEXT("EnterPortalSubText", "Cross into the rift");
	ExitInteractionText = LOCTEXT("ExitPortal", "Exit Portal");
	ExitInteractionSubText = LOCTEXT("ExitPortalSubText", "Return to the overworld");
	CloseInteractionText = LOCTEXT("ClosePortal", "Close Portal");
	CloseInteractionSubText = LOCTEXT("ClosePortalSubText", "Stabilize the rift");
}

#if WITH_EDITOR
EDataValidationResult URpgPortalEncounterDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (FirstEligibleKnowledgeRewardTable.IsNull())
	{
		Context.AddError(LOCTEXT(
			"MissingFirstKnowledgeReward",
			"FirstEligibleKnowledgeRewardTable is required so the first eligible portal completion can unlock Rift containment."));
		return EDataValidationResult::Invalid;
	}

	const URpgLootTable* RewardTable =
		FirstEligibleKnowledgeRewardTable.LoadSynchronous();
	FString ValidationError;
	if (!FRpgPortalStorageProgressionHook::IsDeterministicGuaranteedRewardTable(
			RewardTable,
			ValidationError))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("FirstEligibleKnowledgeRewardTable is not deterministic and guaranteed: %s"),
			*ValidationError)));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
