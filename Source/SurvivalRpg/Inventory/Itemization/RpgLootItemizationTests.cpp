#if WITH_DEV_AUTOMATION_TESTS

#include "RpgItemizationAutomationTestTypes.h"
#include "RpgItemizationGenerator.h"
#include "RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgLootSourceAutomationTestTypes.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryItemizationFragmentViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryItemTooltipWidget.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

namespace RpgLootItemizationTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
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

		~FScopedWidgetWorld()
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

	class FScopedLegacyAffixLabels
	{
	public:
		explicit FScopedLegacyAffixLabels(URpgItemAffixPool* InPool)
			: Pool(InPool)
		{
			if (!Pool)
			{
				return;
			}

			OriginalLabels.Reserve(Pool->Affixes.Num());
			for (FRpgItemAffixDefinition& Affix : Pool->Affixes)
			{
				OriginalLabels.Add(Affix.DisplayName);
				Affix.DisplayName = FText::FromString(
					FString::Printf(TEXT("+ %s"), *Affix.DisplayName.ToString()));
			}
		}

		~FScopedLegacyAffixLabels()
		{
			if (!Pool || OriginalLabels.Num() != Pool->Affixes.Num())
			{
				return;
			}

			for (int32 Index = 0; Index < OriginalLabels.Num(); ++Index)
			{
				Pool->Affixes[Index].DisplayName = OriginalLabels[Index];
			}
		}

	private:
		TObjectPtr<URpgItemAffixPool> Pool = nullptr;
		TArray<FText> OriginalLabels;
	};

	/** Simulates a shipped profile being narrowed after concrete equipment has already been saved. */
	class FScopedIncompatibleProfileRebalance
	{
	public:
		explicit FScopedIncompatibleProfileRebalance(URpgItemizationProfile* InProfile)
			: Profile(InProfile)
			, Pool(InProfile ? InProfile->AffixPool.Get() : nullptr)
		{
			if (!Profile)
			{
				return;
			}

			OriginalMinimumItemLevel = Profile->MinimumItemLevel;
			OriginalMaximumItemLevel = Profile->MaximumItemLevel;
			OriginalBaseStats = Profile->BaseStats;
			if (Pool)
			{
				OriginalAffixes = Pool->Affixes;
			}

			Profile->MinimumItemLevel = 1;
			Profile->MaximumItemLevel = 1;
			for (FRpgItemStatRollDefinition& Definition : Profile->BaseStats)
			{
				Definition.MinimumValue = FScalableFloat(1000.0f);
				Definition.MaximumValue = FScalableFloat(2000.0f);
			}
			if (Pool)
			{
				Pool->Affixes.Reset();
			}
		}

		~FScopedIncompatibleProfileRebalance()
		{
			if (!Profile)
			{
				return;
			}
			Profile->MinimumItemLevel = OriginalMinimumItemLevel;
			Profile->MaximumItemLevel = OriginalMaximumItemLevel;
			Profile->BaseStats = MoveTemp(OriginalBaseStats);
			if (Pool)
			{
				Pool->Affixes = MoveTemp(OriginalAffixes);
			}
		}

	private:
		TObjectPtr<URpgItemizationProfile> Profile = nullptr;
		TObjectPtr<URpgItemAffixPool> Pool = nullptr;
		int32 OriginalMinimumItemLevel = 1;
		int32 OriginalMaximumItemLevel = 100;
		TArray<FRpgItemStatRollDefinition> OriginalBaseStats;
		TArray<FRpgItemAffixDefinition> OriginalAffixes;
	};

	FRpgLootEntry MakeEntry(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		int32 MinimumQuantity = 1,
		int32 MaximumQuantity = 1)
	{
		FRpgLootEntry Entry;
		Entry.ItemDefinition = ItemDefinition;
		Entry.MinimumQuantity = MinimumQuantity;
		Entry.MaximumQuantity = MaximumQuantity;
		return Entry;
	}

	bool AreRollResultsEqual(
		const FRpgLootRollResult& A,
		const FRpgLootRollResult& B)
	{
		if (A.Seed != B.Seed || A.Items.Num() != B.Items.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Items.Num(); ++Index)
		{
			const FRpgLootItemRoll& Left = A.Items[Index];
			const FRpgLootItemRoll& Right = B.Items[Index];
			if (Left.ItemDefinition != Right.ItemDefinition ||
				Left.Quantity != Right.Quantity ||
				Left.SourceLevel != Right.SourceLevel ||
				Left.ItemizationSeed != Right.ItemizationSeed)
			{
				return false;
			}
		}
		return true;
	}

	AActor* CreateInventoryOwner(
		UWorld* World,
		const TCHAR* DebugName,
		URpgInventoryManagerComponent*& OutInventory)
	{
		OutInventory = nullptr;
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			AActor::StaticClass(),
			FName(DebugName));
		SpawnParameters.ObjectFlags = RF_Transient;
		AActor* Owner = World->SpawnActor<AActor>(SpawnParameters);
		if (!Owner)
		{
			return nullptr;
		}

		OutInventory = NewObject<URpgInventoryManagerComponent>(
			Owner,
			TEXT("Inventory"),
			RF_Transient);
		Owner->AddInstanceComponent(OutInventory);
		OutInventory->RegisterComponent();
		return Owner;
	}

	bool FindEntry(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory)
		{
			return false;
		}
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	FRpgInventoryTransferIntent MakeWholeEntryTransfer(
		const FRpgInventoryEntryView& SourceEntry,
		const URpgInventoryManagerComponent* TargetInventory,
		const bool bUseExactPlacement)
	{
		FRpgInventoryTransferIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
		Intent.Quantity = SourceEntry.StackCount;
		if (TargetInventory)
		{
			Intent.TargetContainer = FRpgInventoryContainerHandle::MakeRoot(
				TargetInventory->GetDefaultContainerId());
			if (bUseExactPlacement)
			{
				Intent.TargetPlacement.SetContainerHandle(Intent.TargetContainer);
				Intent.TargetPlacement.X = 0;
				Intent.TargetPlacement.Y = 0;
			}
		}
		return Intent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootResolverDeterminismTest,
	"SurvivalRpg.Loot.Resolver.DeterminismAndWeightedUniqueness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootResolverDeterminismTest::RunTest(const FString& Parameters)
{
	URpgLootTable* Table = NewObject<URpgLootTable>();
	FRpgLootGroup& Independent = Table->Groups.AddDefaulted_GetRef();
	Independent.Mode = ERpgLootGroupMode::Independent;
	FRpgLootEntry Material = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		2,
		5);
	Material.ChancePercent = 100.0f;
	Material.bScaleQuantityWithYield = true;
	Independent.Entries.Add(Material);

	FRpgLootGroup& Weighted = Table->Groups.AddDefaulted_GetRef();
	Weighted.Mode = ERpgLootGroupMode::WeightedPick;
	Weighted.WeightedRollCount = 2;
	FRpgLootEntry Unit = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass());
	Unit.Weight = 4.0f;
	Weighted.Entries.Add(Unit);
	FRpgLootEntry Stack = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass());
	Stack.Weight = 2.0f;
	Weighted.Entries.Add(Stack);
	FRpgLootEntry OtherMaterial = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestNoTraitsItemDefinition::StaticClass());
	OtherMaterial.Weight = 1.0f;
	Weighted.Entries.Add(OtherMaterial);

	TestTrue(TEXT("The deterministic fixture is structurally valid"), Table->HasValidConfiguration());
	FRpgLootRollContext Context;
	Context.Seed = 424242;
	Context.SourceLevel = 17;
	Context.YieldMultiplier = 1.5f;

	FRpgLootRollResult First;
	FRpgLootRollResult Second;
	TestTrue(TEXT("The first seeded roll succeeds"), FRpgLootResolver::RollLoot(Table, Context, First));
	TestTrue(TEXT("The repeated seeded roll succeeds"), FRpgLootResolver::RollLoot(Table, Context, Second));
	TestTrue(TEXT("Equal seeds and context produce byte-equivalent logical rolls"), RpgLootItemizationTests::AreRollResultsEqual(First, Second));
	TestEqual(TEXT("One independent and two weighted rows succeed"), First.Items.Num(), 3);
	if (First.Items.Num() == 3)
	{
		TestTrue(TEXT("Yield scaling keeps the stochastic whole quantity in the expected range"), First.Items[0].Quantity >= 3 && First.Items[0].Quantity <= 8);
		TestEqual(TEXT("The generated-item level captures authoritative source level"), First.Items[0].SourceLevel, 17);
		TestTrue(TEXT("Weighted selection is without replacement"), First.Items[1].ItemDefinition != First.Items[2].ItemDefinition);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootResolverRareFindTest,
	"SurvivalRpg.Loot.Resolver.RareFindAndYieldClamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootResolverRareFindTest::RunTest(const FString& Parameters)
{
	URpgLootTable* Table = NewObject<URpgLootTable>();
	FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
	FRpgLootEntry Rare = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		1,
		1);
	Rare.ChancePercent = 0.5f;
	Rare.bScaleChanceWithRareFind = true;
	Group.Entries.Add(Rare);

	FRpgLootRollContext Context;
	Context.Seed = 7;
	Context.RareFindMultiplier = 1000.0f;
	FRpgLootRollResult GuaranteedResult;
	TestTrue(TEXT("A clamped rare-find roll resolves"), FRpgLootResolver::RollLoot(Table, Context, GuaranteedResult));
	TestEqual(TEXT("Multiplicative rare find clamps effective chance to 100 percent"), GuaranteedResult.Items.Num(), 1);

	Context.RareFindMultiplier = 0.0f;
	FRpgLootRollResult ImpossibleResult;
	TestTrue(TEXT("A zero rare-find roll still resolves successfully"), FRpgLootResolver::RollLoot(Table, Context, ImpossibleResult));
	TestTrue(TEXT("Zero multiplicative rare find produces no sensitive drop"), ImpossibleResult.Items.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootResolverChanceQuantityRoundingTest,
	"SurvivalRpg.Loot.Resolver.ChanceQuantityAndStochasticRounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootResolverChanceQuantityRoundingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	URpgLootTable* ChanceTable = NewObject<URpgLootTable>();
	FRpgLootGroup& Independent = ChanceTable->Groups.AddDefaulted_GetRef();
	Independent.Mode = ERpgLootGroupMode::Independent;
	Independent.GroupChancePercent = 100.0f;
	FRpgLootEntry GuaranteedFirst = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass());
	GuaranteedFirst.ChancePercent = 100.0f;
	Independent.Entries.Add(GuaranteedFirst);
	FRpgLootEntry ImpossibleMiddle = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass());
	ImpossibleMiddle.ChancePercent = 0.0f;
	Independent.Entries.Add(ImpossibleMiddle);
	FRpgLootEntry GuaranteedLast = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass());
	GuaranteedLast.ChancePercent = 100.0f;
	Independent.Entries.Add(GuaranteedLast);

	FRpgLootRollContext Context;
	Context.Seed = 112358;
	FRpgLootRollResult ChanceResult;
	TestTrue(TEXT("Independent chance fixture resolves"), FRpgLootResolver::RollLoot(ChanceTable, Context, ChanceResult));
	TestEqual(TEXT("Independent rows each evaluate their own chance"), ChanceResult.Items.Num(), 2);
	if (ChanceResult.Items.Num() == 2)
	{
		TestEqual(TEXT("First guaranteed independent row remains present"), ChanceResult.Items[0].ItemDefinition, GuaranteedFirst.ItemDefinition);
		TestEqual(TEXT("Later guaranteed row is unaffected by the failed middle row"), ChanceResult.Items[1].ItemDefinition, GuaranteedLast.ItemDefinition);
	}

	Independent.GroupChancePercent = 0.0f;
	FRpgLootRollResult GatedResult;
	TestTrue(TEXT("A zero-chance group still resolves successfully"), FRpgLootResolver::RollLoot(ChanceTable, Context, GatedResult));
	TestTrue(TEXT("The group gate suppresses every contained independent row"), GatedResult.Items.IsEmpty());

	URpgLootTable* RangeTable = NewObject<URpgLootTable>();
	FRpgLootGroup& RangeGroup = RangeTable->Groups.AddDefaulted_GetRef();
	FRpgLootEntry RangeEntry = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		5,
		7);
	RangeEntry.ChancePercent = 100.0f;
	RangeGroup.Entries.Add(RangeEntry);
	bool bObservedMinimum = false;
	bool bObservedMaximum = false;
	for (int32 Seed = 0; Seed < 256; ++Seed)
	{
		Context.Seed = Seed;
		FRpgLootRollResult RangeResult;
		if (!FRpgLootResolver::RollLoot(RangeTable, Context, RangeResult) || RangeResult.Items.Num() != 1)
		{
			AddError(TEXT("Every guaranteed quantity-range sample must resolve exactly once"));
			return false;
		}
		const int32 Quantity = RangeResult.Items[0].Quantity;
		TestTrue(TEXT("Inclusive quantity samples remain inside their authored range"), Quantity >= 5 && Quantity <= 7);
		bObservedMinimum |= Quantity == 5;
		bObservedMaximum |= Quantity == 7;
	}
	TestTrue(TEXT("Seeded range sampling reaches the inclusive minimum"), bObservedMinimum);
	TestTrue(TEXT("Seeded range sampling reaches the inclusive maximum"), bObservedMaximum);

	URpgLootTable* RoundingTable = NewObject<URpgLootTable>();
	FRpgLootGroup& RoundingGroup = RoundingTable->Groups.AddDefaulted_GetRef();
	FRpgLootEntry RoundingEntry = RpgLootItemizationTests::MakeEntry(
		URpgInventoryAutomationTestMaterialDefinition::StaticClass());
	RoundingEntry.ChancePercent = 100.0f;
	RoundingEntry.bScaleQuantityWithYield = true;
	RoundingGroup.Entries.Add(RoundingEntry);
	Context.YieldMultiplier = 1.5f;
	bool bRoundedDown = false;
	bool bRoundedUp = false;
	for (int32 Seed = 0; Seed < 256; ++Seed)
	{
		Context.Seed = Seed;
		FRpgLootRollResult First;
		FRpgLootRollResult Replay;
		if (!FRpgLootResolver::RollLoot(RoundingTable, Context, First) ||
			!FRpgLootResolver::RollLoot(RoundingTable, Context, Replay) ||
			First.Items.Num() != 1 || Replay.Items.Num() != 1)
		{
			AddError(TEXT("Every stochastic-rounding sample and replay must resolve"));
			return false;
		}
		TestEqual(TEXT("Stochastic rounding is deterministic for an explicit seed"), First.Items[0].Quantity, Replay.Items[0].Quantity);
		bRoundedDown |= First.Items[0].Quantity == 1;
		bRoundedUp |= First.Items[0].Quantity == 2;
	}
	TestTrue(TEXT("A fractional 1.5 yield can deterministically round down"), bRoundedDown);
	TestTrue(TEXT("A fractional 1.5 yield can deterministically round up"), bRoundedUp);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemizationGeneratorTest,
	"SurvivalRpg.Itemization.Generation.UniqueAffixesAndRarityWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemizationGeneratorTest::RunTest(const FString& Parameters)
{
	const URpgItemizationAutomationTestProfile* EpicProfile =
		GetDefault<URpgItemizationAutomationTestProfile>();
	TestTrue(TEXT("The deterministic Epic profile is valid"), EpicProfile->HasValidConfiguration());

	FRandomStream FirstStream(91234);
	FRandomStream SecondStream(91234);
	FRpgItemizationState FirstState;
	FRpgItemizationState SecondState;
	TestTrue(TEXT("The first itemization roll succeeds"), FRpgItemizationGenerator::GenerateItemization(EpicProfile, 42, FirstStream, FirstState));
	TestTrue(TEXT("The repeated itemization roll succeeds"), FRpgItemizationGenerator::GenerateItemization(EpicProfile, 42, SecondStream, SecondState));
	TestTrue(TEXT("An equal seed produces exactly equal item state"), FirstState == SecondState);
	TestEqual(TEXT("The source level becomes item level inside profile bounds"), FirstState.ItemLevel, 42);
	TestEqual(TEXT("The Epic fixture rolls exactly three affixes"), FirstState.Affixes.Num(), 3);
	TSet<FName> UniqueAffixIds;
	for (const FRpgRolledItemAffix& Affix : FirstState.Affixes)
	{
		UniqueAffixIds.Add(Affix.AffixId);
	}
	TestEqual(TEXT("Affix selection is without replacement"), UniqueAffixIds.Num(), 3);
	if (!FirstState.BaseStats.IsEmpty())
	{
		const FGameplayTag BaseTag = FirstState.BaseStats[0].StatTag;
		TestEqual(TEXT("Stat lookup exposes the rolled base value"), FirstState.GetBaseValueForStat(BaseTag), FirstState.BaseStats[0].Value);
		TestEqual(TEXT("Total lookup includes the same base value when no affix shares its tag"), FirstState.GetTotalValueForStat(BaseTag), FirstState.BaseStats[0].Value);
	}

	URpgItemizationProfile* DistributionProfile = NewObject<URpgItemizationProfile>();
	DistributionProfile->AffixPool = EpicProfile->AffixPool;
	TestTrue(TEXT("The default 60/28/10/2 profile is valid with a three-affix pool"), DistributionProfile->HasValidConfiguration());
	TMap<ERpgItemRarity, int32> Counts;
	FRandomStream DistributionStream(13579);
	for (int32 RollIndex = 0; RollIndex < 10000; ++RollIndex)
	{
		FRpgItemizationState State;
		if (!FRpgItemizationGenerator::GenerateItemization(
			DistributionProfile,
			1,
			DistributionStream,
			State))
		{
			AddError(TEXT("A distribution sample failed generation"));
			return false;
		}
		++Counts.FindOrAdd(State.Rarity);
	}
	TestTrue(TEXT("Common distribution stays near its 60 percent weight"), Counts.FindRef(ERpgItemRarity::Common) >= 5500 && Counts.FindRef(ERpgItemRarity::Common) <= 6500);
	TestTrue(TEXT("Uncommon distribution stays near its 28 percent weight"), Counts.FindRef(ERpgItemRarity::Uncommon) >= 2300 && Counts.FindRef(ERpgItemRarity::Uncommon) <= 3300);
	TestTrue(TEXT("Rare distribution stays near its 10 percent weight"), Counts.FindRef(ERpgItemRarity::Rare) >= 700 && Counts.FindRef(ERpgItemRarity::Rare) <= 1300);
	TestTrue(TEXT("Epic distribution stays near its 2 percent weight"), Counts.FindRef(ERpgItemRarity::Epic) >= 100 && Counts.FindRef(ERpgItemRarity::Epic) <= 300);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemizationPersistenceTest,
	"SurvivalRpg.Itemization.Instance.PersistenceCopyAndReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemizationPersistenceTest::RunTest(const FString& Parameters)
{
	FRpgLootRollResult FirstRoll;
	FirstRoll.Seed = 123;
	FRpgLootItemRoll& FirstItem = FirstRoll.Items.AddDefaulted_GetRef();
	FirstItem.ItemDefinition = URpgItemizationAutomationTestItemDefinition::StaticClass();
	FirstItem.Quantity = 1;
	FirstItem.SourceLevel = 37;
	FirstItem.ItemizationSeed = 123;
	FInventoryPickup FirstPickup;
	TestTrue(TEXT("An itemized roll materializes into a pickup"), FirstRoll.ToInventoryPickup(GetTransientPackage(), FirstPickup));
	TestEqual(TEXT("Itemized equipment becomes a concrete pickup instance"), FirstPickup.Instances.Num(), 1);
	TestTrue(TEXT("Itemized equipment does not collapse into a definition/count template"), FirstPickup.Templates.IsEmpty());
	if (FirstPickup.Instances.Num() != 1 || !FirstPickup.Instances[0].Item)
	{
		return false;
	}

	URpgInventoryItemInstance* Source = FirstPickup.Instances[0].Item;
	TestTrue(TEXT("The concrete instance owns valid generated state"), Source->GetItemizationStateRef().IsStructurallyValid() && Source->HasGeneratedItemization());
	TestEqual(TEXT("Materialization retains source level"), Source->GetItemizationStateRef().ItemLevel, 37);

	TArray<FRpgInventoryFragmentStatePayload> SavedPayloads;
	TestTrue(TEXT("Generated state exports through the existing fragment payload seam"), Source->ExportRuntimeState(SavedPayloads));
	TestTrue(TEXT("The itemization payload is present beside core inventory state"), SavedPayloads.ContainsByPredicate(
		[](const FRpgInventoryFragmentStatePayload& Payload)
		{
			return Payload.FragmentId == FName(TEXT("Inventory.Itemization.State"));
		}));

	FRpgLootRollResult SecondRoll = FirstRoll;
	SecondRoll.Items[0].ItemizationSeed = 999;
	FInventoryPickup SecondPickup;
	TestTrue(TEXT("A second concrete item materializes"), SecondRoll.ToInventoryPickup(GetTransientPackage(), SecondPickup));
	if (SecondPickup.Instances.Num() != 1 || !SecondPickup.Instances[0].Item)
	{
		return false;
	}
	URpgInventoryItemInstance* Restored = SecondPickup.Instances[0].Item;

	FRpgLootRollResult ThirdRoll = FirstRoll;
	ThirdRoll.Items[0].ItemizationSeed = 456;
	FInventoryPickup ThirdPickup;
	TestTrue(TEXT("A split/transfer target materializes"), ThirdRoll.ToInventoryPickup(GetTransientPackage(), ThirdPickup));
	if (ThirdPickup.Instances.Num() != 1 || !ThirdPickup.Instances[0].Item)
	{
		return false;
	}
	URpgInventoryItemInstance* Copied = ThirdPickup.Instances[0].Item;

	URpgItemizationProfile* MutableProfile =
		GetMutableDefault<URpgItemizationAutomationTestProfile>();
	const URpgInventoryFragment_Itemization* ItemizationFragment =
		Source->FindFragmentByClass<URpgInventoryFragment_Itemization>();
	{
		RpgLootItemizationTests::FScopedIncompatibleProfileRebalance Rebalance(
			MutableProfile);
		TestFalse(
			TEXT("The historical roll is deliberately outside the rebalanced generation profile"),
			ItemizationFragment && ItemizationFragment->IsItemizationStateCompatible(
				Source->GetItemizationStateRef()));

		TArray<FRpgInventoryFragmentStatePayload> ReExportedPayloads;
		TestTrue(
			TEXT("Existing equipment remains saveable after its generation profile changes"),
			Source->ExportRuntimeState(ReExportedPayloads));
		TestTrue(
			TEXT("A historical saved payload imports atomically after profile rebalance"),
			Restored->ImportRuntimeState(SavedPayloads));
		TestTrue(
			TEXT("Save/import preserves every historical generated roll"),
			Restored->GetItemizationStateRef() == Source->GetItemizationStateRef());
		TestTrue(
			TEXT("Fragment copying preserves historical rolls after profile rebalance"),
			Copied->CopyRuntimeStateFrom(Source, false));
		TestTrue(
			TEXT("Copied historical generated state is exact"),
			Copied->GetItemizationStateRef() == Source->GetItemizationStateRef());
	}

	const FProperty* StateProperty = FindFProperty<FProperty>(
		URpgInventoryItemInstance::StaticClass(),
		TEXT("ItemizationState"));
	if (TestNotNull(TEXT("ItemizationState remains reflected"), StateProperty))
	{
		TestTrue(TEXT("ItemizationState remains replicated"), StateProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("ItemizationState retains RepNotify invalidation"), StateProperty->HasAnyPropertyFlags(CPF_RepNotify));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemizationInventoryFlowTest,
	"SurvivalRpg.Itemization.Instance.CorpseSaveTransferWorldDropAndPickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemizationInventoryFlowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgLootItemizationTests::FScopedWidgetWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("The itemization flow world exists"), World))
	{
		return false;
	}

	URpgInventoryManagerComponent* CorpseInventory = nullptr;
	AActor* CorpseOwner = RpgLootItemizationTests::CreateInventoryOwner(
		World,
		TEXT("ItemizedCorpse"),
		CorpseInventory);
	if (!TestNotNull(TEXT("The corpse inventory owner exists"), CorpseOwner) ||
		!TestNotNull(TEXT("The corpse inventory exists"), CorpseInventory))
	{
		return false;
	}

	URpgLootTable* CorpseLootTable = NewObject<URpgLootTable>(CorpseOwner);
	FRpgLootGroup& CorpseGroup = CorpseLootTable->Groups.AddDefaulted_GetRef();
	CorpseGroup.Mode = ERpgLootGroupMode::Independent;
	CorpseGroup.GroupChancePercent = 100.0f;
	FRpgLootEntry& CorpseEntry = CorpseGroup.Entries.AddDefaulted_GetRef();
	CorpseEntry.ItemDefinition =
		URpgItemizationAutomationTestItemDefinition::StaticClass();
	CorpseEntry.MinimumQuantity = 1;
	CorpseEntry.MaximumQuantity = 1;
	CorpseEntry.ChancePercent = 100.0f;
	URpgLootSourceAutomationTestComponent* LootSource =
		NewObject<URpgLootSourceAutomationTestComponent>(
			CorpseOwner,
			TEXT("LootSource"),
			RF_Transient);
	LootSource->ConfigureLootTable(CorpseLootTable, false, 37);
	CorpseOwner->AddInstanceComponent(LootSource);
	LootSource->RegisterComponent();
	if (!CorpseOwner->HasActorBegunPlay())
	{
		CorpseOwner->DispatchBeginPlay();
	}
	LootSource->PopulateLoot();

	const TArray<URpgInventoryItemInstance*> CorpseItems =
		CorpseInventory->GetAllItems();
	if (!TestEqual(
			TEXT("Corpse loot contains one concrete generated equipment instance"),
			CorpseItems.Num(),
			1) ||
		!CorpseItems.IsValidIndex(0) ||
		!TestNotNull(TEXT("The corpse equipment instance is valid"), CorpseItems[0]))
	{
		return false;
	}
	const FRpgInventoryItemId ItemId = CorpseItems[0]->GetItemId();
	const FRpgItemizationState OriginalState =
		CorpseItems[0]->GetItemizationStateRef();
	TestTrue(
		TEXT("Corpse loot owns server-generated itemization"),
		OriginalState.bGenerated && OriginalState.IsStructurallyValid());

	const FRpgInventoryGraphSaveData SavedGraph =
		CorpseInventory->ExportInventoryGraph();
	URpgInventoryManagerComponent* RestoredInventory = nullptr;
	AActor* RestoreOwner = RpgLootItemizationTests::CreateInventoryOwner(
		World,
		TEXT("ItemizedRestore"),
		RestoredInventory);
	FRpgInventoryMutationResult RestoreResult;
	if (!TestNotNull(TEXT("The restore owner exists"), RestoreOwner) ||
		!TestNotNull(TEXT("The restore inventory exists"), RestoredInventory) ||
		!TestTrue(
			TEXT("The complete inventory graph restores atomically"),
			RestoredInventory &&
				RestoredInventory->RestoreInventoryGraph(SavedGraph, RestoreResult)))
	{
		return false;
	}
	URpgInventoryItemInstance* RestoredItem =
		RestoredInventory->FindItemById(ItemId);
	if (!TestNotNull(TEXT("Save/load preserves the concrete item identity"), RestoredItem))
	{
		return false;
	}
	TestTrue(
		TEXT("Save/load preserves every rolled equipment value"),
		RestoredItem->GetItemizationStateRef() == OriginalState);

	URpgInventoryManagerComponent* TransferInventory = nullptr;
	AActor* TransferOwner = RpgLootItemizationTests::CreateInventoryOwner(
		World,
		TEXT("ItemizedTransfer"),
		TransferInventory);
	FRpgInventoryEntryView RestoredEntry;
	if (!TestNotNull(TEXT("The transfer owner exists"), TransferOwner) ||
		!TestNotNull(TEXT("The transfer inventory exists"), TransferInventory) ||
		!TestTrue(
			TEXT("The restored entry resolves by item identity"),
			RpgLootItemizationTests::FindEntry(
				RestoredInventory,
				ItemId,
				RestoredEntry)))
	{
		return false;
	}
	const FRpgInventoryMutationResult TransferResult =
		RestoredInventory->TransferItem(
			TransferInventory,
			RpgLootItemizationTests::MakeWholeEntryTransfer(
				RestoredEntry,
				TransferInventory,
				true));
	if (!TestTrue(TEXT("Cross-inventory transfer succeeds"), TransferResult.IsSuccess()))
	{
		return false;
	}
	URpgInventoryItemInstance* TransferredItem =
		TransferInventory->FindItemById(ItemId);
	if (!TestNotNull(TEXT("Transfer preserves the persistent item identity"), TransferredItem))
	{
		return false;
	}
	TestTrue(
		TEXT("Transfer preserves every rolled equipment value"),
		TransferredItem->GetItemizationStateRef() == OriginalState);

	FActorSpawnParameters DropSpawnParameters;
	DropSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgDroppedInventoryActor::StaticClass(),
		TEXT("ItemizedWorldDrop"));
	DropSpawnParameters.ObjectFlags = RF_Transient;
	DropSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgDroppedInventoryActor* WorldDrop =
		World->SpawnActor<ARpgDroppedInventoryActor>(DropSpawnParameters);
	FRpgInventoryEntryView TransferredEntry;
	if (!TestNotNull(TEXT("The world-drop actor exists"), WorldDrop) ||
		!TestTrue(
			TEXT("The transferred entry resolves before dropping"),
			RpgLootItemizationTests::FindEntry(
				TransferInventory,
				ItemId,
				TransferredEntry)))
	{
		return false;
	}
	const FRpgInventoryMutationResult DropResult =
		WorldDrop->TransferItemFromInventoryByIntent(
			TransferInventory,
			RpgLootItemizationTests::MakeWholeEntryTransfer(
				TransferredEntry,
				WorldDrop->GetLootInventoryManager(),
				false),
			true);
	if (!TestTrue(TEXT("Dropping generated equipment succeeds"), DropResult.IsSuccess()))
	{
		return false;
	}
	URpgInventoryManagerComponent* DropInventory =
		WorldDrop->GetLootInventoryManager();
	URpgInventoryItemInstance* DroppedItem =
		DropInventory ? DropInventory->FindItemById(ItemId) : nullptr;
	if (!TestNotNull(TEXT("The canonical world drop owns the same item identity"), DroppedItem))
	{
		return false;
	}
	TestTrue(
		TEXT("World-drop reconstruction preserves every roll"),
		DroppedItem->GetItemizationStateRef() == OriginalState);

	URpgInventoryManagerComponent* PickupInventory = nullptr;
	AActor* PickupOwner = RpgLootItemizationTests::CreateInventoryOwner(
		World,
		TEXT("ItemizedPickup"),
		PickupInventory);
	FRpgInventoryEntryView DroppedEntry;
	if (!TestNotNull(TEXT("The pickup owner exists"), PickupOwner) ||
		!TestNotNull(TEXT("The pickup inventory exists"), PickupInventory) ||
		!TestTrue(
			TEXT("The dropped entry resolves before collection"),
			RpgLootItemizationTests::FindEntry(
				DropInventory,
				ItemId,
				DroppedEntry)))
	{
		return false;
	}
	const FRpgInventoryMutationResult PickupResult = DropInventory->PickupItem(
		PickupInventory,
		RpgLootItemizationTests::MakeWholeEntryTransfer(
			DroppedEntry,
			PickupInventory,
			true),
		false);
	if (!TestTrue(TEXT("Picking up generated equipment succeeds"), PickupResult.IsSuccess()))
	{
		return false;
	}
	URpgInventoryItemInstance* PickedUpItem =
		PickupInventory->FindItemById(ItemId);
	if (!TestNotNull(TEXT("Pickup preserves the persistent item identity"), PickedUpItem))
	{
		return false;
	}
	TestTrue(
		TEXT("Pickup preserves every rolled equipment value"),
		PickedUpItem->GetItemizationStateRef() == OriginalState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgItemizationPresentationTest,
	"SurvivalRpg.Itemization.UI.RarityLevelOrderedRowsAndLiveRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgItemizationPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const URpgItemizationAutomationTestProfile* Profile =
		GetDefault<URpgItemizationAutomationTestProfile>();
	RpgLootItemizationTests::FScopedLegacyAffixLabels LegacyAffixLabels(
		Profile ? const_cast<URpgItemAffixPool*>(Profile->AffixPool.Get()) : nullptr);
	FRpgLootRollResult PresentationRoll;
	PresentationRoll.Seed = 8128;
	FRpgLootItemRoll& PresentationItem =
		PresentationRoll.Items.AddDefaulted_GetRef();
	PresentationItem.ItemDefinition =
		URpgItemizationAutomationTestItemDefinition::StaticClass();
	PresentationItem.Quantity = 1;
	PresentationItem.SourceLevel = 27;
	PresentationItem.ItemizationSeed = 8128;
	FInventoryPickup PresentationPickup;
	if (!TestTrue(
			TEXT("Presentation fixture materializes a concrete generated item"),
			PresentationRoll.ToInventoryPickup(
				GetTransientPackage(),
				PresentationPickup)) ||
		PresentationPickup.Instances.Num() != 1 ||
		!PresentationPickup.Instances[0].Item)
	{
		return false;
	}

	URpgInventoryItemInstance* Item =
		PresentationPickup.Instances[0].Item;
	const FRpgItemizationState InitialState = Item->GetItemizationStateRef();
	TestTrue(TEXT("Presentation fixture owns generated state"), InitialState.bGenerated);

	FRpgInventoryEntryView Entry;
	Entry.Instance = Item;
	Entry.ItemId = Item->GetItemId();
	Entry.StackCount = 1;
	URpgInventoryEntryViewModel* EntryViewModel =
		NewObject<URpgInventoryEntryViewModel>();
	const TMap<
		TSubclassOf<URpgInventoryItemFragment>,
		TSubclassOf<URpgInventoryFragmentViewModel>> NoAdditionalPresenters;
	EntryViewModel->InitializeFromEntry(Entry, NoAdditionalPresenters);
	URpgInventoryItemizationFragmentViewModel* ItemizationViewModel =
		EntryViewModel->GetItemizationViewModel();
	if (!TestNotNull(TEXT("Itemization fragment automatically creates its read-only presenter"), ItemizationViewModel))
	{
		return false;
	}

	TestTrue(TEXT("Presenter identifies generated equipment"), ItemizationViewModel->IsGenerated());
	TestEqual(TEXT("Presenter exposes server-authored item level"), ItemizationViewModel->GetItemLevel(), 27);
	TestEqual(TEXT("Presenter exposes server-authored rarity"), ItemizationViewModel->GetRarity(), InitialState.Rarity);
	TestFalse(TEXT("Presenter exposes a localized rarity label"), ItemizationViewModel->GetRarityLabel().IsEmpty());
	const TArray<FRpgItemizationDisplayRow>& InitialRows = ItemizationViewModel->GetStatRows();
	TestEqual(
		TEXT("Presenter emits one row per base stat and affix"),
		InitialRows.Num(),
		InitialState.BaseStats.Num() + InitialState.Affixes.Num());
	for (int32 Index = 0; Index < InitialState.BaseStats.Num() && InitialRows.IsValidIndex(Index); ++Index)
	{
		TestFalse(*FString::Printf(TEXT("Row %d remains an ordered base stat"), Index), InitialRows[Index].bAffix);
		TestEqual(TEXT("Base-stat row preserves generated order"), InitialRows[Index].StatTag, InitialState.BaseStats[Index].StatTag);
	}
	for (int32 AffixIndex = 0; AffixIndex < InitialState.Affixes.Num(); ++AffixIndex)
	{
		const int32 RowIndex = InitialState.BaseStats.Num() + AffixIndex;
		if (InitialRows.IsValidIndex(RowIndex))
		{
			TestTrue(*FString::Printf(TEXT("Row %d remains an ordered affix"), RowIndex), InitialRows[RowIndex].bAffix);
			TestEqual(TEXT("Affix row preserves generated order"), InitialRows[RowIndex].StatTag, InitialState.Affixes[AffixIndex].StatTag);
			TestFalse(
				TEXT("Legacy affix labels never duplicate the tooltip's numeric plus prefix"),
				InitialRows[RowIndex].Label.ToString().TrimStart().StartsWith(TEXT("+")));
		}
	}

	FRandomStream UpdatedStream(16384);
	FRpgItemizationState UpdatedState;
	TestTrue(
		TEXT("A second valid state is generated for live refresh"),
		FRpgItemizationGenerator::GenerateItemization(Profile, 63, UpdatedStream, UpdatedState));
	TestTrue(TEXT("Replicated-state seam accepts the updated roll"), Item->ApplyItemizationState(UpdatedState));
	TestEqual(TEXT("Bound presenter refreshes item level without replacement"), ItemizationViewModel->GetItemLevel(), 63);
	TestEqual(TEXT("Bound presenter refreshes rarity without replacement"), ItemizationViewModel->GetRarity(), UpdatedState.Rarity);
	TestEqual(
		TEXT("Bound presenter refreshes every ordered row"),
		ItemizationViewModel->GetStatRows().Num(),
		UpdatedState.BaseStats.Num() + UpdatedState.Affixes.Num());

	RpgLootItemizationTests::FScopedWidgetWorld WidgetWorld;
	if (!TestNotNull(TEXT("A real UMG test world is available"), WidgetWorld.GetWorld()))
	{
		return false;
	}
	URpgInventoryItemTooltipWidget* Tooltip =
		CreateWidget<URpgInventoryItemTooltipWidget>(
			WidgetWorld.GetWorld(),
			URpgInventoryItemTooltipWidget::StaticClass());
	if (!TestNotNull(TEXT("Tooltip is created through the production UMG lifecycle"), Tooltip))
	{
		return false;
	}
	TSharedPtr<SWidget> TooltipSlate = Tooltip->TakeWidget();
	TestTrue(TEXT("Native fallback tooltip builds a Slate hierarchy"), TooltipSlate.IsValid());
	Tooltip->SetItemInstance(Item, 1);
	TestTrue(TEXT("Tooltip binds a concrete generated item"), Tooltip->HasItem());
	URpgInventoryItemizationFragmentViewModel* TooltipItemization =
		Tooltip->GetItemizationViewModel();
	if (TestNotNull(TEXT("Tooltip exposes its itemization presenter"), TooltipItemization))
	{
		TestEqual(TEXT("Tooltip exposes the refreshed item level"), TooltipItemization->GetItemLevel(), 63);
	}
	Tooltip->ClearItem();
	TestFalse(TEXT("Pooled tooltip clears all item state"), Tooltip->HasItem());
	TestNull(TEXT("Pooled tooltip releases its itemization presenter"), Tooltip->GetItemizationViewModel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgLootItemizationValidationTest,
	"SurvivalRpg.Loot.DataValidation.RuntimeContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgLootItemizationValidationTest::RunTest(const FString& Parameters)
{
	URpgLootTable* InvalidTable = NewObject<URpgLootTable>();
	FRpgLootGroup& Weighted = InvalidTable->Groups.AddDefaulted_GetRef();
	Weighted.Mode = ERpgLootGroupMode::WeightedPick;
	Weighted.WeightedRollCount = 2;
	FRpgLootEntry InvalidEntry;
	InvalidEntry.MinimumQuantity = 5;
	InvalidEntry.MaximumQuantity = 2;
	InvalidEntry.Weight = 0.0f;
	Weighted.Entries.Add(InvalidEntry);
	FString TableError;
	TestFalse(TEXT("Malformed weighted tables fail closed"), InvalidTable->HasValidConfiguration(&TableError));
	TestFalse(TEXT("Malformed table validation reports a useful reason"), TableError.IsEmpty());

	URpgItemAffixPool* InvalidPool = NewObject<URpgItemAffixPool>();
	InvalidPool->Affixes =
		GetDefault<URpgItemizationAutomationTestProfile>()->AffixPool->Affixes;
	InvalidPool->Affixes[1].AffixId = InvalidPool->Affixes[0].AffixId;
	FString PoolError;
	TestFalse(TEXT("Duplicate affix ids fail closed"), InvalidPool->HasValidConfiguration(&PoolError));
	TestTrue(TEXT("Duplicate-affix validation identifies the id contract"), PoolError.Contains(TEXT("AffixId")));

	URpgItemAffixPool* UnsupportedStatPool = NewObject<URpgItemAffixPool>();
	FRpgItemAffixDefinition& UnsupportedAffix = UnsupportedStatPool->Affixes.AddDefaulted_GetRef();
	UnsupportedAffix.AffixId = TEXT("Unsupported.Stat");
	UnsupportedAffix.StatTag = RpgGameplayTags::Weapon_Attack_Primary;
	UnsupportedAffix.Weight = 1.0f;
	UnsupportedAffix.MinimumValue = FScalableFloat(1.0f);
	UnsupportedAffix.MaximumValue = FScalableFloat(2.0f);
	FString UnsupportedStatError;
	TestFalse(TEXT("Registered tags outside the supported item-stat catalog fail closed"), UnsupportedStatPool->HasValidConfiguration(&UnsupportedStatError));
	TestTrue(TEXT("Unsupported-stat validation reports the catalog contract"), UnsupportedStatError.Contains(TEXT("supported Item.Stat")));

	URpgItemizationProfile* UnsupportedLevelProfile = NewObject<URpgItemizationProfile>();
	UnsupportedLevelProfile->AffixPool =
		GetDefault<URpgItemizationAutomationTestProfile>()->AffixPool;
	UnsupportedLevelProfile->MaximumItemLevel = 101;
	FString UnsupportedLevelError;
	TestFalse(
		TEXT("Profiles cannot generate outside the supported item-level range"),
		UnsupportedLevelProfile->HasValidConfiguration(&UnsupportedLevelError));
	TestTrue(
		TEXT("Unsupported-level validation reports the 1..100 contract"),
		UnsupportedLevelError.Contains(TEXT("1..100")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
