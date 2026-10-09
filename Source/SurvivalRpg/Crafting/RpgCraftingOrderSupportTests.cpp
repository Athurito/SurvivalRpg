#include "RpgCraftingCapacity.h"
#include "RpgCraftingCategoryCatalog.h"
#include "RpgCraftingRecipeDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Inventory/Itemization/RpgItemizationAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "UObject/StrongObjectPtr.h"

namespace RpgCraftingOrderSupportTests
{
	class FScopedChestWorld
	{
	public:
		FScopedChestWorld()
		{
			GameInstance.Reset(NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient));
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedChestWorld()
		{
			GameInstance->Shutdown();
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		URpgInventoryManagerComponent* CreateChest(int32 Width, int32 Height) const
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARpgInventoryContainerActor* Chest = World ? World->SpawnActor<ARpgInventoryContainerActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters) : nullptr;
			if (!Chest)
			{
				return nullptr;
			}
			Chest->GetContainerComponent()->EnsurePersistentContainerId();
			FRpgInventoryGridSize Grid;
			Grid.Width = Width;
			Grid.Height = Height;
			Chest->GetInventoryManager()->SetDefaultGridSize(Grid);
			return Chest->GetInventoryManager();
		}

		TStrongObjectPtr<UGameInstance> GameInstance;
		UWorld* World = nullptr;
	};

	FRpgInventoryBatchOperation MakeGrant(URpgInventoryManagerComponent* Target, TSubclassOf<URpgInventoryItemDefinition> Definition,
		int32 Quantity, int32 Level, int32 Seed)
	{
		FRpgInventoryBatchOperation Operation;
		Operation.TargetInventory = Target;
		Operation.ItemDefinition = Definition;
		Operation.Quantity = Quantity;
		Operation.ItemizationSourceLevel = Level;
		Operation.ItemizationSeed = Seed;
		return Operation;
	}

	TArray<FRpgItemizationState> CollectStates(const URpgInventoryManagerComponent* Inventory)
	{
		TArray<FRpgItemizationState> States;
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance)
			{
				States.Add(Entry.Instance->GetItemizationState());
			}
		}
		return States;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBatchGrantItemizationTest,
	"SurvivalRpg.Inventory.PhysicalStorage.BatchGrantItemization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBatchGrantItemizationTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingOrderSupportTests;
	FScopedChestWorld Fixture;
	URpgInventoryManagerComponent* First = Fixture.CreateChest(4, 4);
	URpgInventoryManagerComponent* Second = Fixture.CreateChest(4, 4);
	if (!TestNotNull(TEXT("First chest exists"), First) || !TestNotNull(TEXT("Second chest exists"), Second))
	{
		return false;
	}
	const TSubclassOf<URpgInventoryItemDefinition> Itemized = URpgItemizationAutomationTestItemDefinition::StaticClass();

	ERpgInventoryMutationResultCode Code;
	TestTrue(TEXT("A rolled grant passes the dry run"), First->CanApplyInventoryBatch({ MakeGrant(First, Itemized, 3, 5, 42) }, Code));
	TestEqual(TEXT("The dry run stores nothing"), First->GetUsedEntryCount(), 0);
	TestTrue(TEXT("A rolled grant commits"), First->ApplyInventoryBatch({ MakeGrant(First, Itemized, 3, 5, 42) }, FGuid::NewGuid()).IsSuccess());
	TestTrue(TEXT("The same seed rolls the same pieces"), Second->ApplyInventoryBatch({ MakeGrant(Second, Itemized, 3, 5, 42) }, FGuid::NewGuid()).IsSuccess());

	const TArray<FRpgItemizationState> FirstStates = CollectStates(First);
	const TArray<FRpgItemizationState> SecondStates = CollectStates(Second);
	if (!TestEqual(TEXT("Every piece is its own entry"), FirstStates.Num(), 3))
	{
		return false;
	}
	for (int32 Index = 0; Index < FirstStates.Num(); ++Index)
	{
		TestTrue(TEXT("Every piece is rolled"), FirstStates[Index].bGenerated);
		TestTrue(TEXT("Equal seeds give equal pieces"), SecondStates.IsValidIndex(Index) && FirstStates[Index] == SecondStates[Index]);
	}
	TestTrue(TEXT("One stream rolls different pieces"), FirstStates[0] != FirstStates[1] || FirstStates[1] != FirstStates[2]);

	URpgInventoryManagerComponent* Other = Fixture.CreateChest(4, 4);
	TestFalse(TEXT("A roll on a definition without a profile is rejected"),
		Other->CanApplyInventoryBatch({ MakeGrant(Other, URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1, 5, 1) }, Code));
	FRpgInventoryBatchOperation Transfer = MakeGrant(Other, Itemized, 1, 5, 1);
	Transfer.SourceInventory = First;
	Transfer.ItemId = First->GetAllEntries()[0].ItemId;
	Transfer.ItemDefinition = nullptr;
	TestFalse(TEXT("A roll on a transfer is rejected"), Other->CanApplyInventoryBatch({ Transfer }, Code));
	TestTrue(TEXT("Without a level the definition grants unrolled"),
		Other->ApplyInventoryBatch({ MakeGrant(Other, Itemized, 1, 0, 0) }, FGuid::NewGuid()).IsSuccess() &&
		!CollectStates(Other)[0].bGenerated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemizationBaseStatRangesTest,
	"SurvivalRpg.Inventory.Itemization.BaseStatRangesAtLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemizationBaseStatRangesTest::RunTest(const FString& Parameters)
{
	const URpgItemizationProfile* Profile = GetDefault<URpgItemizationAutomationTestProfile>();
	TArray<FRpgItemStatRange> Ranges;
	if (!TestTrue(TEXT("A valid profile reports its ranges"), Profile->GetBaseStatRanges(10, Ranges)) ||
		!TestEqual(TEXT("One range per base stat"), Ranges.Num(), Profile->BaseStats.Num()))
	{
		return false;
	}
	const int32 ItemLevel = Profile->ResolveItemLevel(10);
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		TestEqual(TEXT("Ranges keep the base-stat order"), Ranges[Index].StatTag, Profile->BaseStats[Index].StatTag);
		TestEqual(TEXT("The lower bound is evaluated at the resolved level"), Ranges[Index].MinValue, Profile->BaseStats[Index].MinimumValue.GetValueAtLevel(ItemLevel));
		TestEqual(TEXT("The upper bound is evaluated at the resolved level"), Ranges[Index].MaxValue, Profile->BaseStats[Index].MaximumValue.GetValueAtLevel(ItemLevel));
		TestTrue(TEXT("Ranges are ordered"), Ranges[Index].MinValue <= Ranges[Index].MaxValue);
		TestFalse(TEXT("Every stat has a display name"), GetRpgItemStatDisplayName(Ranges[Index].StatTag).IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingCapacityTest,
	"SurvivalRpg.Crafting.Capacity.UnitsThatFit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCapacityTest::RunTest(const FString& Parameters)
{
	using namespace RpgCraftingOrderSupportTests;
	FScopedChestWorld Fixture;
	URpgInventoryManagerComponent* Chest = Fixture.CreateChest(2, 2);
	if (!TestNotNull(TEXT("Chest exists"), Chest))
	{
		return false;
	}
	URpgCraftingRecipeDefinition* Units = NewObject<URpgCraftingRecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	FRpgCraftingOutputItem& UnitOutput = Units->OutputItems.AddDefaulted_GetRef();
	UnitOutput.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	UnitOutput.Count = 1;

	FRpgCraftingStorageCapacity Capacity = RpgCraftingCapacity::Evaluate(*Chest, *Units, 10);
	TestEqual(TEXT("An empty 2 x 2 chest has four cells"), Capacity.TotalCells, 4);
	TestEqual(TEXT("An empty chest uses none"), Capacity.UsedCells, 0);
	TestEqual(TEXT("Four single items fit"), Capacity.UnitsThatFit, 4);
	TestEqual(TEXT("Single items do not stack"), Capacity.MaxStackSize, 1);
	TestTrue(TEXT("The footprint is reported"), Capacity.Footprint.Width == 1 && Capacity.Footprint.Height == 1);

	Chest->AddItemDefinition(URpgInventoryAutomationTestStackItemDefinition::StaticClass(), 1);
	Capacity = RpgCraftingCapacity::Evaluate(*Chest, *Units, 10);
	TestEqual(TEXT("An occupied cell is used"), Capacity.UsedCells, 1);
	TestEqual(TEXT("Three single items fit beside it"), Capacity.UnitsThatFit, 3);
	TestEqual(TEXT("The wanted count caps the answer"), RpgCraftingCapacity::Evaluate(*Chest, *Units, 2).UnitsThatFit, 2);

	URpgCraftingRecipeDefinition* Stacks = NewObject<URpgCraftingRecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	FRpgCraftingOutputItem& StackOutput = Stacks->OutputItems.AddDefaulted_GetRef();
	StackOutput.ItemDefinition = URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	StackOutput.Count = 5;
	Capacity = RpgCraftingCapacity::Evaluate(*Chest, *Stacks, 99);
	TestEqual(TEXT("Stackables count what is already present"), Capacity.PresentCount, 1);
	TestEqual(TEXT("Stackables report their stack size"), Capacity.MaxStackSize, 10);
	TestEqual(TEXT("Stackables merge before taking new cells: 9 + 3 x 10 pieces hold seven units of five"), Capacity.UnitsThatFit, 7);
	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingCategoryCatalogValidationTest,
	"SurvivalRpg.Crafting.CategoryCatalog.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingCategoryCatalogValidationTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Materials = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Materials"));
	const FGameplayTag Wood = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Category.Materials.Wood"));
	TestEqual(TEXT("A subcategory resolves to its group"), URpgCraftingCategoryCatalog::ResolveCategoryGroup(Wood), Materials);
	TestEqual(TEXT("A group resolves to itself"), URpgCraftingCategoryCatalog::ResolveCategoryGroup(Materials), Materials);
	TestFalse(TEXT("A foreign tag has no group"), URpgCraftingCategoryCatalog::ResolveCategoryGroup(FGameplayTag::RequestGameplayTag(TEXT("Crafting.Station.Kiln"))).IsValid());
	TestEqual(TEXT("Tiers use roman numerals"), URpgCraftingCategoryCatalog::MakeTierNumeral(4).ToString(), FString(TEXT("IV")));
	TestEqual(TEXT("Fallback names use the last tag segment"), URpgCraftingCategoryCatalog::MakeFallbackCategoryName(Wood).ToString(), FString(TEXT("Wood")));

	URpgCraftingCategoryCatalog* Catalog = NewObject<URpgCraftingCategoryCatalog>(GetTransientPackage(), NAME_None, RF_Transient);
	Catalog->Categories.AddDefaulted_GetRef().Category = Materials;
	Catalog->Categories.AddDefaulted_GetRef().Category = Wood;
	Catalog->Tiers.AddDefaulted_GetRef().Tier = 1;
	{
		FDataValidationContext Context;
		TestEqual(TEXT("Groups, subcategories and tiers validate"), Catalog->IsDataValid(Context), EDataValidationResult::Valid);
	}
	Catalog->Categories.AddDefaulted_GetRef().Category = Wood;
	{
		FDataValidationContext Context;
		TestEqual(TEXT("A duplicate category is rejected"), Catalog->IsDataValid(Context), EDataValidationResult::Invalid);
	}
	Catalog->Categories.Pop();
	Catalog->Categories.AddDefaulted_GetRef().Category = FGameplayTag::RequestGameplayTag(TEXT("Crafting.Station.Kiln"));
	{
		FDataValidationContext Context;
		TestEqual(TEXT("A tag outside the category root is rejected"), Catalog->IsDataValid(Context), EDataValidationResult::Invalid);
	}
	Catalog->Categories.Pop();
	Catalog->Tiers.AddDefaulted_GetRef().Tier = 1;
	{
		FDataValidationContext Context;
		TestEqual(TEXT("A duplicate tier is rejected"), Catalog->IsDataValid(Context), EDataValidationResult::Invalid);
	}
	return true;
}

#endif // WITH_EDITOR

#endif
