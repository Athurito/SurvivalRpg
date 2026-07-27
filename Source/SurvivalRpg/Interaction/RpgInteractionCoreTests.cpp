// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptStateBoundaryTest,
	"SurvivalRpg.Interaction.Core.PromptStateBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptStateBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FInteractionOption Option;
	Option.Prompt.AwarenessRange = 800.0f;
	Option.Prompt.FocusRange = 500.0f;
	Option.Prompt.InteractionRange = 350.0f;

	TestEqual(
		TEXT("Awareness boundary remains visible"),
		UInteractionStatics::DeterminePromptState(Option, 800.0f, false, true, true),
		ERpgInteractionPromptState::Nearby);
	TestEqual(
		TEXT("Beyond awareness is hidden"),
		UInteractionStatics::DeterminePromptState(Option, 800.01f, false, true, true),
		ERpgInteractionPromptState::Hidden);
	TestEqual(
		TEXT("Interaction boundary is ready"),
		UInteractionStatics::DeterminePromptState(Option, 350.0f, true, true, true),
		ERpgInteractionPromptState::Ready);
	TestEqual(
		TEXT("Immediately beyond interaction range is focused out of range"),
		UInteractionStatics::DeterminePromptState(Option, 350.01f, true, true, true),
		ERpgInteractionPromptState::FocusedOutOfRange);

	Option.Availability = ERpgInteractionAvailability::Blocked;
	TestEqual(
		TEXT("Provider blocked option remains blocked at the interaction boundary"),
		UInteractionStatics::DeterminePromptState(Option, 350.0f, true, true, true),
		ERpgInteractionPromptState::Blocked);
	Option.Availability = ERpgInteractionAvailability::Available;
	TestEqual(
		TEXT("GAS activation failure is blocked"),
		UInteractionStatics::DeterminePromptState(Option, 100.0f, true, false, true),
		ERpgInteractionPromptState::Blocked);
	TestEqual(
		TEXT("Required line of sight failure is blocked"),
		UInteractionStatics::DeterminePromptState(Option, 100.0f, true, true, false),
		ERpgInteractionPromptState::Blocked);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionFocusOrderingTest,
	"SurvivalRpg.Interaction.Core.DeterministicFocusOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionFocusOrderingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FVector ViewOrigin = FVector::ZeroVector;
	const FVector ViewDirection = FVector::ForwardVector;

	FInteractionOption Current;
	Current.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	Current.TargetRef.WorldLocation = FVector(100.0f, 0.0f, 0.0f);
	Current.Prompt.FocusRange = 500.0f;

	FInteractionOption Candidate = Current;
	Candidate.Prompt.InteractionPriority = 1;
	Candidate.TargetRef.WorldLocation = FVector(300.0f, 200.0f, 0.0f);
	TestTrue(
		TEXT("Priority wins before aim and distance"),
		UInteractionStatics::IsBetterFocusCandidate(Candidate, Current, ViewOrigin, ViewDirection));

	Candidate = Current;
	Current.TargetRef.WorldLocation = FVector(100.0f, 50.0f, 0.0f);
	Candidate.TargetRef.WorldLocation = FVector(300.0f, 0.0f, 0.0f);
	TestTrue(
		TEXT("View alignment wins before normalized distance"),
		UInteractionStatics::IsBetterFocusCandidate(Candidate, Current, ViewOrigin, ViewDirection));

	Current.TargetRef.WorldLocation = FVector(300.0f, 0.0f, 0.0f);
	Current.Prompt.FocusRange = 500.0f;
	Candidate = Current;
	Candidate.TargetRef.WorldLocation = FVector(400.0f, 0.0f, 0.0f);
	Candidate.Prompt.FocusRange = 800.0f;
	TestTrue(
		TEXT("Lower normalized distance wins equal-priority equal-alignment ties"),
		UInteractionStatics::IsBetterFocusCandidate(Candidate, Current, ViewOrigin, ViewDirection));

	Current.TargetRef.WorldLocation = FVector(100.0f, 0.0f, 0.0f);
	Current.Prompt.FocusRange = 500.0f;
	Current.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	Candidate = Current;
	Candidate.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Collect;
	TestTrue(
		TEXT("Stable semantic key breaks otherwise identical ties"),
		UInteractionStatics::IsBetterFocusCandidate(Candidate, Current, ViewOrigin, ViewDirection));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionSemanticEqualityTest,
	"SurvivalRpg.Interaction.Core.SemanticEquality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionSemanticEqualityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FInteractionOption First;
	First.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	First.TargetRef.WorldLocation = FVector(100.0f, 10.0f, 5.0f);
	First.Prompt.ActionText = NSLOCTEXT("RpgInteractionTests", "Use", "Use");

	FInteractionOption Same = First;
	Same.TargetRef.WorldLocation.X += 0.05f;
	TestTrue(TEXT("Sub-millimeter presentation jitter is semantically unchanged"), First == Same);

	Same.Prompt.ActionText = NSLOCTEXT("RpgInteractionTests", "Open", "Open");
	TestTrue(TEXT("Prompt text is part of the semantic diff"), First != Same);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
