#if WITH_DEV_AUTOMATION_TESTS

#include "RpgStorageAccessRules.h"
#include "RpgBaseCampActor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PhysicalStorageTestWoodOak, "Item.Material.AutomationStorage.Wood.Oak");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PhysicalStorageTestWood, "Item.Material.AutomationStorage.Wood");

namespace RpgPhysicalStorageRulesTests
{
	class FScopedWorld
	{
	public:
		FScopedWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
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
		template<typename T> T* Spawn(FVector Location = FVector::ZeroVector)
		{
			FActorSpawnParameters Parameters;
			Parameters.ObjectFlags = RF_Transient;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(Location, FRotator::ZeroRotator, Parameters);
		}
		UWorld* World = nullptr;
	private:
		UGameInstance* GameInstance = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageAreaGeometryTest,
	"SurvivalRpg.Storage.Physical.AreaGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageAreaGeometryTest::RunTest(const FString& Parameters)
{
	using namespace RpgStorageAccessRules;
	TestTrue(TEXT("Base membership ignores floor height"), IsInsideBaseArea(FVector::ZeroVector, 100.0f, FVector(20, 30, 100000)));
	TestTrue(TEXT("Base boundary belongs to its base"), IsInsideBaseArea(FVector::ZeroVector, 100.0f, FVector(100, 0, 0)));
	TestFalse(TEXT("A point beyond the horizontal radius is outside"), IsInsideBaseArea(FVector::ZeroVector, 100.0f, FVector(101, 0, 0)));
	TestTrue(TEXT("Touching areas conflict"), BaseAreasConflict(FVector::ZeroVector, 100, FVector(200, 0, 500), 100));
	TestFalse(TEXT("Separated areas are permitted"), BaseAreasConflict(FVector::ZeroVector, 100, FVector(201, 0, 0), 100));
	TestFalse(TEXT("Zero never means unlimited base area"), IsInsideBaseArea(FVector::ZeroVector, 0, FVector::ZeroVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageSourceDomainTest,
	"SurvivalRpg.Storage.Physical.SourceDomain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageSourceDomainTest::RunTest(const FString& Parameters)
{
	RpgPhysicalStorageRulesTests::FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Standalone test world"), Fixture.World)) return false;
	ARpgBaseCampActor* Base = Fixture.Spawn<ARpgBaseCampActor>();
	if (!TestNotNull(TEXT("Base exists"), Base)) return false;
	TestTrue(TEXT("Base accepts a finite non-conflicting radius"), Base->SetBaseArea(FVector::ZeroVector, 1000));
	ARpgBaseCampActor* Second = Fixture.Spawn<ARpgBaseCampActor>(FVector(6000, 0, 0));
	TestTrue(TEXT("Second base accepts a distinct area"), Second->SetBaseArea(FVector(3000, 0, 0), 1000));
	TestFalse(TEXT("Moving to touch the first area is rejected"), Second->SetBaseArea(FVector(2000, 0, 0), 1000));
	ARpgInventoryContainerActor* FarInside = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(900, 0, 8000));
	ARpgInventoryContainerActor* NearInside = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(900, 0, 0));
	ARpgInventoryContainerActor* Outside = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(1100, 0, 0));
	NearInside->DispatchBeginPlay();
	Outside->DispatchBeginPlay();
	TestEqual(TEXT("Placed chest publishes its base on first play before any save or query"), NearInside->GetContainerComponent()->GetBaseId(), Base->GetBaseId());
	TestTrue(TEXT("Outside chest publishes no base membership"), Outside->GetContainerComponent()->GetBaseId().IsNone());
	ARpgInventoryContainerActor* OtherBase = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(3000, 0, 0));
	ARpgInventoryContainerActor* WithdrawalLoot = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(850, 0, 0));
	WithdrawalLoot->GetContainerComponent()->SetTransferPolicy(ERpgInventoryContainerTransferPolicy::WithdrawOnly);
	ARpgInventoryAutomationTestPlayerState* OtherPlayer = Fixture.Spawn<ARpgInventoryAutomationTestPlayerState>();
	URpgInventoryContainerComponent* AccidentalPlayerContainer = NewObject<URpgInventoryContainerComponent>(OtherPlayer, NAME_None, RF_Transient);
	OtherPlayer->AddInstanceComponent(AccidentalPlayerContainer);
	AccidentalPlayerContainer->RegisterComponent();
	ARpgCraftingStationActor* Station = Fixture.Spawn<ARpgCraftingStationActor>(FVector(700, 0, 0));
	TArray<URpgInventoryManagerComponent*> Sources;
	RpgStorageAccessRules::ResolveStorageSources(Fixture.World, FVector::ZeroVector, 1, Sources);
	TestTrue(TEXT("Inside station includes far chest on another floor"), Sources.Contains(FarInside->GetInventoryManager()));
	TestTrue(TEXT("Inside station includes another station tray"), Sources.Contains(Station->GetCraftingStationComponent()->GetOutputInventory()));
	TestFalse(TEXT("Inside station excludes outside chest"), Sources.Contains(Outside->GetInventoryManager()));
	TestFalse(TEXT("Inside station excludes another base"), Sources.Contains(OtherBase->GetInventoryManager()));
	TestFalse(TEXT("Withdrawal-only loot is never a crafting source"), Sources.Contains(WithdrawalLoot->GetInventoryManager()));
	for (URpgInventoryManagerComponent* Source : Sources) TestNotEqual(TEXT("Other player inventory is excluded"), Source->GetOwner(), static_cast<AActor*>(OtherPlayer));
	RpgStorageAccessRules::ResolveStorageSources(Fixture.World, FVector(1200, 0, 0), 400, Sources);
	TestTrue(TEXT("Outside station reaches one nearby base chest"), Sources.Contains(NearInside->GetInventoryManager()));
	TestTrue(TEXT("Outside station reaches an outside chest"), Sources.Contains(Outside->GetInventoryManager()));
	TestFalse(TEXT("Outside station radius is spatial, not vertical-free"), Sources.Contains(FarInside->GetInventoryManager()));
	TestFalse(TEXT("A nearby base chest does not expose that base's distant station tray"), Sources.Contains(Station->GetCraftingStationComponent()->GetOutputInventory()));
	RpgStorageAccessRules::ResolveStorageSources(Fixture.World, FVector(1200, 0, 0), 0, Sources);
	TestTrue(TEXT("Outside zero radius has no shared sources"), Sources.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageDestinationOrderingTest,
	"SurvivalRpg.Storage.Physical.DestinationOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageDestinationOrderingTest::RunTest(const FString& Parameters)
{
	RpgPhysicalStorageRulesTests::FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Standalone test world"), Fixture.World)) return false;
	ARpgBaseCampActor* Base = Fixture.Spawn<ARpgBaseCampActor>();
	Base->SetBaseArea(FVector::ZeroVector, 1000);
	ARpgInventoryContainerActor* First = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(100, 0, 0));
	ARpgInventoryContainerActor* Second = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(200, 0, 0));
	ARpgInventoryContainerActor* General = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(300, 0, 0));
	ARpgInventoryContainerActor* Empty = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(400, 0, 0));
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	for (ARpgInventoryContainerActor* Chest : { First, Second, General, Empty }) Chest->GetContainerComponent()->EnsurePersistentContainerId();
	FRpgStorageAssignment Assignment;
	Assignment.ItemDefinition = Material;
	TestTrue(TEXT("First assignment is accepted"), First->GetContainerComponent()->SetAssignments({Assignment}));
	TestTrue(TEXT("Second assignment is accepted"), Second->GetContainerComponent()->SetAssignments({Assignment}));
	TestTrue(TEXT("Orders compare across chests"), First->GetContainerComponent()->GetAssignments()[0].AssignmentOrder < Second->GetContainerComponent()->GetAssignments()[0].AssignmentOrder);
	General->GetInventoryManager()->AddItemDefinition(Material, 10);
	TArray<URpgInventoryContainerComponent*> Targets = RpgStorageAccessRules::GetPhysicalStorageTargets(Fixture.World, FVector::ZeroVector, 0, Material);
	if (!TestEqual(TEXT("Empty general chest remains excluded"), Targets.Num(), 3)) return false;
	TestTrue(TEXT("Explicit empty destination outranks stocked general chest"), Targets[0] == First->GetContainerComponent());
	TestTrue(TEXT("Equal quantities use oldest assignment"), Targets[1] == Second->GetContainerComponent());
	Second->GetInventoryManager()->AddItemDefinition(Material, 2);
	Targets = RpgStorageAccessRules::GetPhysicalStorageTargets(Fixture.World, FVector::ZeroVector, 0, Material);
	TestTrue(TEXT("Higher stock wins within the exact assignment stage"), Targets[0] == Second->GetContainerComponent());
	TestTrue(TEXT("General chest remains after all explicit targets"), Targets.Last() == General->GetContainerComponent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageCategoryHierarchyTest,
	"SurvivalRpg.Storage.Physical.CategoryHierarchy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageCategoryHierarchyTest::RunTest(const FString& Parameters)
{
	RpgPhysicalStorageRulesTests::FScopedWorld Fixture;
	if (!TestNotNull(TEXT("Standalone test world"), Fixture.World)) return false;
	ARpgBaseCampActor* Base = Fixture.Spawn<ARpgBaseCampActor>();
	Base->SetBaseArea(FVector::ZeroVector, 1000);
	ARpgInventoryContainerActor* Exact = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(100, 0, 0));
	ARpgInventoryContainerActor* Category = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(200, 0, 0));
	ARpgInventoryContainerActor* General = Fixture.Spawn<ARpgInventoryContainerActor>(FVector(300, 0, 0));
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	URpgInventoryFragment_ItemTraits* Traits = const_cast<URpgInventoryFragment_ItemTraits*>(
		Cast<URpgInventoryFragment_ItemTraits>(GetDefault<URpgInventoryAutomationTestMaterialDefinition>()->FindFragmentByClass(URpgInventoryFragment_ItemTraits::StaticClass())));
	if (!TestNotNull(TEXT("Fixture material traits"), Traits)) return false;
	const FGameplayTagContainer PreviousTags = Traits->ItemTags;
	Traits->ItemTags.AddTag(TAG_PhysicalStorageTestWoodOak);
	FRpgStorageAssignment ExactRule;
	ExactRule.ItemDefinition = Material;
	FRpgStorageAssignment CategoryRule;
	CategoryRule.Category = TAG_PhysicalStorageTestWood;
	Exact->GetContainerComponent()->EnsurePersistentContainerId();
	Category->GetContainerComponent()->EnsurePersistentContainerId();
	General->GetContainerComponent()->EnsurePersistentContainerId();
	TestTrue(TEXT("Category slot accepts its parent tag"), Category->GetContainerComponent()->SetAssignments({CategoryRule}));
	TestTrue(TEXT("Exact slot accepts its item"), Exact->GetContainerComponent()->SetAssignments({ExactRule}));
	Category->GetInventoryManager()->AddItemDefinition(Material, 5);
	General->GetInventoryManager()->AddItemDefinition(Material, 10);
	TArray<URpgInventoryContainerComponent*> Targets = RpgStorageAccessRules::GetPhysicalStorageTargets(Fixture.World, FVector::ZeroVector, 0, Material);
	if (TestEqual(TEXT("Parent category matches descendant item tags"), Targets.Num(), 3))
	{
		TestTrue(TEXT("Exact item wins even when newer and empty"), Targets[0] == Exact->GetContainerComponent());
		TestTrue(TEXT("Category wins over fuller general storage"), Targets[1] == Category->GetContainerComponent());
	}
	TestTrue(TEXT("A mixed rule list accepts category and exact selections"), Category->GetContainerComponent()->SetAssignments({CategoryRule, ExactRule}));
	int64 RuleOrder = 0;
	TestEqual(TEXT("Mixed rule list uses its best matching priority"), Category->GetContainerComponent()->GetAssignmentRank(Material, RuleOrder), 0);
	Traits->ItemTags = PreviousTags;
	return true;
}

#endif
