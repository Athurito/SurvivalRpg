#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryContainerActor.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseConstructionSiteActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace RpgPhysicalStorageActionTests
{
	class FFixture
	{
	public:
		FFixture()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}
		~FFixture()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
			GameInstance->RemoveFromRoot();
		}
		template<typename T> T* Spawn(FVector Location = FVector::ZeroVector)
		{
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags = RF_Transient;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(Location, FRotator::ZeroRotator, Parameters);
		}
		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Standalone command world"), World)) return false;
			Controller = Spawn<ARpgInventoryAutomationTestPlayerController>();
			PlayerState = Spawn<ARpgInventoryAutomationTestPlayerState>();
			Pawn = Spawn<APawn>();
			if (!Controller || !PlayerState || !Pawn) return false;
			USceneComponent* Root = NewObject<USceneComponent>(Pawn, NAME_None, RF_Transient);
			Pawn->AddInstanceComponent(Root);
			Pawn->SetRootComponent(Root);
			Root->RegisterComponent();
			Controller->SetPlayerState(PlayerState);
			PlayerState->SetOwner(Controller);
			Controller->Possess(Pawn);
			Inventory = PlayerState->GetInventoryManagerComponent();
			Actions = Controller->GetInventoryUiActionComponent();
			Base = Spawn<ARpgBaseCampActor>();
			if (!Base || !Inventory || !Actions) return false;
			return Test.TestTrue(TEXT("Fixture base has a valid area"), Base->SetBaseArea(FVector::ZeroVector, 2000));
		}
		ARpgInventoryContainerActor* Chest(FVector Location = FVector(100, 0, 0))
		{
			ARpgInventoryContainerActor* Result = Spawn<ARpgInventoryContainerActor>(Location);
			Result->GetContainerComponent()->EnsurePersistentContainerId();
			return Result;
		}
		URpgInventoryItemInstance* AddPlayerMaterial(int32 Count, int32 X = 0)
		{
			FRpgInventoryGridPlacement Placement;
			Placement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId));
			Placement.X = X;
			Placement.Y = 0;
			return Inventory->AddItemDefinitionToPlacement(Material(), Count, Placement);
		}
		FRpgPhysicalStorageRequest Request(ARpgInventoryContainerActor* ChestActor, ERpgPhysicalStorageCommand Command) const
		{
			FRpgPhysicalStorageRequest Result;
			Result.RequestId = FGuid::NewGuid();
			Result.Command = Command;
			Result.ContainerId = ChestActor->GetContainerComponent()->GetPersistentContainerId();
			Result.ExpectedSettingsRevision = ChestActor->GetContainerComponent()->GetSettingsRevision();
			return Result;
		}
		void AddFloor()
		{
			AActor* Floor = Spawn<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor, NAME_None, RF_Transient);
			Floor->AddInstanceComponent(Box);
			Floor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10000, 10000, 10));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Floor->SetActorLocation(FVector(0, 0, -10));
		}
		static TSubclassOf<URpgInventoryItemDefinition> Material() { return URpgInventoryAutomationTestMaterialDefinition::StaticClass(); }
		UWorld* World = nullptr;
		ARpgInventoryAutomationTestPlayerController* Controller = nullptr;
		ARpgInventoryAutomationTestPlayerState* PlayerState = nullptr;
		APawn* Pawn = nullptr;
		ARpgBaseCampActor* Base = nullptr;
		URpgInventoryManagerComponent* Inventory = nullptr;
		URpgInventoryUiActionComponent* Actions = nullptr;
	private:
		UGameInstance* GameInstance = nullptr;
	};

	class FScopedChestDefinition
	{
	public:
		FScopedChestDefinition()
		{
			Property = FindFProperty<FObjectPropertyBase>(URpgInventoryContainerComponent::StaticClass(), TEXT("BuildableDefinition"));
			DefaultContainer = GetMutableDefault<ARpgInventoryContainerActor>()->GetContainerComponent();
			Previous = Property->GetObjectPropertyValue_InContainer(DefaultContainer);
			Definition = NewObject<URpgBaseBuildableDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			Definition->BuildActorClass = ARpgInventoryContainerActor::StaticClass();
			Definition->ChestUpgradeTiers.SetNum(3);
			for (int32 Tier = 0; Tier < 3; ++Tier)
			{
				Definition->ChestUpgradeTiers[Tier].GridSize.Width = 6;
				Definition->ChestUpgradeTiers[Tier].GridSize.Height = 4 + 2 * Tier;
			}
			Property->SetObjectPropertyValue_InContainer(DefaultContainer, Definition);
		}
		~FScopedChestDefinition() { Property->SetObjectPropertyValue_InContainer(DefaultContainer, Previous); }
		void ConfigureChest(ARpgInventoryContainerActor* Chest)
		{
			Property->SetObjectPropertyValue_InContainer(Chest->GetContainerComponent(), Definition);
			Chest->GetInventoryManager()->SetDefaultGridSize(Definition->ChestUpgradeTiers[0].GridSize);
		}
		void SetTierCost(int32 Tier, int32 Count)
		{
			FRpgBaseBuildResourceCost Cost;
			Cost.ItemDefinition = FFixture::Material();
			Cost.Count = Count;
			Definition->ChestUpgradeTiers[Tier].Costs = {Cost};
		}
		URpgBaseBuildableDefinition* Definition = nullptr;
	private:
		FObjectPropertyBase* Property = nullptr;
		URpgInventoryContainerComponent* DefaultContainer = nullptr;
		UObject* Previous = nullptr;
	};

	class FScopedAuthoredChestBuildCosts
	{
	public:
		FScopedAuthoredChestBuildCosts()
		{
			// Spawning must exercise the authored component archetype. Mutating a native actor CDO's
			// component after class initialization does not update the template used by new components.
			Definition = LoadObject<URpgBaseBuildableDefinition>(nullptr,
				TEXT("/Game/SurvivalRpg/Storage/Physical/DA_Buildable_SharedChest.DA_Buildable_SharedChest"));
			if (Definition) { PreviousCosts = Definition->BuildCosts; }
		}
		~FScopedAuthoredChestBuildCosts()
		{
			if (Definition) { Definition->BuildCosts = MoveTemp(PreviousCosts); }
		}
		URpgBaseBuildableDefinition* Definition = nullptr;
	private:
		TArray<FRpgBaseBuildResourceCost> PreviousCosts;
	};

	/** Turns the authored chest definition into a free station chest for one test and restores it afterwards. */
	class FScopedAuthoredStationChest
	{
	public:
		explicit FScopedAuthoredStationChest(float LinkRadius)
		{
			Definition = LoadObject<URpgBaseBuildableDefinition>(nullptr,
				TEXT("/Game/SurvivalRpg/Storage/Physical/DA_Buildable_SharedChest.DA_Buildable_SharedChest"));
			if (!Definition) { return; }
			PreviousCosts = Definition->BuildCosts;
			PreviousRadius = Definition->CraftingStationLinkRadius;
			PreviousNameFormat = Definition->LinkedStationNameFormat;
			Definition->BuildCosts.Reset();
			Definition->CraftingStationLinkRadius = LinkRadius;
			Definition->LinkedStationNameFormat = FText::FromString(TEXT("{Station} storage"));
		}
		~FScopedAuthoredStationChest()
		{
			if (!Definition) { return; }
			Definition->BuildCosts = MoveTemp(PreviousCosts);
			Definition->CraftingStationLinkRadius = PreviousRadius;
			Definition->LinkedStationNameFormat = PreviousNameFormat;
		}
		URpgBaseBuildableDefinition* Definition = nullptr;
	private:
		TArray<FRpgBaseBuildResourceCost> PreviousCosts;
		float PreviousRadius = 0.0f;
		FText PreviousNameFormat;
	};

	/** Spawns a crafting station; a None id leaves it without a stable identity, like a runtime-spawned station. */
	URpgCraftingStationComponent* SpawnStation(FFixture& Fixture, FVector Location, FName StationId, const FText& DisplayName = FText::GetEmpty())
	{
		ARpgCraftingStationActor* Actor = Fixture.Spawn<ARpgCraftingStationActor>(Location);
		URpgCraftingStationComponent* Station = Actor ? Actor->GetCraftingStationComponent() : nullptr;
		if (Station && !StationId.IsNone())
		{
			FindFProperty<FNameProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("PersistentStationId"))->SetPropertyValue_InContainer(Station, StationId);
		}
		else if (Actor)
		{
			// Uninitialized fixture worlds report every actor as a level actor; this one models a runtime spawn.
			Actor->bNetStartup = false;
			Actor->bNetLoadOnClient = false;
		}
		if (Station && !DisplayName.IsEmpty())
		{
			FindFProperty<FTextProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("StationDisplayName"))->SetPropertyValue_InContainer(Station, DisplayName);
		}
		return Station;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageCommandReplayAccessTest,
	"SurvivalRpg.Storage.Physical.Commands.AccessAndReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageCommandReplayAccessTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	ARpgInventoryContainerActor* Chest = Fixture.Chest();
	URpgInventoryContainerComponent* Container = Chest->GetContainerComponent();
	FRpgPhysicalStorageRequest Assign = Fixture.Request(Chest, ERpgPhysicalStorageCommand::SetAssignments);
	FRpgStorageAssignment Rule;
	Rule.ItemDefinition = FFixture::Material();
	Assign.Assignments = {Rule};
	Fixture.Actions->RequestPhysicalStorageCommand(Assign);
	if (!TestEqual(TEXT("Accessible assignment command applies"), Container->GetAssignments().Num(), 1)) return false;
	const int32 AssignedRevision = Container->GetSettingsRevision();
	Fixture.Actions->RequestPhysicalStorageCommand(Assign);
	TestEqual(TEXT("Exact retry does not allocate another rule order"), Container->GetSettingsRevision(), AssignedRevision);
	FRpgPhysicalStorageRequest ReusedId = Assign;
	ReusedId.Assignments.Reset();
	Fixture.Actions->RequestPhysicalStorageCommand(ReusedId);
	TestEqual(TEXT("Same request ID with changed payload cannot remove rules"), Container->GetAssignments().Num(), 1);
	FRpgPhysicalStorageRequest Stale = Fixture.Request(Chest, ERpgPhysicalStorageCommand::SetAssignments);
	Stale.ExpectedSettingsRevision = AssignedRevision - 1;
	Fixture.Actions->RequestPhysicalStorageCommand(Stale);
	TestEqual(TEXT("Stale revision cannot remove rules"), Container->GetAssignments().Num(), 1);
	Fixture.Pawn->SetActorLocation(FVector(5000, 0, 0));
	Fixture.Actions->RequestPhysicalStorageCommand(Fixture.Request(Chest, ERpgPhysicalStorageCommand::SetAssignments));
	TestEqual(TEXT("Out-of-range player cannot change rules"), Container->GetAssignments().Num(), 1);
	Fixture.Pawn->SetActorLocation(FVector::ZeroVector);
	if (!TestNotNull(TEXT("Player receives seven ordinary material units"), Fixture.AddPlayerMaterial(7))) return false;
	FRpgPhysicalStorageRequest Deposit = Fixture.Request(Chest, ERpgPhysicalStorageCommand::DepositMaterials);
	Fixture.Actions->RequestPhysicalStorageCommand(Deposit);
	TestEqual(TEXT("Deposit moves exactly the player stock"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 7);
	TestEqual(TEXT("Player stock is debited once"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 0);
	Fixture.AddPlayerMaterial(2);
	Fixture.Actions->RequestPhysicalStorageCommand(Deposit);
	TestEqual(TEXT("Replayed deposit does not sweep newly acquired material"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	TestEqual(TEXT("Replay conserves destination stock"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 7);
	Container->SetContainerAccessible(false);
	Fixture.Actions->RequestPhysicalStorageCommand(Fixture.Request(Chest, ERpgPhysicalStorageCommand::DepositMaterials));
	TestEqual(TEXT("Locked chest cannot trigger a deposit"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageUpgradePaymentTest,
	"SurvivalRpg.Storage.Physical.Commands.UpgradePayment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageUpgradePaymentTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FScopedChestDefinition Definition;
	Definition.SetTierCost(1, 5);
	Definition.SetTierCost(2, 7);
	ARpgInventoryContainerActor* Chest = Fixture.Chest();
	Definition.ConfigureChest(Chest);
	ARpgInventoryContainerActor* Stock = Fixture.Chest(FVector(800, 0, 0));
	Fixture.AddPlayerMaterial(3);
	Stock->GetInventoryManager()->AddItemDefinition(FFixture::Material(), 4);
	ARpgInventoryAutomationTestPlayerState* OtherPlayer = Fixture.Spawn<ARpgInventoryAutomationTestPlayerState>();
	ARpgInventoryAutomationTestPlayerController* OtherController = Fixture.Spawn<ARpgInventoryAutomationTestPlayerController>();
	OtherController->SetPlayerState(OtherPlayer);
	OtherPlayer->SetOwner(OtherController);
	URpgInventoryManagerComponent* OtherInventory = OtherPlayer->GetInventoryManagerComponent();
	FRpgInventoryGridPlacement OtherPlacement;
	OtherPlacement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId));
	OtherPlacement.X = 0;
	OtherPlacement.Y = 0;
	if (!TestNotNull(TEXT("Another player owns enough material to cover the otherwise missing upgrade cost"),
		OtherInventory->AddItemDefinitionToPlacement(FFixture::Material(), 10, OtherPlacement))) return false;
	const int32 OtherCount = OtherInventory->GetTotalItemCountByDefinition(FFixture::Material());
	FRpgPhysicalStorageRequest Upgrade = Fixture.Request(Chest, ERpgPhysicalStorageCommand::Upgrade);
	Fixture.Actions->RequestPhysicalStorageCommand(Upgrade);
	TestEqual(TEXT("First upgrade enters destination tier one"), Chest->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 1);
	TestEqual(TEXT("First upgrade uses tier-one dimensions"), Chest->GetInventoryManager()->GetDefaultGridSize().Height, 6);
	TestEqual(TEXT("Upgrade consumes player inventory first"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 0);
	TestEqual(TEXT("Upgrade consumes only the remaining two units from base storage"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	Fixture.Actions->RequestPhysicalStorageCommand(Upgrade);
	TestEqual(TEXT("Duplicate upgrade cannot charge twice"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	Fixture.Actions->RequestPhysicalStorageCommand(Fixture.Request(Chest, ERpgPhysicalStorageCommand::Upgrade));
	TestEqual(TEXT("Insufficient own/shared materials preserve the current tier"), Chest->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 1);
	TestEqual(TEXT("Failed upgrade retains all shared materials"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	TestEqual(TEXT("Other player's stock is never consumed"), OtherInventory->GetTotalItemCountByDefinition(FFixture::Material()), OtherCount);
	Fixture.AddPlayerMaterial(5);
	Fixture.Actions->RequestPhysicalStorageCommand(Fixture.Request(Chest, ERpgPhysicalStorageCommand::Upgrade));
	TestEqual(TEXT("Second upgrade enters the final tier"), Chest->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 2);
	TestEqual(TEXT("Final root grid has eight rows"), Chest->GetInventoryManager()->GetDefaultGridSize().Height, 8);
	TestEqual(TEXT("Complete payment conserves the expected remainder"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()) + Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStoragePlacementContractTest,
	"SurvivalRpg.Storage.Physical.Commands.Placement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStoragePlacementContractTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FScopedChestDefinition Definition;
	FText Reason;
	const FTransform ValidTransform(FRotator::ZeroRotator, FVector(200, 0, 0));
	TestFalse(TEXT("Unsupported placement without ground is rejected"), Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, ValidTransform, NAME_None, Reason));
	Fixture.AddFloor();
	TestTrue(TEXT("Supported empty floor accepts the authored footprint"), Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, ValidTransform, NAME_None, Reason));
	FTransform Scaled = ValidTransform;
	Scaled.SetScale3D(FVector(0.1));
	TestFalse(TEXT("Client cannot shrink the collision footprint using scale"), Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, Scaled, NAME_None, Reason));
	TestFalse(TEXT("Placement beyond builder reach is rejected"), Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, FTransform(FVector(1500, 0, 0)), NAME_None, Reason));
	Fixture.Pawn->SetActorLocation(FVector(1800, 0, 0));
	TestFalse(TEXT("A footprint straddling the base boundary is rejected"), Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, FTransform(FVector(1980, 0, 0)), NAME_None, Reason));
	Fixture.Pawn->SetActorLocation(FVector::ZeroVector);
	URpgBaseBuildableDefinition* Mismatched = NewObject<URpgBaseBuildableDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Mismatched->BuildActorClass = ARpgInventoryContainerActor::StaticClass();
	TestFalse(TEXT("One actor class cannot be built with a substituted cost definition"), Fixture.Actions->CanPlacePhysicalStorage(Mismatched, ValidTransform, NAME_None, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageBuildRelocateTest,
	"SurvivalRpg.Storage.Physical.Commands.BuildAndRelocate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageBuildRelocateTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FScopedAuthoredChestBuildCosts Definition;
	if (!TestNotNull(TEXT("Authored physical chest build definition"), Definition.Definition) ||
		!TestFalse(TEXT("Authored chest has a starting capacity tier"), Definition.Definition->ChestUpgradeTiers.IsEmpty())) return false;
	Fixture.AddFloor();
	FRpgBaseBuildResourceCost Cost;
	Cost.ItemDefinition = FFixture::Material();
	Cost.Count = 5;
	Definition.Definition->BuildCosts = {Cost};
	ARpgInventoryContainerActor* Stock = Fixture.Chest(FVector(700, 0, 0));
	Fixture.AddPlayerMaterial(3);
	Stock->GetInventoryManager()->AddItemDefinition(FFixture::Material(), 4);
	auto BuiltChests = [&Fixture]()
	{
		TArray<ARpgInventoryContainerActor*> Results;
		for (TActorIterator<ARpgInventoryContainerActor> It(Fixture.World); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->GetContainerComponent()->ExportPhysicalStorageMetadata().bRuntimeBuilt) Results.Add(*It);
		}
		return Results;
	};
	FRpgPhysicalStorageRequest Build;
	Build.RequestId = FGuid::NewGuid();
	Build.Command = ERpgPhysicalStorageCommand::Build;
	Build.BuildableDefinition = Definition.Definition;
	Build.Transform = FTransform(FVector(200, 0, 0));
	FText PlacementReason;
	if (!TestTrue(TEXT("Initial build placement is accepted"),
		Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, Build.Transform, NAME_None, PlacementReason))) return false;
	Fixture.Actions->RequestPhysicalStorageCommand(Build);
	TArray<ARpgInventoryContainerActor*> Constructed = BuiltChests();
	if (!TestEqual(TEXT("Paid construction publishes one physical chest"), Constructed.Num(), 1)) return false;
	ARpgInventoryContainerActor* Built = Constructed[0];
	TestEqual(TEXT("Constructed actor retains its authored build definition"),
		Built->GetContainerComponent()->GetBuildableDefinition(), Definition.Definition);
	TestFalse(TEXT("Committed chest leaves construction staging"), Built->GetContainerComponent()->IsConstructionPending());
	TestEqual(TEXT("Build consumes the acting player's three units first"), Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 0);
	TestEqual(TEXT("Build pays only the remaining two units from the base"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	TestTrue(TEXT("Fresh build uses its authored starting tier"),
		Built->GetInventoryManager()->GetDefaultGridSize() == Definition.Definition->ChestUpgradeTiers[0].GridSize);
	Fixture.Actions->RequestPhysicalStorageCommand(Build);
	TestEqual(TEXT("Build replay cannot create a second chest"), BuiltChests().Num(), 1);
	TestEqual(TEXT("Build replay retains the shared remainder"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	FRpgPhysicalStorageRequest Unfunded = Build;
	Unfunded.RequestId = FGuid::NewGuid();
	Unfunded.Transform = FTransform(FVector(400, 0, 0));
	Fixture.Actions->RequestPhysicalStorageCommand(Unfunded);
	TestEqual(TEXT("Unfunded construction publishes no actor"), BuiltChests().Num(), 1);
	TestEqual(TEXT("Unfunded construction consumes no partial payment"), Stock->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 2);
	URpgInventoryItemInstance* KeptItem = Built->GetInventoryManager()->AddItemDefinition(FFixture::Material(), 1);
	if (!TestNotNull(TEXT("Built chest accepts a concrete item"), KeptItem)) return false;
	const FRpgInventoryItemId KeptId = KeptItem->GetItemId();
	const FName ContainerId = Built->GetContainerComponent()->GetPersistentContainerId();
	FRpgPhysicalStorageRequest BeginMove = Fixture.Request(Built, ERpgPhysicalStorageCommand::BeginRelocate);
	Fixture.Actions->RequestPhysicalStorageCommand(BeginMove);
	FRpgPhysicalStorageRequest Move = Fixture.Request(Built, ERpgPhysicalStorageCommand::Relocate);
	Move.RelocationSessionId = BeginMove.RequestId;
	Move.Transform = FTransform(FRotator(0, 90, 0), FVector(300, 0, 0));
	Fixture.Actions->RequestPhysicalStorageCommand(Move);
	TestTrue(TEXT("Confirmed relocation updates the existing actor"), Built->GetActorTransform().Equals(Move.Transform));
	TestEqual(TEXT("Relocation preserves chest identity"), Built->GetContainerComponent()->GetPersistentContainerId(), ContainerId);
	TestNotNull(TEXT("Relocation preserves concrete item identity"), Built->GetInventoryManager()->FindItemById(KeptId));
	const int32 RevisionAfterMove = Built->GetContainerComponent()->GetSettingsRevision();
	Fixture.Actions->RequestPhysicalStorageCommand(Move);
	TestEqual(TEXT("Move replay does not mutate settings twice"), Built->GetContainerComponent()->GetSettingsRevision(), RevisionAfterMove);
	FRpgPhysicalStorageRequest InvalidMove = Fixture.Request(Built, ERpgPhysicalStorageCommand::Relocate);
	InvalidMove.Transform = FTransform(FVector(5000, 0, 0));
	Fixture.Actions->RequestPhysicalStorageCommand(InvalidMove);
	TestTrue(TEXT("Rejected relocation leaves the last confirmed transform intact"), Built->GetActorTransform().Equals(Move.Transform));
	TestNotNull(TEXT("Rejected relocation preserves inventory contents"), Built->GetInventoryManager()->FindItemById(KeptId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageRelocationSessionTest,
	"SurvivalRpg.Storage.Physical.Commands.RelocationSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageRelocationSessionTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FScopedChestDefinition Definition;
	Fixture.AddFloor();
	ARpgInventoryContainerActor* Chest = Fixture.Chest();
	Definition.ConfigureChest(Chest);
	URpgInventoryItemInstance* Item = Chest->GetInventoryManager()->AddItemDefinition(FFixture::Material(), 4);
	if (!TestNotNull(TEXT("Movable chest holds concrete material"), Item)) return false;
	const FRpgInventoryItemId ItemId = Item->GetItemId();
	const FVector Original = Chest->GetActorLocation();
	auto Begin = [&]() {
		FRpgPhysicalStorageRequest Request = Fixture.Request(Chest, ERpgPhysicalStorageCommand::BeginRelocate);
		Fixture.Actions->RequestPhysicalStorageCommand(Request);
		return Request.RequestId;
	};
	auto Move = [&](FGuid Session, FVector Location) {
		FRpgPhysicalStorageRequest Request = Fixture.Request(Chest, ERpgPhysicalStorageCommand::Relocate);
		Request.RelocationSessionId = Session;
		Request.Transform = FTransform(Location);
		Fixture.Actions->RequestPhysicalStorageCommand(Request);
		return Request;
	};
	Fixture.Pawn->SetActorLocation(FVector(700, 0, 0));
	Move(Begin(), FVector(800, 0, 0));
	TestTrue(TEXT("A remote start cannot authorize moving a shared chest"), Chest->GetActorLocation().Equals(Original));
	Fixture.Pawn->SetActorLocation(FVector::ZeroVector);
	const FGuid Authorized = Begin();
	Fixture.Pawn->SetActorLocation(FVector(700, 0, 0));
	TestFalse(TEXT("Walking left direct interaction reach"), Chest->GetContainerComponent()->CanActorAccess(Fixture.Pawn));
	Move(FGuid::NewGuid(), FVector(800, 0, 0));
	TestTrue(TEXT("A guessed move token is rejected"), Chest->GetActorLocation().Equals(Original));
	ARpgInventoryAutomationTestPlayerController* OtherController = Fixture.Spawn<ARpgInventoryAutomationTestPlayerController>();
	ARpgInventoryAutomationTestPlayerState* OtherState = Fixture.Spawn<ARpgInventoryAutomationTestPlayerState>();
	OtherController->SetPlayerState(OtherState);
	OtherState->SetOwner(OtherController);
	OtherController->Possess(Fixture.Spawn<APawn>());
	FRpgPhysicalStorageRequest Foreign = Fixture.Request(Chest, ERpgPhysicalStorageCommand::Relocate);
	Foreign.RelocationSessionId = Authorized;
	Foreign.Transform = FTransform(FVector(400, 0, 0));
	OtherController->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(Foreign);
	TestTrue(TEXT("A second controller cannot borrow another player's move authorization"), Chest->GetActorLocation().Equals(Original));
	// Ordinary crafting/withdrawal changes contents, not the container settings authorization.
	Chest->GetInventoryManager()->AddItemDefinition(FFixture::Material(), 2);
	bool bTransformCallbackRan = false;
	bool bNestedMoveAccepted = false;
	const FDelegateHandle TransformCallback = Chest->GetRootComponent()->TransformUpdated.AddLambda(
		[&](USceneComponent*, EUpdateTransformFlags, ETeleportType) {
			bTransformCallbackRan = true;
			bNestedMoveAccepted |= Chest->GetContainerComponent()->TryRelocatePhysicalStorage(
				FTransform(FVector(900, 0, 0)), Chest->GetContainerComponent()->GetSettingsRevision());
		});
	const FRpgPhysicalStorageRequest Confirmed = Move(Authorized, FVector(800, 0, 0));
	Chest->GetRootComponent()->TransformUpdated.Remove(TransformCallback);
	TestTrue(TEXT("Test exercised synchronous transform callbacks"), bTransformCallbackRan);
	TestFalse(TEXT("A nested mover cannot change this chest during publication"), bNestedMoveAccepted);
	TestTrue(TEXT("Authorized move follows the player across the same base"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	TestNotNull(TEXT("Walking relocation retains original item identity"), Chest->GetInventoryManager()->FindItemById(ItemId));
	TestEqual(TEXT("Concurrent content changes remain intact"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 6);
	const int32 Revision = Chest->GetContainerComponent()->GetSettingsRevision();
	Fixture.Actions->RequestPhysicalStorageCommand(Confirmed);
	TestEqual(TEXT("Confirmed move replay cannot mutate twice"), Chest->GetContainerComponent()->GetSettingsRevision(), Revision);
	Move(Authorized, FVector(900, 0, 0));
	TestTrue(TEXT("Consumed move token cannot authorize another request"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));

	const FGuid Cancelled = Begin();
	FRpgPhysicalStorageRequest Cancel = Fixture.Request(Chest, ERpgPhysicalStorageCommand::CancelRelocate);
	Cancel.RelocationSessionId = Cancelled;
	Fixture.Actions->RequestPhysicalStorageCommand(Cancel);
	Move(Cancelled, FVector(900, 0, 0));
	TestTrue(TEXT("Cancel revokes the session"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	const FGuid Stale = Begin();
	Chest->GetContainerComponent()->MarkPhysicalStorageMoved();
	Move(Stale, FVector(900, 0, 0));
	TestTrue(TEXT("Another mover/settings mutation invalidates even a fresh client revision"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	const FGuid Outside = Begin();
	Fixture.Pawn->SetActorLocation(FVector(2100, 0, 0));
	Move(Outside, FVector(1500, 0, 0));
	TestTrue(TEXT("Session cannot be used from outside the original base"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	Fixture.Pawn->SetActorLocation(FVector(700, 0, 0));
	const FGuid Locked = Begin();
	Chest->GetContainerComponent()->SetContainerAccessible(false);
	Move(Locked, FVector(900, 0, 0));
	TestTrue(TEXT("A locked chest cannot be moved by an existing session"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	Chest->GetContainerComponent()->SetContainerAccessible(true);
	const FGuid OldPawn = Begin();
	APawn* Replacement = Fixture.Spawn<APawn>();
	Fixture.Controller->Possess(Replacement);
	Move(OldPawn, FVector(900, 0, 0));
	TestTrue(TEXT("Respawn cannot reuse the previous pawn authorization"), Chest->GetActorLocation().Equals(FVector(800, 0, 0)));
	TestEqual(TEXT("Rejected requests never alter material counts"), Chest->GetInventoryManager()->GetTotalItemCountByDefinition(FFixture::Material()), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageRejectsLegacyConstructionTest,
	"SurvivalRpg.Storage.Physical.Commands.RejectsLegacyConstructionBypass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageRejectsLegacyConstructionTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	Fixture.AddPlayerMaterial(5);
	for (UClass* BuildClass : { ARpgInventoryContainerActor::StaticClass(), ARpgBaseStorageStationActor::StaticClass() })
	{
		URpgBaseBuildableDefinition* Definition = NewObject<URpgBaseBuildableDefinition>();
		Definition->BuildActorClass = BuildClass;
		FRpgBaseBuildResourceCost& Cost = Definition->BuildCosts.AddDefaulted_GetRef();
		Cost.ItemDefinition = FFixture::Material();
		Cost.Count = 1;
		Fixture.Actions->RequestPlaceBaseBuildable(Fixture.Base, Definition,
			FTransform(FVector(100, 0, 250)), true);
	}
	int32 ConstructionSites = 0;
	for (TActorIterator<ARpgBaseConstructionSiteActor> It(Fixture.World); It; ++It) ++ConstructionSites;
	int32 Containers = 0;
	for (TActorIterator<ARpgInventoryContainerActor> It(Fixture.World); It; ++It) ++Containers;
	int32 LegacyStations = 0;
	for (TActorIterator<ARpgBaseStorageStationActor> It(Fixture.World); It; ++It) ++LegacyStations;
	TestEqual(TEXT("Legacy construction cannot stage a storage bypass"), ConstructionSites, 0);
	TestEqual(TEXT("Legacy construction cannot spawn an unchecked physical chest"), Containers, 0);
	TestEqual(TEXT("Legacy construction cannot recreate retired virtual storage"), LegacyStations, 0);
	TestEqual(TEXT("Rejected legacy storage construction preserves player materials"),
		Fixture.Inventory->GetTotalItemCountByDefinition(FFixture::Material()), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgStationChestPlacementTest,
	"SurvivalRpg.Storage.Physical.StationChest.LinkOnPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStationChestPlacementTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageActionTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) return false;
	FScopedAuthoredStationChest Definition(300.0f);
	if (!TestNotNull(TEXT("Authored chest build definition"), Definition.Definition)) return false;
	Fixture.AddFloor();
	auto BuiltChests = [&Fixture]()
	{
		TArray<URpgInventoryContainerComponent*> Results;
		for (TActorIterator<ARpgInventoryContainerActor> It(Fixture.World); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->GetContainerComponent()->ExportPhysicalStorageMetadata().bRuntimeBuilt) Results.Add(It->GetContainerComponent());
		}
		return Results;
	};
	auto CanPlace = [&](FVector Location)
	{
		FText Reason;
		return Fixture.Actions->CanPlacePhysicalStorage(Definition.Definition, FTransform(Location), NAME_None, Reason);
	};
	auto Build = [&](FVector Location)
	{
		FRpgPhysicalStorageRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.Command = ERpgPhysicalStorageCommand::Build;
		Request.BuildableDefinition = Definition.Definition;
		Request.Transform = FTransform(Location);
		Fixture.Actions->RequestPhysicalStorageCommand(Request);
	};

	const FVector FirstLocation(250, 0, 0);
	TestFalse(TEXT("A station chest needs a crafting station in range"), CanPlace(FirstLocation));
	Build(FirstLocation);
	TestEqual(TEXT("Without a station nothing is built"), BuiltChests().Num(), 0);

	URpgCraftingStationComponent* Kiln = SpawnStation(Fixture, FVector(500, 0, 0), TEXT("Station_Kiln"), FText::FromString(TEXT("Kiln")));
	// Nearer than the kiln, but a station without a stable id cannot keep a saved link. Stations and chests keep their
	// 120 cm interaction spheres clear of every footprint, since those spheres block the placement test.
	URpgCraftingStationComponent* Unsaved = SpawnStation(Fixture, FVector(250, -200, 0), NAME_None);
	if (!TestNotNull(TEXT("Kiln station"), Kiln) || !TestNotNull(TEXT("Station without id"), Unsaved)) return false;
	TestTrue(TEXT("A free station in range allows placement"), CanPlace(FirstLocation));
	Build(FirstLocation);
	TArray<URpgInventoryContainerComponent*> Built = BuiltChests();
	if (!TestEqual(TEXT("Placement builds one station chest"), Built.Num(), 1)) return false;
	URpgInventoryContainerComponent* KilnChest = Built[0];
	TestEqual(TEXT("The chest links to the nearest station with a stable id"), KilnChest->GetLinkedStationId(), FName(TEXT("Station_Kiln")));
	TestTrue(TEXT("The linked chest is a station chest"), KilnChest->IsStationChest());
	TestEqual(TEXT("The chest is named after its station"), KilnChest->GetStorageDisplayName().ToString(), FString(TEXT("Kiln storage")));
	TestEqual(TEXT("The station targets its chest by default"), Kiln->GetDefaultOutputTargetId(), KilnChest->GetPersistentContainerId());
	TestTrue(TEXT("The station without id keeps automatic storing"), Unsaved->GetDefaultOutputTargetId().IsNone());

	// One chest per station: the kiln is taken, so a second chest needs another free station in range.
	const FVector SecondLocation(400, 250, 0);
	TestFalse(TEXT("A station with a chest accepts no second one"), CanPlace(SecondLocation));
	Build(SecondLocation);
	TestEqual(TEXT("The rejected second chest is not built"), BuiltChests().Num(), 1);
	URpgCraftingStationComponent* Forge = SpawnStation(Fixture, FVector(350, 480, 0), TEXT("Station_Forge"), FText::FromString(TEXT("Forge")));
	if (!TestNotNull(TEXT("Forge station"), Forge)) return false;
	Build(SecondLocation);
	Built = BuiltChests();
	if (!TestEqual(TEXT("The second chest links to the next free station"), Built.Num(), 2)) return false;
	URpgInventoryContainerComponent* ForgeChest = Built[0] == KilnChest ? Built[1] : Built[0];
	TestEqual(TEXT("The second chest belongs to the forge"), ForgeChest->GetLinkedStationId(), FName(TEXT("Station_Forge")));
	TestEqual(TEXT("The kiln keeps its own chest"), Kiln->GetDefaultOutputTargetId(), KilnChest->GetPersistentContainerId());
	TestEqual(TEXT("The forge targets its own chest"), Forge->GetDefaultOutputTargetId(), ForgeChest->GetPersistentContainerId());

	// Relocation keeps the link within range and rejects a move away from every free station.
	AActor* KilnChestActor = KilnChest->GetOwner();
	auto Move = [&](FVector Location)
	{
		FRpgPhysicalStorageRequest Begin = Fixture.Request(Cast<ARpgInventoryContainerActor>(KilnChestActor), ERpgPhysicalStorageCommand::BeginRelocate);
		Fixture.Actions->RequestPhysicalStorageCommand(Begin);
		FRpgPhysicalStorageRequest Request = Fixture.Request(Cast<ARpgInventoryContainerActor>(KilnChestActor), ERpgPhysicalStorageCommand::Relocate);
		Request.RelocationSessionId = Begin.RequestId;
		Request.Transform = FTransform(Location);
		Fixture.Actions->RequestPhysicalStorageCommand(Request);
	};
	Move(FVector(450, -200, 0));
	TestTrue(TEXT("A move within range is confirmed"), KilnChestActor->GetActorLocation().Equals(FVector(450, -200, 0)));
	TestEqual(TEXT("The moved chest keeps its station"), KilnChest->GetLinkedStationId(), FName(TEXT("Station_Kiln")));
	Fixture.Pawn->SetActorLocation(FVector(300, -100, 0));
	Move(FVector(-200, 0, 0));
	TestTrue(TEXT("A move away from every free station is rejected"), KilnChestActor->GetActorLocation().Equals(FVector(450, -200, 0)));
	TestEqual(TEXT("A rejected move keeps the link"), KilnChest->GetLinkedStationId(), FName(TEXT("Station_Kiln")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgStationChestAssetContractTest,
	"SurvivalRpg.Storage.Physical.StationChest.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStationChestAssetContractTest::RunTest(const FString& Parameters)
{
	const URpgBaseBuildableDefinition* Definition = LoadObject<URpgBaseBuildableDefinition>(nullptr,
		TEXT("/Game/SurvivalRpg/Storage/Physical/DA_Buildable_StationChest.DA_Buildable_StationChest"));
	if (!TestNotNull(TEXT("The station chest definition exists"), Definition)) return false;
	TestTrue(TEXT("The definition links to a station"), Definition->CraftingStationLinkRadius > 0.0f);
	TestTrue(TEXT("The linked name includes the station"), Definition->LinkedStationNameFormat.ToString().Contains(TEXT("{Station}")));
	TestTrue(TEXT("The station chest is upgradeable"), Definition->ChestUpgradeTiers.Num() >= 2);
	for (int32 Tier = 1; Tier < Definition->ChestUpgradeTiers.Num(); ++Tier)
	{
		const FRpgInventoryGridSize& Previous = Definition->ChestUpgradeTiers[Tier - 1].GridSize;
		const FRpgInventoryGridSize& Current = Definition->ChestUpgradeTiers[Tier].GridSize;
		TestTrue(TEXT("Every upgrade tier enlarges the grid"),
			Current.Width >= Previous.Width && Current.Height >= Previous.Height && !(Current == Previous));
	}
	const ARpgInventoryContainerActor* Template = Definition->BuildActorClass
		? Cast<ARpgInventoryContainerActor>(Definition->BuildActorClass->GetDefaultObject())
		: nullptr;
	if (!TestNotNull(TEXT("The definition builds a physical chest"), Template)) return false;
	TestTrue(TEXT("The chest Blueprint uses the station chest definition"),
		Template->GetContainerComponent()->GetBuildableDefinition() == Definition);
	return true;
}

#endif
