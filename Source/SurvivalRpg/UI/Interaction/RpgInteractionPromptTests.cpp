#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInteractionPromptData.h"
#include "RpgInteractionPromptWidget.h"

#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptDataSemanticDiffTest,
	"SurvivalRpg.UI.Interaction.PromptDataSemanticDiff",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptDataSemanticDiffTest::RunTest(const FString& Parameters)
{
	URpgInteractionPromptData* PromptData = NewObject<URpgInteractionPromptData>();
	FInteractionOption Option;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	Option.Prompt.ActionText = FText::FromString(TEXT("Open"));
	Option.Prompt.TargetText = FText::FromString(TEXT("Ancient Door"));
	Option.Availability = ERpgInteractionAvailability::Available;
	Option.TargetRef.InstanceIndex = 7;
	Option.TargetRef.WorldLocation = FVector(100.0, 200.0, 300.0);

	int32 BroadcastCount = 0;
	PromptData->OnPromptChangedNative().AddLambda(
		[&BroadcastCount](URpgInteractionPromptData*)
		{
			++BroadcastCount;
		});

	TestTrue(
		TEXT("Initial option update changes stable data"),
		PromptData->UpdateFromOption(Option, ERpgInteractionPromptState::Ready));
	TestEqual(TEXT("Initial update broadcasts once"), BroadcastCount, 1);
	TestEqual(TEXT("Action text is copied"), PromptData->ActionText.ToString(), FString(TEXT("Open")));
	TestEqual(TEXT("Instance context is retained"), PromptData->TargetRef.InstanceIndex, 7);

	TestFalse(
		TEXT("Semantically identical update is silent"),
		PromptData->UpdateFromOption(Option, ERpgInteractionPromptState::Ready));
	TestEqual(TEXT("Identical update does not rebroadcast"), BroadcastCount, 1);

	Option.TargetRef.WorldLocation += FVector(10.0, 0.0, 0.0);
	TestFalse(
		TEXT("A moving target updates spatial context without invalidating presentation"),
		PromptData->UpdateFromOption(Option, ERpgInteractionPromptState::Ready));
	TestEqual(TEXT("Spatial-only update does not rebroadcast"), BroadcastCount, 1);
	TestEqual(
		TEXT("Spatial-only update still retains the current position"),
		PromptData->TargetRef.WorldLocation,
		Option.TargetRef.WorldLocation);

	TestTrue(
		TEXT("State-only update is semantic"),
		PromptData->UpdateFromOption(Option, ERpgInteractionPromptState::FocusedOutOfRange));
	TestEqual(TEXT("State update broadcasts once"), BroadcastCount, 2);

	PromptData->Clear();
	PromptData->Clear();
	TestEqual(TEXT("Only the first clear broadcasts"), BroadcastCount, 3);
	TestEqual(TEXT("Clear hides the presentation"), PromptData->State, ERpgInteractionPromptState::Hidden);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptInputResolutionTest,
	"SurvivalRpg.UI.Interaction.InteractInputResolution",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptInputResolutionTest::RunTest(const FString& Parameters)
{
	URpgPawnData* PawnData = NewObject<URpgPawnData>();
	URpgInputConfig* InputConfig = NewObject<URpgInputConfig>();
	UInputAction* InteractAction = NewObject<UInputAction>();
	PawnData->InputConfig = InputConfig;

	FRpgInputAction& Mapping = InputConfig->AbilityInputActions.AddDefaulted_GetRef();
	Mapping.InputAction = InteractAction;
	Mapping.InputTag = RpgGameplayTags::InputTag_Ability_Interact;

	TestEqual(
		TEXT("Prompt resolves IA_Interact through PawnData semantic config"),
		URpgInteractionPromptWidget::ResolveInteractionInputAction(PawnData),
		static_cast<const UInputAction*>(InteractAction));
	TestNull(
		TEXT("Missing PawnData resolves no input action"),
		URpgInteractionPromptWidget::ResolveInteractionInputAction(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgIndicatorAbsoluteWorldPositionTest,
	"SurvivalRpg.UI.Indicator.AbsoluteWorldPositionOverride",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgIndicatorAbsoluteWorldPositionTest::RunTest(const FString& Parameters)
{
	UIndicatorDescriptor* Descriptor = NewObject<UIndicatorDescriptor>();
	const FVector InstancePosition(125.0, -250.0, 375.0);

	TestFalse(TEXT("Descriptor starts component-relative"), Descriptor->HasWorldPositionOverride());
	Descriptor->SetWorldPositionOverride(InstancePosition);
	TestTrue(TEXT("Absolute override is enabled"), Descriptor->HasWorldPositionOverride());
	TestEqual(TEXT("Absolute instance position is retained"), Descriptor->GetWorldPositionOverride(), InstancePosition);

	Descriptor->ClearWorldPositionOverride();
	TestFalse(TEXT("Override can be cleared for descriptor reuse"), Descriptor->HasWorldPositionOverride());
	return true;
}

#endif
