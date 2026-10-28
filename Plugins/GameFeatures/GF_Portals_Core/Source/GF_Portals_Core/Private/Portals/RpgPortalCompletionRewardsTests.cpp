#include "Portals/RpgPortalCompletionRewards.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalMessages.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimComponent.h"
#include "SurvivalRpg/Progression/Rewards/RpgRewardClaimDefinition.h"

namespace RpgPortalCompletionRewardsTests
{
	class FScopedWorld
	{
	public:
		FScopedWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPortalCompletionRewardsTest,
	"SurvivalRpg.Portals.Rewards.EligibleCompletionRewardsEveryParticipantOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPortalCompletionRewardsTest::RunTest(const FString& Parameters)
{
	using namespace RpgPortalCompletionRewardsTests;
	FScopedWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* Closer =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	ARpgInventoryAutomationTestPlayerState* Partner =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	URpgRewardClaimComponent* CloserClaims = Closer ? Closer->GetRewardClaimComponent() : nullptr;
	URpgRewardClaimComponent* PartnerClaims = Partner ? Partner->GetRewardClaimComponent() : nullptr;
	if (!TestNotNull(TEXT("The closing player has reward claims"), CloserClaims) ||
		!TestNotNull(TEXT("The partner has reward claims"), PartnerClaims))
	{
		return false;
	}

	URpgRewardClaimDefinition* Reward = NewObject<URpgRewardClaimDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Reward->DisplayName = FText::FromString(TEXT("Automation portal reward"));
	FRpgRewardClaimOption& Option = Reward->Options.AddDefaulted_GetRef();
	Option.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	URpgPortalEncounterDefinition* Encounter =
		NewObject<URpgPortalEncounterDefinition>(GetTransientPackage(), TEXT("AutomationPortalEncounter"), RF_Transient);
	Encounter->CompletionReward = Reward;

	FRpgPortalCompletedMessage Message;
	Message.EncounterDefinition = Encounter;
	Message.Participants = {Closer, Partner, nullptr};
	Message.CompletionId = FGuid::NewGuid();
	Message.bRewardsEligible = false;
	TestEqual(TEXT("An ineligible completion grants nothing"), FRpgPortalCompletionRewards::GrantCompletionRewards(Message), 0);

	Message.bRewardsEligible = true;
	TestEqual(TEXT("An eligible completion rewards both participants"), FRpgPortalCompletionRewards::GrantCompletionRewards(Message), 2);
	TestEqual(TEXT("The same completion grants nothing again"), FRpgPortalCompletionRewards::GrantCompletionRewards(Message), 0);
	const FName FirstId = FRpgPortalCompletionRewards::MakeClaimId(Message);
	TestTrue(TEXT("The claim id names the encounter"), FirstId.ToString().StartsWith(TEXT("Portal/AutomationPortalEncounter/")));
	const FRpgRewardClaim* Claim = CloserClaims->FindPendingClaim(FirstId);
	TestTrue(
		TEXT("The closer's claim carries the reward and the portal source"),
		Claim && Claim->Reward == Reward && Claim->SourceTag == RpgPortalGameplayTags::Rpg_Reward_Source_Portal);
	TestNotNull(TEXT("The partner holds the same completion's claim"), PartnerClaims->FindPendingClaim(FirstId));

	Message.CompletionId = FGuid::NewGuid();
	TestEqual(TEXT("A later completion rewards both again"), FRpgPortalCompletionRewards::GrantCompletionRewards(Message), 2);
	TestEqual(TEXT("Each completion is its own claim"), CloserClaims->GetPendingClaims().Num(), 2);

	FRpgInventoryGridSize GridSize;
	GridSize.Width = 2;
	GridSize.Height = 1;
	Closer->GetInventoryManagerComponent()->SetDefaultGridSize(GridSize);
	TestEqual(TEXT("The closer claims the first reward"), CloserClaims->ClaimReward(FirstId, 0), ERpgRewardClaimResult::Claimed);
	TestEqual(
		TEXT("The reward is in the closer's inventory"),
		Closer->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass()),
		1);
	TestNotNull(TEXT("The partner's claim is untouched"), PartnerClaims->FindPendingClaim(FirstId));

	Encounter->CompletionReward = nullptr;
	Message.CompletionId = FGuid::NewGuid();
	TestEqual(TEXT("An encounter without a reward grants nothing"), FRpgPortalCompletionRewards::GrantCompletionRewards(Message), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
