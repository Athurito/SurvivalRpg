// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInteractionPresentation.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Interaction/Components/RpgInteractionPromptAnchorComponent.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

namespace RpgInteractionPresentationTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
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

	AActor* SpawnTestActor(UWorld* World, const TCHAR* Name)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			AActor::StaticClass(),
			Name);
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
	ComponentType* AddSceneComponent(
		AActor* Owner,
		const FName ComponentName)
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
		Component->SetupAttachment(Owner->GetRootComponent());
		Component->RegisterComponent();
		return Component;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionExpandedPromptStateTest,
	"SurvivalRpg.Interaction.Presentation.ExpandedPromptStates",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionExpandedPromptStateTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	TestTrue(
		TEXT("Ready owns the expanded key-and-action prompt"),
		RpgInteractionPresentation::IsFullPromptState(
			ERpgInteractionPromptState::Ready));
	TestTrue(
		TEXT("Blocked owns the expanded reason prompt"),
		RpgInteractionPresentation::IsFullPromptState(
			ERpgInteractionPromptState::Blocked));
	TestFalse(
		TEXT("Nearby remains a compact circle"),
		RpgInteractionPresentation::IsFullPromptState(
			ERpgInteractionPromptState::Nearby));
	TestFalse(
		TEXT("Focused out of range remains a compact circle"),
		RpgInteractionPresentation::IsFullPromptState(
			ERpgInteractionPromptState::FocusedOutOfRange));
	TestFalse(
		TEXT("Hidden owns no expanded presentation"),
		RpgInteractionPresentation::IsFullPromptState(
			ERpgInteractionPromptState::Hidden));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInteractionDescriptorPlacementTest,
	"SurvivalRpg.Interaction.Presentation.DescriptorPlacement",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInteractionDescriptorPlacementTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInteractionPresentationTests;

	FScopedTestWorld TestWorld;
	AActor* AnchoredActor = SpawnTestActor(
		TestWorld.GetWorld(),
		TEXT("AnchoredInteractionActor"));
	URpgInteractionPromptAnchorComponent* PromptAnchor =
		AddSceneComponent<URpgInteractionPromptAnchorComponent>(
			AnchoredActor,
			TEXT("PromptAnchor"));
	if (!TestNotNull(TEXT("Anchored test actor exists"), AnchoredActor) ||
		!TestNotNull(TEXT("Prompt anchor exists"), PromptAnchor))
	{
		return false;
	}

	FInteractionOption AnchoredOption;
	AnchoredOption.TargetRef.TargetActor = AnchoredActor;
	AnchoredOption.TargetRef.WorldLocation = FVector(9000.0f);
	UIndicatorDescriptor* Descriptor = NewObject<UIndicatorDescriptor>();
	Descriptor->SetWorldPositionOverride(FVector(1234.0f));
	TestTrue(
		TEXT("Explicit prompt anchor configures a descriptor"),
		RpgInteractionPresentation::ConfigureDescriptorPlacement(
			*Descriptor,
			AnchoredOption));
	TestEqual(
		TEXT("Explicit anchor owns descriptor lifetime and projection"),
		Descriptor->GetSceneComponent(),
		static_cast<USceneComponent*>(PromptAnchor));
	TestEqual(
		TEXT("Explicit anchor uses point projection"),
		Descriptor->GetProjectionMode(),
		EActorCanvasProjectionMode::ComponentPoint);
	TestFalse(
		TEXT("Explicit anchor clears the gameplay hit-point override"),
		Descriptor->HasWorldPositionOverride());

	PromptAnchor->DestroyComponent();
	TestTrue(
		TEXT("Destroyed anchors safely fall back to actor bounds"),
		RpgInteractionPresentation::ConfigureDescriptorPlacement(
			*Descriptor,
			AnchoredOption));
	TestEqual(
		TEXT("Actor root owns bounds-fallback lifetime"),
		Descriptor->GetSceneComponent(),
		AnchoredActor->GetRootComponent());
	TestEqual(
		TEXT("Normal actors project from stable actor bounds"),
		Descriptor->GetProjectionMode(),
		EActorCanvasProjectionMode::ActorBoundingBox);
	TestFalse(
		TEXT("Bounds fallback never reuses the gameplay hit point"),
		Descriptor->HasWorldPositionOverride());

	AActor* InstancedActor = SpawnTestActor(
		TestWorld.GetWorld(),
		TEXT("InstancedInteractionActor"));
	UInstancedStaticMeshComponent* InstancedMesh =
		AddSceneComponent<UInstancedStaticMeshComponent>(
			InstancedActor,
			TEXT("Instances"));
	if (!TestNotNull(TEXT("Instanced test actor exists"), InstancedActor) ||
		!TestNotNull(TEXT("Instanced mesh exists"), InstancedMesh))
	{
		return false;
	}

	const FVector InstanceLocation(125.0f, -40.0f, 75.0f);
	const int32 InstanceIndex = InstancedMesh->AddInstance(
		FTransform(InstanceLocation));
	FInteractionOption InstanceOption;
	InstanceOption.TargetRef.TargetActor = InstancedActor;
	InstanceOption.TargetRef.TargetComponent = InstancedMesh;
	InstanceOption.TargetRef.InstanceIndex = InstanceIndex;
	InstanceOption.TargetRef.WorldLocation = FVector(-9000.0f);
	TestTrue(
		TEXT("A valid ISM instance configures a descriptor"),
		RpgInteractionPresentation::ConfigureDescriptorPlacement(
			*Descriptor,
			InstanceOption));
	TestEqual(
		TEXT("The instance component owns descriptor lifetime"),
		Descriptor->GetSceneComponent(),
		static_cast<USceneComponent*>(InstancedMesh));
	TestTrue(
		TEXT("Instances use an explicit stable per-instance point"),
		Descriptor->HasWorldPositionOverride());
	TestTrue(
		TEXT("Instance projection uses its transform rather than the hit point"),
		Descriptor->GetWorldPositionOverride().Equals(
			InstanceLocation,
			KINDA_SMALL_NUMBER));

	InstanceOption.TargetRef.InstanceIndex = InstanceIndex + 100;
	TestFalse(
		TEXT("Invalid instance indices do not produce a marker"),
		RpgInteractionPresentation::ConfigureDescriptorPlacement(
			*Descriptor,
			InstanceOption));
	TestNull(
		TEXT("Invalid instance placement releases its component"),
		Descriptor->GetSceneComponent());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
