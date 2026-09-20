// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/PlayerStartPIE.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgPlayerStartSelectionTests
{
	/** No BeginPlay, profile restore, disk persistence or user map is involved in selection tests. */
	class FScopedWorld
	{
	public:
		FScopedWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (World)
			{
				Mode = Spawn<ARpgGameModeBase>(FVector::ZeroVector);
				if (Mode)
				{
					Mode->bEnableDiskPersistence = false;
				}
			}
		}

		~FScopedWorld()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}

		template <typename T>
		T* Spawn(const FVector& Location)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags = RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(Location, FRotator::ZeroRotator, Params);
		}

		UBoxComponent* AddBlocker(const FVector& Location, const FVector& Extent = FVector(20.0))
		{
			AActor* Owner = Spawn<AActor>(Location);
			if (!Owner) return nullptr;
			UBoxComponent* Box = NewObject<UBoxComponent>(Owner, NAME_None, RF_Transient);
			Owner->AddInstanceComponent(Box);
			Owner->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->SetWorldLocation(Location);
			Box->RegisterComponent();
			return Box;
		}

		UWorld* World = nullptr;
		ARpgGameModeBase* Mode = nullptr;

	private:
		UGameInstance* GameInstance = nullptr;
	};

	UBlueprint* MakeTransientPawnBlueprint(UClass* ParentClass, const TCHAR* Prefix)
	{
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), FName(Prefix)), BPTYPE_Normal);
		if (Blueprint) Blueprint->SetFlags(RF_Transient);
		return Blueprint;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPlayerStartMoverOccupancyTest,
	"SurvivalRpg.Spawning.PlayerStarts.BlueprintMoverUsesFreeAuthoredStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerStartMoverOccupancyTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerStartSelectionTests;
	UClass* PawnClass = LoadClass<APawn>(nullptr,
		TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/BP_RpgGasp_Mover.BP_RpgGasp_Mover_C"));
	if (!TestNotNull(TEXT("The actual project-owned Mover pawn loads"), PawnClass)) return false;
	TestNull(TEXT("This regression exercises collision authored entirely in Blueprint"),
		PawnClass->GetDefaultObject<APawn>()->GetRootComponent());
	FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Isolated game mode exists"), Fixture.Mode)) return false;
	Fixture.Mode->DefaultPawnClass = PawnClass;
	APlayerStart* Occupied = Fixture.Spawn<APlayerStart>(FVector(0, 0, 200));
	APlayerStart* Free = Fixture.Spawn<APlayerStart>(FVector(500, 0, 200));
	if (!TestNotNull(TEXT("Occupied start exists"), Occupied)
		|| !TestNotNull(TEXT("Free start exists"), Free)
		|| !TestNotNull(TEXT("A blocking body occupies one start"), Fixture.AddBlocker(Occupied->GetActorLocation()))) return false;
	TestTrue(TEXT("Engine CDO-only occupancy cannot detect this Blueprint root"),
		!Fixture.World->EncroachingBlockingGeometry(PawnClass->GetDefaultObject<APawn>(),
			Occupied->GetActorLocation(), Occupied->GetActorRotation()));
	TestTrue(TEXT("The free authored start is selected despite the empty pawn CDO"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == Free);

	APlayerStartPIE* CameraStart = Fixture.Spawn<APlayerStartPIE>(Occupied->GetActorLocation());
	if (!TestNotNull(TEXT("PIE camera start exists"), CameraStart)) return false;
	TestTrue(TEXT("An occupied camera start cannot override the free authored start"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == Free);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPlayerStartNativeAndFullOccupancyTest,
	"SurvivalRpg.Spawning.PlayerStarts.NativeCapsuleAndOccupiedCameraFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerStartNativeAndFullOccupancyTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerStartSelectionTests;
	FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Isolated game mode exists"), Fixture.Mode)) return false;
	Fixture.Mode->DefaultPawnClass = ACharacter::StaticClass();
	APlayerStartPIE* CameraStart = Fixture.Spawn<APlayerStartPIE>(FVector(0, 0, 200));
	APlayerStart* AuthoredStart = Fixture.Spawn<APlayerStart>(FVector(500, 0, 200));
	if (!TestNotNull(TEXT("PIE camera start exists"), CameraStart)
		|| !TestNotNull(TEXT("Authored start exists"), AuthoredStart)) return false;
	TestTrue(TEXT("An unoccupied camera start retains Play From Here priority"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == CameraStart);
	if (!TestNotNull(TEXT("Camera start blocker exists"), Fixture.AddBlocker(CameraStart->GetActorLocation()))) return false;
	TestTrue(TEXT("Native CMC collision also sends a second player to the free authored start"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == AuthoredStart);
	if (!TestNotNull(TEXT("Authored start blocker exists"), Fixture.AddBlocker(AuthoredStart->GetActorLocation()))) return false;
	TestTrue(TEXT("Complete occupancy preserves the existing engine fallback instead of introducing spawn failure/retry"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == CameraStart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPlayerStartInheritedCapsuleTest,
	"SurvivalRpg.Spawning.PlayerStarts.InheritedCapsuleTransformAndCollisionResponses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerStartInheritedCapsuleTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerStartSelectionTests;
	// Transient fixture classes deliberately differ from the GASP body. They are never saved.
	TStrongObjectPtr<UBlueprint> Parent(MakeTransientPawnBlueprint(APawn::StaticClass(), TEXT("SpawnCapsuleParent")));
	if (!TestNotNull(TEXT("Transient parent Blueprint exists"), Parent.Get())) return false;
	USimpleConstructionScript* Script = Parent->SimpleConstructionScript;
	if (!TestNotNull(TEXT("Parent construction script exists"), Script)) return false;
	const TArray<USCS_Node*> OriginalRoots = Script->GetRootNodes();
	for (USCS_Node* Node : OriginalRoots) Script->RemoveNode(Node);
	USCS_Node* CapsuleNode = Script->CreateNode(UCapsuleComponent::StaticClass(), TEXT("SpawnBody"));
	Script->AddNode(CapsuleNode);
	UCapsuleComponent* ParentCapsule = Cast<UCapsuleComponent>(CapsuleNode->ComponentTemplate);
	if (!TestNotNull(TEXT("Parent capsule template exists"), ParentCapsule)) return false;
	ParentCapsule->SetCapsuleSize(15.0f, 45.0f);
	ParentCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ParentCapsule->SetCollisionObjectType(ECC_Pawn);
	ParentCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Parent.Get());
	FKismetEditorUtilities::CompileBlueprint(Parent.Get(), EBlueprintCompileOptions::SkipGarbageCollection);
	if (!TestNotNull(TEXT("Parent class compiled"), Parent->GeneratedClass.Get())) return false;

	TStrongObjectPtr<UBlueprint> Child(MakeTransientPawnBlueprint(Parent->GeneratedClass, TEXT("SpawnCapsuleChild")));
	if (!TestNotNull(TEXT("Transient child Blueprint exists"), Child.Get())) return false;
	UInheritableComponentHandler* Handler = Child->GetInheritableComponentHandler(true);
	if (!TestNotNull(TEXT("Child component override handler exists"), Handler)) return false;
	UCapsuleComponent* Override = Cast<UCapsuleComponent>(Handler->CreateOverridenComponentTemplate(FComponentKey(CapsuleNode)));
	if (!TestNotNull(TEXT("Child capsule override exists"), Override)) return false;
	Override->SetCapsuleSize(80.0f, 140.0f);
	Override->SetRelativeLocation(FVector(40, 0, 0));
	Override->SetRelativeScale3D(FVector(1.5));
	FBlueprintEditorUtils::MarkBlueprintAsModified(Child.Get());
	FKismetEditorUtilities::CompileBlueprint(Child.Get(), EBlueprintCompileOptions::SkipGarbageCollection);
	if (!TestNotNull(TEXT("Child class compiled"), Child->GeneratedClass.Get())) return false;

	FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Isolated game mode exists"), Fixture.Mode)) return false;
	Fixture.Mode->DefaultPawnClass = Child->GeneratedClass;
	APlayerStartPIE* CameraStart = Fixture.Spawn<APlayerStartPIE>(FVector(0, 0, 250));
	APlayerStart* Free = Fixture.Spawn<APlayerStart>(FVector(1000, 0, 250));
	if (!TestNotNull(TEXT("PIE start exists"), CameraStart)
		|| !TestNotNull(TEXT("Free authored start exists"), Free)
		|| !TestNotNull(TEXT("Offset blocker exists"), Fixture.AddBlocker(FVector(140, 0, 250), FVector(5)))) return false;
	TestNull(TEXT("Inherited SCS root is absent from the CDO too"), Child->GeneratedClass->GetDefaultObject<APawn>()->GetRootComponent());
	TestTrue(TEXT("Child radius, root offset and scale all contribute to rejecting the camera start"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == Free);

	Fixture.Mode->DefaultPawnClass = Parent->GeneratedClass;
	TestTrue(TEXT("The smaller unmodified parent body fits at the same camera start"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == CameraStart);
	Fixture.Mode->DefaultPawnClass = Child->GeneratedClass;
	UCapsuleComponent* ActualOverride = Cast<UCapsuleComponent>(CapsuleNode->GetActualComponentTemplate(
		CastChecked<UBlueprintGeneratedClass>(Child->GeneratedClass)));
	if (!TestNotNull(TEXT("Compiled child exposes its actual inherited override"), ActualOverride)) return false;
	ActualOverride->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	TestTrue(TEXT("A nonblocking contact respects the actual template collision response"),
		Fixture.Mode->ChoosePlayerStart_Implementation(nullptr) == CameraStart);
	return true;
}

#endif
