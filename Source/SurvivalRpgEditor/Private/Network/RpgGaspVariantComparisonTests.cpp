// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspMoverTraversalTestFixture.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Character/RpgMoverRagdollComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspVariantComparisonTests
{
	constexpr TCHAR ExperienceClassPath[] = TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspMoverRagdollExperience.RpgGaspMoverRagdollExperience_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/Ragdoll/RPG/DA_PawnData_GaspMoverRagdoll.DA_PawnData_GaspMoverRagdoll");
	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMoverRagdollExperience"));
	}
}

/** The Ragdoll child must retain the normal Mover traversal contract on the same saved obstacle course. */
NETWORK_TEST_CLASS(GaspVariantComparisonPIE, "SurvivalRpg.GASP.VariantComparison")
{
	using EAction = RpgGaspMoverTraversalTests::EAction;
	FRpgGaspMoverTraversalTestFixture Fixture{TestRunner, Assert, TestCommandBuilder};
	BEFORE_EACH() { Fixture.Initialize(RpgGaspVariantComparisonTests::ExperienceId()); }
	AFTER_EACH() { Fixture.Cleanup(); }

	void VerifyRagdollComposition()
	{
		using namespace RpgGaspVariantComparisonTests;
		int32 Worlds = 0, AuthorityPawns = 0, AutonomousPawns = 0, SimulatedPawns = 0;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !IsValid(World) || World->bIsTearingDown || World->IsBeingCleanedUp()) continue;
			++Worlds;
			const AGameStateBase* GameState = World->GetGameState();
			const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
			if (!TestRunner->TestTrue(TEXT("Every comparison world has a loaded Experience"), Manager && Manager->IsExperienceLoaded())) continue;
			const URpgExperienceDefinition* Experience = Manager->GetCurrentExperienceChecked();
			TestRunner->TestEqual(TEXT("Every role actually loaded the Ragdoll Experience"), Experience->GetClass()->GetPathName(), FString(ExperienceClassPath));
			const URpgPawnData* ExpectedData = Experience->DefaultPawnData;
			if (!TestRunner->TestNotNull(TEXT("Ragdoll Experience supplies PawnData"), ExpectedData)) continue;
			TestRunner->TestEqual(TEXT("Every role selects the dedicated Ragdoll PawnData"), ExpectedData->GetPathName(), FString(PawnDataPath));
			for (const APlayerState* Player : GameState->PlayerArray)
			{
				const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
				if (!TestRunner->TestNotNull(TEXT("Every comparison player has its composed pawn"), Pawn)) continue;
				const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
				const URpgPawnData* ActualData = Extension ? Extension->GetPawnData<URpgPawnData>() : nullptr;
				TestRunner->TestTrue(TEXT("Each pawn uses the selected Experience PawnData"), ActualData == ExpectedData);
				TestRunner->TestTrue(TEXT("Each pawn is the exact class selected by Ragdoll PawnData"), Pawn->GetClass() == ExpectedData->PawnClass);
				const URpgMoverRagdollComponent* Ragdoll = Pawn->FindComponentByClass<URpgMoverRagdollComponent>();
				TestRunner->TestTrue(TEXT("Every role has the initialized Ragdoll presentation component"), Ragdoll && Ragdoll->IsRagdollPresentationReady());
				TestRunner->TestTrue(TEXT("Normal traversal leaves the optional living-ragdoll lifecycle inactive"), Ragdoll && Ragdoll->GetRagdollPhase() == ERpgMoverRagdollPhase::Inactive);
				if (Pawn->GetLocalRole() == ROLE_Authority) ++AuthorityPawns;
				else if (Pawn->GetLocalRole() == ROLE_AutonomousProxy) ++AutonomousPawns;
				else if (Pawn->GetLocalRole() == ROLE_SimulatedProxy) ++SimulatedPawns;
				UE_LOG(LogTemp, Display, TEXT("RpgGaspVariantComparison world=%s role=%d player=%d pawn=%s experience=%s pawnData=%s ragdollReady=%d"),
					*World->GetPathName(), static_cast<int32>(Pawn->GetLocalRole()), Player->GetPlayerId(), *Pawn->GetClass()->GetPathName(),
					*Experience->GetClass()->GetPathName(), *GetPathNameSafe(ActualData), Ragdoll && Ragdoll->IsRagdollPresentationReady());
			}
		}
		TestRunner->TestEqual(TEXT("Comparison retained listen server, remote owner and observer worlds"), Worlds, 3);
		TestRunner->TestTrue(TEXT("Ragdoll composition was checked on authority"), AuthorityPawns > 0);
		TestRunner->TestTrue(TEXT("Ragdoll composition was checked on a remote owner"), AutonomousPawns > 0);
		TestRunner->TestTrue(TEXT("Ragdoll composition was checked on a simulated observer"), SimulatedPawns > 0);
	}

	void Queue(EAction Action)
	{
		Fixture.Queue(RpgGaspMoverTraversalTests::EGait::Run, RpgGaspMoverTraversalTests::EScenario::Success, false, 0.0f, Action);
		TestCommandBuilder.Then(TEXT("Confirm actual Ragdoll composition on every traversal role"), [this]() { VerifyRagdollComposition(); });
	}
	TEST_METHOD(RagdollVariantRunningMantlePreservesMoverTraversal) { Queue(EAction::Mantle); }
	TEST_METHOD(RagdollVariantRunningVaultPreservesMoverTraversal) { Queue(EAction::Vault); }
	TEST_METHOD(RagdollVariantRunningHurdlePreservesMoverTraversal) { Queue(EAction::Hurdle); }
};

#endif
#endif
