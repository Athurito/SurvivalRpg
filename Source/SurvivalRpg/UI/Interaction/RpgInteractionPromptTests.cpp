#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInteractionPromptData.h"
#include "RpgInteractionPromptAutomationTestTypes.h"
#include "RpgInteractionReticleWidget.h"
#include "RpgInteractionPromptWidget.h"

#include "CommonInputModeTypes.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptDataSemanticDiffTest,
	"SurvivalRpg.UI.Interaction.PromptDataSemanticDiff",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptDataSemanticDiffTest::RunTest(const FString& Parameters)
{
	URpgInteractionPromptData* PromptData = NewObject<URpgInteractionPromptData>();
	USphereComponent* FirstCollision = NewObject<USphereComponent>();
	USphereComponent* AlternateCollision = NewObject<USphereComponent>();
	FInteractionOption Option;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	Option.Prompt.ActionText = FText::FromString(TEXT("Open"));
	Option.Prompt.TargetText = FText::FromString(TEXT("Ancient Door"));
	Option.Availability = ERpgInteractionAvailability::Available;
	Option.TargetRef.TargetComponent = FirstCollision;
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

	Option.TargetRef.TargetComponent = AlternateCollision;
	TestFalse(
		TEXT("An incidental collision-component change is presentation-silent"),
		PromptData->UpdateFromOption(Option, ERpgInteractionPromptState::Ready));
	TestEqual(TEXT("Collision-component jitter does not rebroadcast"), BroadcastCount, 1);
	TestEqual(
		TEXT("Silent collision changes still refresh the target context"),
		PromptData->TargetRef.TargetComponent.Get(),
		static_cast<UPrimitiveComponent*>(AlternateCollision));

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
	FRpgInteractionPromptRangeReentryTest,
	"SurvivalRpg.UI.Interaction.PromptRangeReentry",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptRangeReentryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	URpgInteractionPromptAutomationAbility* Ability =
		NewObject<URpgInteractionPromptAutomationAbility>();
	URpgIndicatorManagerComponent* IndicatorManager =
		NewObject<URpgIndicatorManagerComponent>();
	USphereComponent* TargetComponent = NewObject<USphereComponent>();

	FInteractionOption Option;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	Option.TargetRef.TargetComponent = TargetComponent;
	Option.TargetRef.WorldLocation = FVector(100.0, 0.0, 50.0);
	Option.Prompt.ActionText = FText::FromString(TEXT("Open"));
	Option.Prompt.TargetText = FText::FromString(TEXT("Storage"));
	Option.Prompt.FocusWidgetClass = URpgInteractionPromptWidget::StaticClass();
	Option.PromptState = ERpgInteractionPromptState::Ready;

	int32 AddedCount = 0;
	bool bWasConfiguredBeforeRegistration = false;
	IndicatorManager->OnIndicatorAdded.AddLambda(
		[&](UIndicatorDescriptor* Descriptor)
		{
			++AddedCount;
			bWasConfiguredBeforeRegistration = Descriptor &&
				!Descriptor->GetIndicatorClass().IsNull() &&
				Descriptor->GetSceneComponent() == TargetComponent &&
				Descriptor->GetDataObject() != nullptr;
		});

	Ability->ReconcileFocusForTesting(IndicatorManager, { Option });
	TestEqual(TEXT("The focus descriptor is registered once"), IndicatorManager->GetIndicators().Num(), 1);
	TestEqual(TEXT("The actor canvas receives one add event"), AddedCount, 1);
	TestTrue(TEXT("The descriptor is fully configured before registration"), bWasConfiguredBeforeRegistration);

	UIndicatorDescriptor* InitialDescriptor = IndicatorManager->GetIndicators()[0];
	Ability->ReconcileFocusForTesting(IndicatorManager, {});
	TestEqual(TEXT("Range exit retains the stable descriptor"), IndicatorManager->GetIndicators().Num(), 1);
	TestEqual(TEXT("Range exit does not recreate or re-register the widget"), AddedCount, 1);
	TestFalse(TEXT("Range exit hides the retained descriptor"), InitialDescriptor->GetIsVisible());
	const URpgInteractionPromptData* HiddenData =
		Cast<URpgInteractionPromptData>(InitialDescriptor->GetDataObject());
	TestNotNull(TEXT("The retained descriptor keeps its stable data object"), HiddenData);
	if (HiddenData)
	{
		TestEqual(TEXT("Range exit clears presentation state"), HiddenData->State, ERpgInteractionPromptState::Hidden);
	}

	Ability->ReconcileFocusForTesting(IndicatorManager, { Option });
	TestEqual(TEXT("Range re-entry still has exactly one descriptor"), IndicatorManager->GetIndicators().Num(), 1);
	TestEqual(TEXT("Range re-entry reuses the registered widget"), AddedCount, 1);
	TestEqual(TEXT("Range re-entry reuses the same descriptor"), IndicatorManager->GetIndicators()[0], InitialDescriptor);
	TestTrue(TEXT("Range re-entry makes the prompt visible again"), InitialDescriptor->GetIsVisible());
	const URpgInteractionPromptData* RestoredData =
		Cast<URpgInteractionPromptData>(InitialDescriptor->GetDataObject());
	if (RestoredData)
	{
		TestEqual(TEXT("Range re-entry restores ready state"), RestoredData->State, ERpgInteractionPromptState::Ready);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptProgressiveDisclosureTest,
	"SurvivalRpg.UI.Interaction.ProgressiveDisclosureReconciliation",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptProgressiveDisclosureTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	URpgInteractionPromptAutomationAbility* Ability =
		NewObject<URpgInteractionPromptAutomationAbility>();
	URpgIndicatorManagerComponent* IndicatorManager =
		NewObject<URpgIndicatorManagerComponent>();
	AActor* TargetActor = NewObject<AActor>();
	USceneComponent* RootComponent = NewObject<USceneComponent>(
		TargetActor,
		TEXT("Root"));
	TargetActor->AddInstanceComponent(RootComponent);
	TargetActor->SetRootComponent(RootComponent);
	URpgInteractionPromptAnchorComponent* PromptAnchor =
		NewObject<URpgInteractionPromptAnchorComponent>(
			TargetActor,
			TEXT("PromptAnchor"));
	TargetActor->AddInstanceComponent(PromptAnchor);
	PromptAnchor->SetupAttachment(RootComponent);
	USphereComponent* NearbyCollision = NewObject<USphereComponent>(
		TargetActor,
		TEXT("NearbyCollision"));
	TargetActor->AddInstanceComponent(NearbyCollision);
	NearbyCollision->SetupAttachment(RootComponent);
	USphereComponent* FocusCollision = NewObject<USphereComponent>(
		TargetActor,
		TEXT("FocusCollision"));
	TargetActor->AddInstanceComponent(FocusCollision);
	FocusCollision->SetupAttachment(RootComponent);

	FInteractionOption NearbyOption;
	NearbyOption.InteractionTag =
		RpgGameplayTags::Rpg_Interaction_Action_Generic;
	NearbyOption.TargetRef.TargetActor = TargetActor;
	NearbyOption.TargetRef.TargetComponent = NearbyCollision;
	NearbyOption.Prompt.ActionText = FText::FromString(TEXT("Open"));
	NearbyOption.Prompt.NearbyWidgetClass =
		URpgInteractionPromptWidget::StaticClass();
	NearbyOption.PromptState = ERpgInteractionPromptState::Nearby;

	FInteractionOption FocusOption = NearbyOption;
	FocusOption.TargetRef.TargetComponent = FocusCollision;
	FocusOption.Prompt.FocusWidgetClass =
		URpgInteractionPromptWidget::StaticClass();
	FocusOption.PromptState = ERpgInteractionPromptState::Ready;

	int32 AddedCount = 0;
	IndicatorManager->OnIndicatorAdded.AddLambda(
		[&AddedCount](UIndicatorDescriptor*)
		{
			++AddedCount;
		});
	const auto CountVisible = [IndicatorManager]()
	{
		int32 Count = 0;
		for (const UIndicatorDescriptor* Descriptor :
			IndicatorManager->GetIndicators())
		{
			Count += Descriptor && Descriptor->GetIsVisible() ? 1 : 0;
		}
		return Count;
	};

	Ability->ReconcileForTesting(
		IndicatorManager,
		{},
		{ NearbyOption });
	TestEqual(
		TEXT("Nearby discovery registers one compact descriptor"),
		IndicatorManager->GetIndicators().Num(),
		1);
	TestEqual(
		TEXT("Nearby discovery exposes exactly one marker"),
		CountVisible(),
		1);
	UIndicatorDescriptor* RetainedNearbyDescriptor =
		IndicatorManager->GetIndicators()[0];
	TestEqual(
		TEXT("The explicit prompt anchor owns the initial marker placement"),
		RetainedNearbyDescriptor->GetSceneComponent(),
		static_cast<USceneComponent*>(PromptAnchor));
	PromptAnchor->DestroyComponent();
	TestEqual(
		TEXT("Destroying only the cosmetic anchor re-resolves to actor bounds immediately"),
		RetainedNearbyDescriptor->GetSceneComponent(),
		RootComponent);

	FocusOption.PromptState =
		ERpgInteractionPromptState::FocusedOutOfRange;
	Ability->ReconcileForTesting(
		IndicatorManager,
		{ FocusOption },
		{});
	TestEqual(
		TEXT("Aimed out-of-range focus reserves a circle before the nearby scan"),
		IndicatorManager->GetIndicators().Num(),
		1);
	TestEqual(
		TEXT("Aimed out-of-range focus still exposes exactly one marker"),
		CountVisible(),
		1);
	TestEqual(
		TEXT("Focus reservation reuses the presentation descriptor"),
		IndicatorManager->GetIndicators()[0],
		RetainedNearbyDescriptor);

	FocusOption.PromptState = ERpgInteractionPromptState::Ready;
	Ability->ReconcileForTesting(
		IndicatorManager,
		{ FocusOption },
		{ NearbyOption });
	TestEqual(
		TEXT("Focus adds one retained full-prompt descriptor"),
		IndicatorManager->GetIndicators().Num(),
		2);
	TestEqual(
		TEXT("Different collision components never double-render the same option"),
		CountVisible(),
		1);
	TestFalse(
		TEXT("The matching nearby marker is cached but hidden during Ready"),
		RetainedNearbyDescriptor->GetIsVisible());
	const int32 AddedAfterFirstFocus = AddedCount;

	NearbyOption.InteractionTag =
		RpgGameplayTags::Rpg_Interaction_Action_Door_Open;
	FocusOption.InteractionTag =
		RpgGameplayTags::Rpg_Interaction_Action_Door_Close;
	Ability->ReconcileForTesting(
		IndicatorManager,
		{ FocusOption },
		{ NearbyOption });
	TestEqual(
		TEXT("A stale nearby action tag cannot remain over the focused prompt"),
		CountVisible(),
		1);
	TestFalse(
		TEXT("Visual-slot matching hides the stale-tag circle"),
		RetainedNearbyDescriptor->GetIsVisible());
	TestEqual(
		TEXT("A dynamic action-tag change does not add descriptors"),
		AddedCount,
		AddedAfterFirstFocus);

	FocusOption.PromptState =
		ERpgInteractionPromptState::FocusedOutOfRange;
	Ability->ReconcileForTesting(
		IndicatorManager,
		{ FocusOption },
		{ NearbyOption });
	TestEqual(
		TEXT("Out-of-range focus keeps exactly one visible circle"),
		CountVisible(),
		1);
	TestTrue(
		TEXT("Out-of-range focus restores the cached nearby circle"),
		RetainedNearbyDescriptor->GetIsVisible());
	TestEqual(
		TEXT("Range transition does not register more descriptors"),
		AddedCount,
		AddedAfterFirstFocus);

	FocusOption.PromptState = ERpgInteractionPromptState::Blocked;
	Ability->ReconcileForTesting(
		IndicatorManager,
		{ FocusOption },
		{ NearbyOption });
	TestEqual(
		TEXT("Blocked in interaction range owns the single expanded prompt"),
		CountVisible(),
		1);
	TestFalse(
		TEXT("Blocked focus hides its cached circle"),
		RetainedNearbyDescriptor->GetIsVisible());
	TestEqual(
		TEXT("Blocked transition reuses both descriptors"),
		AddedCount,
		AddedAfterFirstFocus);

	Ability->ReconcileForTesting(IndicatorManager, {}, {});
	TestEqual(
		TEXT("Leaving awareness removes the nearby descriptor"),
		IndicatorManager->GetIndicators().Num(),
		1);
	TestFalse(
		TEXT("The retained focus descriptor is hidden without a target"),
		IndicatorManager->GetIndicators()[0]->GetIsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptInputResolutionTest,
	"SurvivalRpg.UI.Interaction.InteractInputResolution",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptInputResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
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
	FRpgInteractionPromptPresentationStateTest,
	"SurvivalRpg.UI.Interaction.PromptPresentationStates",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptPresentationStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto HiddenRules = URpgInteractionPromptWidget::ResolvePresentationRules(
		ERpgInteractionPromptState::Hidden);
	TestFalse(TEXT("Hidden collapses the entire prompt"), HiddenRules.bShowWidget);
	TestFalse(TEXT("Hidden does not retain a compact marker"), HiddenRules.bShowNearbyMarker);

	const auto NearbyRules = URpgInteractionPromptWidget::ResolvePresentationRules(
		ERpgInteractionPromptState::Nearby);
	TestTrue(TEXT("Nearby retains the marker widget"), NearbyRules.bShowWidget);
	TestFalse(TEXT("Nearby hides action text"), NearbyRules.bShowActionText);
	TestFalse(TEXT("Nearby hides the input glyph"), NearbyRules.bShowInputAction);
	TestFalse(TEXT("Nearby hides prompt-specific icons"), NearbyRules.bShowPromptIcon);
	TestFalse(TEXT("Nearby hides blocked text"), NearbyRules.bShowBlockedReason);
	TestTrue(TEXT("Nearby shows only its compact marker"), NearbyRules.bShowNearbyMarker);

	const auto OutOfRangeRules = URpgInteractionPromptWidget::ResolvePresentationRules(
		ERpgInteractionPromptState::FocusedOutOfRange);
	TestTrue(TEXT("Out-of-range focus retains the marker widget"), OutOfRangeRules.bShowWidget);
	TestFalse(TEXT("Out-of-range focus hides action text"), OutOfRangeRules.bShowActionText);
	TestFalse(TEXT("Out-of-range focus hides the input glyph"), OutOfRangeRules.bShowInputAction);
	TestFalse(TEXT("Out-of-range focus hides blocked text"), OutOfRangeRules.bShowBlockedReason);
	TestTrue(TEXT("Out-of-range focus falls back to the compact marker"), OutOfRangeRules.bShowNearbyMarker);

	const auto ReadyRules = URpgInteractionPromptWidget::ResolvePresentationRules(
		ERpgInteractionPromptState::Ready);
	TestTrue(TEXT("Ready shows the prompt"), ReadyRules.bShowWidget);
	TestTrue(TEXT("Ready shows action text"), ReadyRules.bShowActionText);
	TestTrue(TEXT("Ready shows the input glyph"), ReadyRules.bShowInputAction);
	TestTrue(TEXT("Ready may show its semantic icon"), ReadyRules.bShowPromptIcon);
	TestFalse(TEXT("Ready hides blocked text"), ReadyRules.bShowBlockedReason);
	TestFalse(TEXT("Ready replaces the compact marker"), ReadyRules.bShowNearbyMarker);

	const auto BlockedRules = URpgInteractionPromptWidget::ResolvePresentationRules(
		ERpgInteractionPromptState::Blocked);
	TestTrue(TEXT("Blocked shows the prompt"), BlockedRules.bShowWidget);
	TestFalse(TEXT("Blocked hides action text"), BlockedRules.bShowActionText);
	TestFalse(TEXT("Blocked hides the input glyph"), BlockedRules.bShowInputAction);
	TestFalse(TEXT("Blocked hides the semantic icon"), BlockedRules.bShowPromptIcon);
	TestTrue(TEXT("Blocked shows its reason"), BlockedRules.bShowBlockedReason);
	TestFalse(TEXT("Blocked replaces the compact marker"), BlockedRules.bShowNearbyMarker);

	URpgInteractionPromptData* PromptData = NewObject<URpgInteractionPromptData>();
	TestFalse(
		TEXT("Blocked prompts without a provider reason receive a localized fallback"),
		URpgInteractionPromptWidget::ResolveBlockedReasonText(
			PromptData,
			ERpgInteractionPromptState::Blocked).IsEmpty());

	const FText ProviderReason = FText::FromString(TEXT("Requires a key"));
	PromptData->BlockedReason = ProviderReason;
	TestTrue(
		TEXT("Provider-authored blocked reasons take precedence"),
		URpgInteractionPromptWidget::ResolveBlockedReasonText(
			PromptData,
			ERpgInteractionPromptState::Blocked).IdenticalTo(ProviderReason));
	TestTrue(
		TEXT("Non-blocked states never expose stale blocked text"),
		URpgInteractionPromptWidget::ResolveBlockedReasonText(
			PromptData,
			ERpgInteractionPromptState::Ready).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionReticleInputModeTest,
	"SurvivalRpg.UI.Interaction.ReticleInputMode",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionReticleInputModeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(
		TEXT("The reticle is visible during gameplay input"),
		URpgInteractionReticleWidget::ShouldShowForInputMode(ECommonInputMode::Game));
	TestFalse(
		TEXT("The reticle is hidden while menus own input"),
		URpgInteractionReticleWidget::ShouldShowForInputMode(ECommonInputMode::Menu));
	TestFalse(
		TEXT("The reticle stays hidden for mixed modal input"),
		URpgInteractionReticleWidget::ShouldShowForInputMode(ECommonInputMode::All));
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
