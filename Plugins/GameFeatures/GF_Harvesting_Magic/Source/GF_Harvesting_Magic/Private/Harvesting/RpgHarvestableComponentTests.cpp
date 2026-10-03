#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestableComponentTests
{
	using RpgHarvestAutomation::FScopedTestWorld;

	constexpr int32 YieldPerSection = 2;
	constexpr int32 ExperiencePerSection = 10;

	URpgHarvestProfile* MakeSectionedProfile(
		UObject* Outer,
		const int32 SectionCount,
		const float RespawnSeconds)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgHarvestAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = YieldPerSection;
		Entry.MaximumQuantity = YieldPerSection;
		Entry.ChancePercent = 100.0f;

		Profile->LootTable = Table;
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = ExperiencePerSection;
		Profile->SectionCount = SectionCount;
		Profile->MinimumRespawnSeconds = RespawnSeconds;
		Profile->MaximumRespawnSeconds = RespawnSeconds;
		return Profile;
	}

	/** Spawns a node, assigns its protected designer profile, binds an optional listener, then begins play. */
	ARpgHarvestAutomationNodeActor* SpawnNode(
		UWorld* World,
		URpgHarvestProfile* Profile,
		URpgHarvestAutomationNodeStateListener* Listener = nullptr)
	{
		if (!World)
		{
			return nullptr;
		}

		ARpgHarvestAutomationNodeActor* Node = World->SpawnActorDeferred<ARpgHarvestAutomationNodeActor>(
			ARpgHarvestAutomationNodeActor::StaticClass(),
			FTransform::Identity,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		FObjectProperty* ProfileProperty = FindFProperty<FObjectProperty>(
			URpgHarvestableComponent::StaticClass(),
			TEXT("HarvestProfile"));
		if (!Node || !Node->HarvestableNode || !ProfileProperty)
		{
			return nullptr;
		}

		ProfileProperty->SetObjectPropertyValue_InContainer(Node->HarvestableNode, Profile);
		if (Listener)
		{
			Node->HarvestableNode->OnHarvestStateChanged.AddDynamic(
				Listener,
				&URpgHarvestAutomationNodeStateListener::HandleStateChanged);
		}

		Node->FinishSpawning(FTransform::Identity);
		if (!Node->HasActorBegunPlay())
		{
			Node->DispatchBeginPlay();
		}
		return Node;
	}

	ARpgHarvestAutomationTestPlayerState* SpawnHarvester(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			ARpgHarvestAutomationTestPlayerState::StaticClass(),
			TEXT("NodeHarvesterState"));
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(SpawnParameters);
	}

	/** Builds a request the way an ability does: it observes the revision before committing. */
	FRpgHarvestRequest MakeRequest(
		ARpgHarvestAutomationNodeActor* Node,
		AActor* Harvester,
		const int32 RequestedSections = 1)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		Request.Hit = FHitResult(Node, nullptr, FVector(0.0, 0.0, 50.0), FVector::UpVector);
		Request.ExpectedRevision = Node && Node->HarvestableNode
			? IRpgHarvestableTarget::Execute_GetHarvestRevision(Node->HarvestableNode, Request.Hit)
			: INDEX_NONE;
		return Request;
	}

	int32 CountMaterial(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgInventoryManagerComponent* Inventory =
			Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
		return Inventory
			? Inventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass())
			: 0;
	}

	float GetMiningXP(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgTradeSkillProgressionComponent* TradeSkills =
			Harvester ? Harvester->GetTradeSkillProgressionComponent() : nullptr;
		return TradeSkills
			? TradeSkills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Mining)
			: 0.0f;
	}

	TArray<ARpgDroppedInventoryActor*> GetWorldDrops(UWorld* World)
	{
		TArray<ARpgDroppedInventoryActor*> Drops;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				Drops.Add(*It);
			}
		}
		return Drops;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeSectionsExactlyOnceTest,
	"SurvivalRpg.Harvesting.Node.SectionsExtractExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeSectionsExactlyOnceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeSectionedProfile(World, 4, 0.0f));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Sectioned node exists"), Node) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgHarvestableComponent* Component = Node->HarvestableNode;
	TestTrue(TEXT("Standalone node has authority"), Node->HasAuthority());
	TestEqual(TEXT("Node starts with its complete stock"), Component->GetRemainingSections(), 4);
	TestEqual(TEXT("Section count comes from the profile"), Component->GetSectionCount(), 4);

	for (int32 Hit = 1; Hit <= 4; ++Hit)
	{
		const FRpgHarvestResult Result = Component->CommitHarvest_Implementation(MakeRequest(Node, Harvester));
		TestTrue(*FString::Printf(TEXT("Valid hit %d on remaining stock is harvested"), Hit), Result.IsSuccess());
		TestEqual(*FString::Printf(TEXT("Hit %d extracts exactly one section"), Hit), Result.SectionsTaken, 1);
		TestEqual(*FString::Printf(TEXT("Hit %d reports the stock left"), Hit), Result.RemainingSections, 4 - Hit);
		TestEqual(*FString::Printf(TEXT("Hit %d grants its material immediately"), Hit), CountMaterial(Harvester), Hit * YieldPerSection);
		TestTrue(
			*FString::Printf(TEXT("Hit %d awards XP once per section"), Hit),
			FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(Hit * ExperiencePerSection)));
		TestEqual(*FString::Printf(TEXT("Component stock matches hit %d"), Hit), Component->GetRemainingSections(), 4 - Hit);
		TestEqual(
			*FString::Printf(TEXT("Revision changes only on depletion (hit %d)"), Hit),
			Component->GetHarvestState().Revision,
			Hit < 4 ? 0 : 1);
		TestTrue(*FString::Printf(TEXT("Only the last hit depletes (hit %d)"), Hit), Result.bDepleted == (Hit == 4));
	}

	TestFalse(TEXT("A depleted node is not harvestable"), Component->IsHarvestable());
	const FRpgHarvestResult EmptyResult = Component->CommitHarvest_Implementation(MakeRequest(Node, Harvester));
	TestTrue(TEXT("A hit on the empty node is reported as Depleted"), EmptyResult.Outcome == ERpgHarvestOutcome::Depleted);
	TestEqual(TEXT("The empty node grants nothing more"), CountMaterial(Harvester), 4 * YieldPerSection);
	TestEqual(TEXT("Remaining stock never becomes negative"), Component->GetRemainingSections(), 0);
	TestTrue(
		TEXT("The empty node awards no further XP"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(4 * ExperiencePerSection)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeMultiSectionBatchTest,
	"SurvivalRpg.Harvesting.Node.MultiSectionRequestClampsAndBatches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeMultiSectionBatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeSectionedProfile(World, 4, 0.0f));
	ARpgHarvestAutomationNodeActor* OverflowNode = SpawnNode(World, MakeSectionedProfile(World, 4, 0.0f));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	ARpgHarvestAutomationTestPlayerState* FullHarvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Batch node exists"), Node) ||
		!TestNotNull(TEXT("Overflow node exists"), OverflowNode) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester) ||
		!TestNotNull(TEXT("Full harvester exists"), FullHarvester))
	{
		return false;
	}

	const FRpgHarvestRequest TripleRequest = MakeRequest(Node, Harvester, 3);
	const FRpgHarvestResult Preview = Node->HarvestableNode->EvaluateHarvest_Implementation(TripleRequest);
	TestTrue(TEXT("Evaluation previews a successful multi-section harvest"), Preview.IsSuccess());
	TestEqual(TEXT("Evaluation reports the sections it would take"), Preview.SectionsTaken, 3);
	TestEqual(TEXT("Evaluation does not change the stock"), Node->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("Evaluation grants nothing"), CountMaterial(Harvester), 0);

	const FRpgHarvestResult First = Node->HarvestableNode->CommitHarvest_Implementation(TripleRequest);
	TestTrue(TEXT("A three-section request is harvested"), First.IsSuccess());
	TestEqual(TEXT("It extracts three sections"), First.SectionsTaken, 3);
	TestEqual(TEXT("One section remains"), First.RemainingSections, 1);
	TestTrue(TEXT("The batch reaches the inventory"), First.Delivery == ERpgHarvestDelivery::Inventory);
	TestEqual(TEXT("Three sections yield three section rewards"), CountMaterial(Harvester), 3 * YieldPerSection);
	TestTrue(
		TEXT("Three sections award three XP grants"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(3 * ExperiencePerSection)));

	const FRpgHarvestResult Second = Node->HarvestableNode->CommitHarvest_Implementation(MakeRequest(Node, Harvester, 3));
	TestTrue(TEXT("A request larger than the remaining stock is still harvested"), Second.IsSuccess());
	TestEqual(TEXT("It is clamped to the remaining section"), Second.SectionsTaken, 1);
	TestTrue(TEXT("The clamped request depletes the node"), Second.bDepleted);
	TestEqual(TEXT("The total yield equals the defined stock"), CountMaterial(Harvester), 4 * YieldPerSection);

	URpgInventoryManagerComponent* FullInventory = FullHarvester->GetInventoryManagerComponent();
	FullInventory->SetFixedMaxEntries(0);
	FullInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	const FRpgHarvestResult Overflow =
		OverflowNode->HarvestableNode->CommitHarvest_Implementation(MakeRequest(OverflowNode, FullHarvester, 3));
	TestTrue(TEXT("A full inventory still harvests through the overflow drop"), Overflow.IsSuccess());
	TestTrue(TEXT("The result reports the world drop"), Overflow.Delivery == ERpgHarvestDelivery::WorldDrop);
	const TArray<ARpgDroppedInventoryActor*> Drops = GetWorldDrops(World);
	if (!TestEqual(TEXT("A multi-section overflow spawns exactly one drop"), Drops.Num(), 1))
	{
		return false;
	}
	const URpgInventoryManagerComponent* DropInventory = Drops[0]->GetLootInventoryManager();
	TestTrue(
		TEXT("The single drop contains every overflowed section"),
		DropInventory &&
			DropInventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass()) ==
				3 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeSharedStockTest,
	"SurvivalRpg.Harvesting.Node.SharedStockAcrossHarvesters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeSharedStockTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeSectionedProfile(World, 4, 0.0f));
	ARpgHarvestAutomationTestPlayerState* First = SpawnHarvester(World);
	ARpgHarvestAutomationTestPlayerState* Second = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Shared node exists"), Node) ||
		!TestNotNull(TEXT("First harvester exists"), First) ||
		!TestNotNull(TEXT("Second harvester exists"), Second))
	{
		return false;
	}

	// Both harvesters select the target before either commits, as concurrent abilities do.
	const FRpgHarvestRequest FirstRequest = MakeRequest(Node, First);
	const FRpgHarvestRequest SecondRequest = MakeRequest(Node, Second);
	int32 SectionsHarvested = 0;
	for (int32 Round = 0; Round < 2; ++Round)
	{
		const FRpgHarvestResult FirstResult = Node->HarvestableNode->CommitHarvest_Implementation(FirstRequest);
		const FRpgHarvestResult SecondResult = Node->HarvestableNode->CommitHarvest_Implementation(SecondRequest);
		TestTrue(*FString::Printf(TEXT("First harvester hit %d succeeds"), Round), FirstResult.IsSuccess());
		TestTrue(*FString::Printf(TEXT("Concurrent second harvester hit %d is not rejected"), Round), SecondResult.IsSuccess());
		SectionsHarvested += FirstResult.SectionsTaken + SecondResult.SectionsTaken;
	}

	TestEqual(TEXT("Both harvesters together extract exactly the defined stock"), SectionsHarvested, 4);
	TestEqual(TEXT("First harvester received two sections"), CountMaterial(First), 2 * YieldPerSection);
	TestEqual(TEXT("Second harvester received two sections"), CountMaterial(Second), 2 * YieldPerSection);
	TestTrue(
		TEXT("Further hits from either harvester report depletion"),
		Node->HarvestableNode->CommitHarvest_Implementation(FirstRequest).Outcome == ERpgHarvestOutcome::Depleted &&
			Node->HarvestableNode->CommitHarvest_Implementation(SecondRequest).Outcome == ERpgHarvestOutcome::Depleted);
	TestEqual(TEXT("Depletion grants nothing extra"), CountMaterial(First) + CountMaterial(Second), 4 * YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeRespawnTest,
	"SurvivalRpg.Harvesting.Node.StaleRevisionRespawnAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeRespawnTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeSectionedProfile(World, 4, 0.01f));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Respawning node exists"), Node) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgHarvestableComponent* Component = Node->HarvestableNode;

	TestWorld.PrimeTimerManager();
	const FRpgHarvestRequest PreDepletionRequest = MakeRequest(Node, Harvester, 4);
	TestTrue(TEXT("A full-stock request depletes the node"), Component->CommitHarvest_Implementation(PreDepletionRequest).bDepleted);
	TestEqual(TEXT("Depletion advances the revision"), Component->GetHarvestState().Revision, 1);

	TestWorld.AdvanceTimers(0.02f);
	TestTrue(TEXT("The respawn timer restores the node"), Component->IsHarvestable());
	TestEqual(TEXT("Respawn restores the complete stock"), Component->GetRemainingSections(), 4);
	TestEqual(TEXT("Respawn advances the revision"), Component->GetHarvestState().Revision, 2);
	TestTrue(
		TEXT("A request selected before depletion is stale after respawn"),
		Component->CommitHarvest_Implementation(PreDepletionRequest).Outcome == ERpgHarvestOutcome::Stale);
	TestEqual(TEXT("The stale request grants nothing"), CountMaterial(Harvester), 4 * YieldPerSection);

	TestTrue(TEXT("A fresh request harvests one section"), Component->CommitHarvest_Implementation(MakeRequest(Node, Harvester)).IsSuccess());
	TestEqual(TEXT("Three sections remain"), Component->GetRemainingSections(), 3);
	TestTrue(TEXT("Authority can restore partial stock"), Component->RestoreHarvestStock());
	TestEqual(TEXT("Restoring refills the stock"), Component->GetRemainingSections(), 4);
	TestEqual(TEXT("Restoring an active node keeps its revision"), Component->GetHarvestState().Revision, 2);
	TestFalse(TEXT("Restoring a full node changes nothing"), Component->RestoreHarvestStock());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeRequirementsTest,
	"SurvivalRpg.Harvesting.Node.ToolSkillAndRequestValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeRequirementsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestProfile* ToolProfile = MakeSectionedProfile(World, 4, 0.0f);
	ToolProfile->RequiredToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	URpgHarvestProfile* SkillProfile = MakeSectionedProfile(World, 4, 0.0f);
	SkillProfile->MinimumSkillLevel = 2;
	AddExpectedMessage(
		TEXT("has no harvest profile and rejects every harvest request"),
		ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains,
		1);
	ARpgHarvestAutomationNodeActor* ToolNode = SpawnNode(World, ToolProfile);
	ARpgHarvestAutomationNodeActor* SkillNode = SpawnNode(World, SkillProfile);
	ARpgHarvestAutomationNodeActor* UnconfiguredNode = SpawnNode(World, nullptr);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Tool node exists"), ToolNode) ||
		!TestNotNull(TEXT("Skill node exists"), SkillNode) ||
		!TestNotNull(TEXT("Unconfigured node exists"), UnconfiguredNode) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}

	FRpgHarvestRequest ToolRequest = MakeRequest(ToolNode, Harvester);
	TestTrue(
		TEXT("A tool-less request is rejected as WrongTool"),
		ToolNode->HarvestableNode->CommitHarvest_Implementation(ToolRequest).Outcome == ERpgHarvestOutcome::WrongTool);
	ToolRequest.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting;
	TestTrue(
		TEXT("A parent tool category is rejected as WrongTool"),
		ToolNode->HarvestableNode->EvaluateHarvest_Implementation(ToolRequest).Outcome == ERpgHarvestOutcome::WrongTool);
	ToolRequest.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	TestTrue(TEXT("The matching tool harvests"), ToolNode->HarvestableNode->CommitHarvest_Implementation(ToolRequest).IsSuccess());

	TestTrue(
		TEXT("An unmet skill requirement is reported as SkillGate"),
		SkillNode->HarvestableNode->CommitHarvest_Implementation(MakeRequest(SkillNode, Harvester)).Outcome ==
			ERpgHarvestOutcome::SkillGate);
	TestEqual(TEXT("The skill-gated node keeps its stock"), SkillNode->HarvestableNode->GetRemainingSections(), 4);

	FRpgHarvestRequest ZeroSections = MakeRequest(ToolNode, Harvester, 0);
	ZeroSections.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	TestTrue(
		TEXT("A request for zero sections is invalid"),
		ToolNode->HarvestableNode->EvaluateHarvest_Implementation(ZeroSections).Outcome == ERpgHarvestOutcome::Invalid);

	FRpgHarvestRequest WrongActor = MakeRequest(ToolNode, Harvester);
	WrongActor.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	WrongActor.Hit = FHitResult(SkillNode, nullptr, FVector::ZeroVector, FVector::UpVector);
	TestTrue(
		TEXT("A hit on another actor is invalid for this node"),
		ToolNode->HarvestableNode->EvaluateHarvest_Implementation(WrongActor).Outcome == ERpgHarvestOutcome::Invalid);
	TestEqual(
		TEXT("Another actor's hit reports no revision"),
		IRpgHarvestableTarget::Execute_GetHarvestRevision(ToolNode->HarvestableNode, WrongActor.Hit),
		INDEX_NONE);

	FRpgHarvestRequest WrongAbility = MakeRequest(ToolNode, Harvester);
	WrongAbility.ToolTag = RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
	WrongAbility.AbilityId = FGameplayTag();
	TestTrue(
		TEXT("A request without a harvesting ability id is invalid"),
		ToolNode->HarvestableNode->EvaluateHarvest_Implementation(WrongAbility).Outcome == ERpgHarvestOutcome::Invalid);

	TestTrue(
		TEXT("A node without a profile rejects every request"),
		UnconfiguredNode->HarvestableNode->CommitHarvest_Implementation(MakeRequest(UnconfiguredNode, Harvester)).Outcome ==
			ERpgHarvestOutcome::Invalid);
	TestEqual(TEXT("Only the valid tool hit granted material"), CountMaterial(Harvester), YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeFailedDeliveryTest,
	"SurvivalRpg.Harvesting.Node.FailedDeliveryLeavesStockUntouched",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeFailedDeliveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestProfile* Profile = MakeSectionedProfile(World, 4, 0.0f);
	Profile->OverflowDropClass = ARpgHarvestAutomationPartialFailureDropActor::StaticClass();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, Profile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Failing node exists"), Node) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgInventoryManagerComponent* Inventory = Harvester->GetInventoryManagerComponent();
	Inventory->SetFixedMaxEntries(0);
	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);

	const FRpgHarvestResult Result = Node->HarvestableNode->CommitHarvest_Implementation(MakeRequest(Node, Harvester, 2));
	TestTrue(TEXT("A failed overflow materialization is reported"), Result.Outcome == ERpgHarvestOutcome::DeliveryFailed);
	TestEqual(TEXT("The stock is untouched"), Node->HarvestableNode->GetRemainingSections(), 4);
	TestEqual(TEXT("The revision is untouched"), Node->HarvestableNode->GetHarvestState().Revision, 0);
	TestEqual(TEXT("No partial drop is published"), GetWorldDrops(World).Num(), 0);
	TestTrue(TEXT("No XP is awarded"), FMath::IsNearlyZero(GetMiningXP(Harvester)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestNodeAuthorityAndEventsTest,
	"SurvivalRpg.Harvesting.Node.ReadOnlyEvaluationAuthorityAndEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestNodeAuthorityAndEventsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableComponentTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestAutomationNodeStateListener* Listener = NewObject<URpgHarvestAutomationNodeStateListener>();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeSectionedProfile(World, 4, 0.0f), Listener);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Observed node exists"), Node) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgHarvestableComponent* Component = Node->HarvestableNode;

	TestEqual(TEXT("BeginPlay presents the current state once"), Listener->EventCount, 1);
	TestTrue(TEXT("The BeginPlay state is initial"), Listener->bLastInitialState);
	TestEqual(TEXT("The initial state reports the full stock"), Listener->LastRemainingSections, 4);
	TestEqual(TEXT("The initial state reports the section count"), Listener->LastSectionCount, 4);

	const FRpgHarvestRequest Request = MakeRequest(Node, Harvester);
	Node->SetRole(ROLE_SimulatedProxy);
	const FRpgHarvestResult Preview = Component->EvaluateHarvest_Implementation(Request);
	TestTrue(TEXT("A client can evaluate the replicated stock for previews"), Preview.IsSuccess());
	TestEqual(TEXT("The client preview reports the section it would take"), Preview.SectionsTaken, 1);
	TestTrue(
		TEXT("A client cannot commit a harvest"),
		Component->CommitHarvest_Implementation(Request).Outcome == ERpgHarvestOutcome::Invalid);
	TestFalse(TEXT("A client cannot restore stock"), Component->RestoreHarvestStock());
	TestEqual(TEXT("Rejected client calls leave the stock untouched"), Component->GetRemainingSections(), 4);
	TestEqual(TEXT("Rejected client calls grant nothing"), CountMaterial(Harvester), 0);
	Node->SetRole(ROLE_Authority);

	TestTrue(TEXT("Authority commits the same request"), Component->CommitHarvest_Implementation(Request).IsSuccess());
	TestEqual(TEXT("The commit presents the new state"), Listener->EventCount, 2);
	TestFalse(TEXT("A live commit is not initial state"), Listener->bLastInitialState);
	TestEqual(TEXT("The live event reports the stock left"), Listener->LastRemainingSections, 3);
	TestTrue(TEXT("The partially harvested node stays active"), Listener->bLastActive);

	const FStructProperty* StateProperty =
		FindFProperty<FStructProperty>(URpgHarvestableComponent::StaticClass(), TEXT("HarvestState"));
	TestTrue(
		TEXT("The stock is a replicated property with a RepNotify"),
		StateProperty && StateProperty->HasAllPropertyFlags(CPF_Net | CPF_RepNotify));
	TestTrue(TEXT("The component replicates by default"), Component->GetIsReplicated());
	TestFalse(TEXT("The component never ticks"), Component->PrimaryComponentTick.bCanEverTick);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
