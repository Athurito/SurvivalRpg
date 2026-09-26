// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

class FAutomationTestBase;
struct FNoDiscardAsserter;
class FTestCommandBuilder;

namespace RpgGaspMoverTraversalTests
{
enum class EAction : uint8 { Mantle, Vault, Hurdle };
enum class EScenario : uint8 { Success, Jump, HeldRetry, BlockedExit, Cancel, Death, ColliderLoss, LateJoin, CorrectDuringWarp, CorrectAfterWarp, ReplaceActiveAndReplay, ReplacePendingAndReplay, NaturalEnd, ResumeStandingInput, SupportLoss, AutoBlendNaturalEnd, CancelDuringAutoBlend, ReplayWhileReplacedTraversalBuffered };
enum class EGait : uint8 { Stand, Walk, Run };
}

/** Shared saved-map input, authority, lifecycle and real Fixed rollback fixture for Mover traversal. */
class FRpgGaspMoverTraversalTestFixture
{
public:
	using EAction = RpgGaspMoverTraversalTests::EAction;
	using EScenario = RpgGaspMoverTraversalTests::EScenario;
	using EGait = RpgGaspMoverTraversalTests::EGait;
	FRpgGaspMoverTraversalTestFixture(FAutomationTestBase* Runner, FNoDiscardAsserter& Assert, FTestCommandBuilder& Builder);
	~FRpgGaspMoverTraversalTestFixture();
	/** Selects an optional PIE Experience; an empty ID uses the saved map, and cleanup restores the prior override. */
	void Initialize(const FPrimaryAssetId& ExperienceOverride = FPrimaryAssetId());
	void Cleanup();
	void Queue(EGait Gait, EScenario Scenario = EScenario::Success, bool bHost = false, float Yaw = 0.0f,
		EAction Action = EAction::Mantle, int32 Lane = 0);

private:
	struct FState;
	TUniquePtr<FState> State;
};
