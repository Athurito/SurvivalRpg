#include "Portals/RpgPortalCompletionRewards.h"

#include "GameFramework/PlayerState.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalMessages.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimComponent.h"

int32 FRpgPortalCompletionRewards::GrantCompletionRewards(const FRpgPortalCompletedMessage& Message)
{
	const URpgPortalEncounterDefinition* Encounter = Message.EncounterDefinition;
	if (!Message.bRewardsEligible || !Encounter || !Encounter->CompletionReward || !Message.CompletionId.IsValid())
	{
		return 0;
	}

	const FName ClaimId = MakeClaimId(Message);
	int32 GrantedCount = 0;
	for (APlayerState* Participant : Message.Participants)
	{
		URpgRewardClaimComponent* Claims = URpgRewardClaimComponent::FindForActor(Participant);
		if (Claims && Claims->GrantClaim(ClaimId, Encounter->CompletionReward, RpgPortalGameplayTags::Rpg_Reward_Source_Portal, 0))
		{
			++GrantedCount;
		}
	}
	return GrantedCount;
}

FName FRpgPortalCompletionRewards::MakeClaimId(const FRpgPortalCompletedMessage& Message)
{
	return FName(*FString::Printf(
		TEXT("Portal/%s/%s"),
		*GetNameSafe(Message.EncounterDefinition.Get()),
		*Message.CompletionId.ToString(EGuidFormats::Digits)));
}
