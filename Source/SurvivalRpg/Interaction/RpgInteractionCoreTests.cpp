// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractableDoorComponent.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/Interaction/InteractableComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace RpgInteractionCoreTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
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
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	AActor* SpawnTestActor(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			AActor::StaticClass(),
			TEXT("InteractionCoreTestActor"));
		SpawnParameters.ObjectFlags = RF_Transient;
		AActor* Actor = World->SpawnActor<AActor>(SpawnParameters);
		if (!Actor)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(
			Actor,
			TEXT("Root"),
			RF_Transient);
		Actor->AddInstanceComponent(Root);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		return Actor;
	}

	template <typename ComponentType>
	ComponentType* AddTestComponent(AActor* Owner, const FName ComponentName)
	{
		if (!Owner)
		{
			return nullptr;
		}

		ComponentType* Component = NewObject<ComponentType>(
			Owner,
			ComponentName,
			RF_Transient);
		Owner->AddInstanceComponent(Component);
		if constexpr (TIsDerivedFrom<ComponentType, USceneComponent>::IsDerived)
		{
			Component->SetupAttachment(Owner->GetRootComponent());
		}
		Component->RegisterComponent();
		return Component;
	}

	TScriptInterface<IInteractableTarget> MakeTargetInterface(
		URpgInteractableDoorComponent* Component)
	{
		TScriptInterface<IInteractableTarget> Target;
		Target.SetObject(Component);
		Target.SetInterface(Component);
		return Target;
	}
}

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

	Same = First;
	Same.Prompt.PromptAnchorId = TEXT("Alternate");
	TestTrue(TEXT("Prompt anchor id is part of the semantic diff"), First != Same);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPresentationIdentityTest,
	"SurvivalRpg.Interaction.Core.PresentationIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPresentationIdentityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionCoreTests;

	FScopedTestWorld TestWorld;
	AActor* TargetActor = SpawnTestActor(TestWorld.GetWorld());
	if (!TestNotNull(TEXT("Presentation identity target actor exists"), TargetActor))
	{
		return false;
	}

	URpgInteractableDoorComponent* ProviderA = AddTestComponent<URpgInteractableDoorComponent>(
		TargetActor,
		TEXT("ProviderA"));
	URpgInteractableDoorComponent* ProviderB = AddTestComponent<URpgInteractableDoorComponent>(
		TargetActor,
		TEXT("ProviderB"));
	USphereComponent* CollisionA = AddTestComponent<USphereComponent>(TargetActor, TEXT("CollisionA"));
	UBoxComponent* CollisionB = AddTestComponent<UBoxComponent>(TargetActor, TEXT("CollisionB"));
	if (!TestNotNull(TEXT("First provider exists"), ProviderA) ||
		!TestNotNull(TEXT("Second provider exists"), ProviderB) ||
		!TestNotNull(TEXT("First collision component exists"), CollisionA) ||
		!TestNotNull(TEXT("Second collision component exists"), CollisionB))
	{
		return false;
	}

	FInteractionOption First;
	First.InteractableTarget = MakeTargetInterface(ProviderA);
	First.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Generic;
	First.TargetRef.TargetActor = TargetActor;
	First.TargetRef.TargetComponent = CollisionA;

	FInteractionOption SameProviderDifferentCollision = First;
	SameProviderDifferentCollision.TargetRef.TargetComponent = CollisionB;
	TestTrue(
		TEXT("Gameplay identity preserves the concrete collision component"),
		UInteractionStatics::MakeStableOptionKey(First) !=
			UInteractionStatics::MakeStableOptionKey(SameProviderDifferentCollision));
	TestEqual(
		TEXT("Presentation identity ignores incidental collision components"),
		UInteractionStatics::MakePresentationOptionKey(First),
		UInteractionStatics::MakePresentationOptionKey(SameProviderDifferentCollision));
	TestEqual(
		TEXT("Incidental collision components share one projected slot"),
		UInteractionStatics::MakePresentationSlotKey(First),
		UInteractionStatics::MakePresentationSlotKey(SameProviderDifferentCollision));

	FInteractionOption DifferentProvider = First;
	DifferentProvider.InteractableTarget = MakeTargetInterface(ProviderB);
	TestTrue(
		TEXT("Different component providers keep separate presentation identities"),
		UInteractionStatics::MakePresentationOptionKey(First) !=
			UInteractionStatics::MakePresentationOptionKey(DifferentProvider));
	TestTrue(
		TEXT("Different component providers retain separate visual slots"),
		UInteractionStatics::MakePresentationSlotKey(First) !=
			UInteractionStatics::MakePresentationSlotKey(DifferentProvider));

	FInteractionOption DifferentInstance = First;
	DifferentInstance.TargetRef.InstanceIndex = 7;
	TestTrue(
		TEXT("Instanced mesh items keep separate presentation identities"),
		UInteractionStatics::MakePresentationOptionKey(First) !=
			UInteractionStatics::MakePresentationOptionKey(DifferentInstance));

	FInteractionOption DifferentAction = First;
	DifferentAction.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_Collect;
	TestTrue(
		TEXT("Different interaction actions keep separate presentation identities"),
		UInteractionStatics::MakePresentationOptionKey(First) !=
			UInteractionStatics::MakePresentationOptionKey(DifferentAction));
	TestEqual(
		TEXT("Dynamic actions at the same projection point share one visual slot"),
		UInteractionStatics::MakePresentationSlotKey(First),
		UInteractionStatics::MakePresentationSlotKey(DifferentAction));

	UInstancedStaticMeshComponent* Instances =
		AddTestComponent<UInstancedStaticMeshComponent>(
			TargetActor,
			TEXT("Instances"));
	if (!TestNotNull(TEXT("Instanced component exists"), Instances))
	{
		return false;
	}
	FInteractionOption FirstInstance = First;
	FirstInstance.TargetRef.TargetComponent = Instances;
	FirstInstance.TargetRef.InstanceIndex = 2;
	FInteractionOption SecondInstance = FirstInstance;
	SecondInstance.TargetRef.InstanceIndex = 3;
	TestTrue(
		TEXT("Different ISM indices retain distinct visual slots"),
		UInteractionStatics::MakePresentationSlotKey(FirstInstance) !=
			UInteractionStatics::MakePresentationSlotKey(SecondInstance));

	FInteractionQuery NonInstancedQuery;
	NonInstancedQuery.CandidateHit = FHitResult(
		TargetActor,
		CollisionA,
		FVector::ZeroVector,
		FVector::UpVector);
	NonInstancedQuery.CandidateHit.Item = 17;
	FInteractionOption NonInstancedOption;
	UInteractionStatics::NormalizeInteractionOption(
		NonInstancedQuery,
		MakeTargetInterface(ProviderA),
		NonInstancedOption);
	TestEqual(
		TEXT("Non-ISM hit items never become instance identities"),
		NonInstancedOption.TargetRef.InstanceIndex,
		INDEX_NONE);

	FInteractionQuery InstancedQuery = NonInstancedQuery;
	InstancedQuery.CandidateHit.Component = Instances;
	InstancedQuery.CandidateHit.Item = 4;
	FInteractionOption InstancedOption;
	UInteractionStatics::NormalizeInteractionOption(
		InstancedQuery,
		MakeTargetInterface(ProviderA),
		InstancedOption);
	TestEqual(
		TEXT("ISM hit items remain stable instance identities"),
		InstancedOption.TargetRef.InstanceIndex,
		4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGenericInteractableComponentOptionTest,
	"SurvivalRpg.Interaction.Core.GenericComponentEmitsOption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgGenericInteractableComponentOptionTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionCoreTests;

	FScopedTestWorld TestWorld;
	AActor* TargetActor = SpawnTestActor(TestWorld.GetWorld());
	UInteractableComponent* Interactable =
		AddTestComponent<UInteractableComponent>(
			TargetActor,
			TEXT("GenericInteractable"));
	if (!TestNotNull(TEXT("Generic interactable component exists"), Interactable))
	{
		return false;
	}

	TScriptInterface<IInteractableTarget> Target;
	Target.SetObject(Interactable);
	Target.SetInterface(Interactable);
	TArray<FInteractionOption> Options;
	FInteractionOptionBuilder Builder(Target, Options);
	FInteractionQuery Query;
	Interactable->GatherInteractionOptions(Query, Builder);
	TestEqual(TEXT("Generic component emits its configured option"), Options.Num(), 1);
	if (!Options.IsEmpty())
	{
		TestEqual(
			TEXT("Generic component stamps its owning actor"),
			Options[0].TargetRef.TargetActor.Get(),
			TargetActor);
		TestEqual(
			TEXT("Builder preserves the component provider"),
			Options[0].InteractableTarget.GetObject(),
			static_cast<UObject*>(Interactable));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgReviveInteractionCollisionProfileTest,
	"SurvivalRpg.Interaction.Core.ReviveDiscoveryCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgReviveInteractionCollisionProfileTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	FCollisionResponseTemplate PawnCapsuleProfile;
	if (!TestTrue(
		TEXT("RpgPawnCapsule collision profile exists"),
		UCollisionProfile::Get()->GetProfileTemplate(
			TEXT("RpgPawnCapsule"),
			PawnCapsuleProfile)))
	{
		return false;
	}
	TestEqual(
		TEXT("Pawn capsules participate in revive interaction discovery"),
		PawnCapsuleProfile.ResponseToChannels.GetResponse(
			Rpg_TraceChannel_Interaction),
		ECR_Overlap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionPromptAnchorResolutionTest,
	"SurvivalRpg.Interaction.Core.PromptAnchorResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInteractionPromptAnchorResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionCoreTests;

	FScopedTestWorld TestWorld;
	AActor* TargetActor = SpawnTestActor(TestWorld.GetWorld());
	if (!TestNotNull(TEXT("Prompt anchor target actor exists"), TargetActor))
	{
		return false;
	}

	URpgInteractionPromptAnchorComponent* LastDefault =
		AddTestComponent<URpgInteractionPromptAnchorComponent>(TargetActor, TEXT("Z_DefaultAnchor"));
	URpgInteractionPromptAnchorComponent* FirstDefault =
		AddTestComponent<URpgInteractionPromptAnchorComponent>(TargetActor, TEXT("A_DefaultAnchor"));
	URpgInteractionPromptAnchorComponent* Alternate =
		AddTestComponent<URpgInteractionPromptAnchorComponent>(TargetActor, TEXT("B_AlternateAnchor"));
	if (!TestNotNull(TEXT("Last default anchor exists"), LastDefault) ||
		!TestNotNull(TEXT("First default anchor exists"), FirstDefault) ||
		!TestNotNull(TEXT("Alternate anchor exists"), Alternate))
	{
		return false;
	}
	Alternate->AnchorId = TEXT("Alternate");

	TestEqual(TEXT("Prompt anchors default to the Default id"), FirstDefault->AnchorId, FName(TEXT("Default")));
	TestFalse(TEXT("Prompt anchors never tick"), FirstDefault->PrimaryComponentTick.bCanEverTick);
	TestEqual(
		TEXT("Prompt anchors have no collision"),
		FirstDefault->GetCollisionEnabled(),
		ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Prompt anchors are not replicated"), FirstDefault->GetIsReplicated());
	TestTrue(TEXT("Prompt anchors are hidden during gameplay"), FirstDefault->bHiddenInGame);

	FInteractionOption Option;
	Option.TargetRef.TargetActor = TargetActor;
	TestEqual(
		TEXT("Duplicate ids resolve deterministically by component name"),
		UInteractionStatics::FindPromptAnchorComponent(Option),
		FirstDefault);

	Option.Prompt.PromptAnchorId = TEXT("Alternate");
	TestEqual(
		TEXT("An explicit prompt anchor id resolves its matching component"),
		UInteractionStatics::FindPromptAnchorComponent(Option),
		Alternate);

	Option.Prompt.PromptAnchorId = TEXT("Missing");
	TestNull(
		TEXT("A missing prompt anchor id leaves presentation fallback to the caller"),
		UInteractionStatics::FindPromptAnchorComponent(Option));

#if WITH_EDITOR
	FDataValidationContext DuplicateContext;
	TestEqual(
		TEXT("Duplicate ids fail component data validation"),
		FirstDefault->IsDataValid(DuplicateContext),
		EDataValidationResult::Invalid);
	TestTrue(
		TEXT("Duplicate ids emit one focused validation error"),
		DuplicateContext.GetNumErrors() > 0);
#endif

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
