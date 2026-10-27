#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancedMeshActor.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestAutomationTestWorld.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestPersistenceComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestStockRules.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include <limits>
#include "GameFramework/GameStateBase.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestWorkTests
{
	using namespace RpgHarvestAutomation;

	constexpr int32 Unit = RpgHarvestWork::UnitsPerSection;
	constexpr int32 YieldPerSection = 2;
	constexpr int32 ExperiencePerSection = 10;

	URpgHarvestProfile* MakeProfile(UObject* Outer, const int32 SectionCount, const float RespawnSeconds = 0.0f)
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

	ARpgHarvestAutomationTestPlayerState* SpawnHarvester(UWorld* World)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World ? World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(SpawnParameters) : nullptr;
	}

	/** Spawns a node with Profile; a Name and bLoadedWithMap make it a map-placed node the persistence saves. */
	ARpgHarvestAutomationNodeActor* SpawnNode(
		UWorld* World,
		URpgHarvestProfile* Profile,
		const FName Name = NAME_None,
		const bool bLoadedWithMap = false)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = Name;
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.bDeferConstruction = true;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgHarvestAutomationNodeActor* Node = World ? World->SpawnActor<ARpgHarvestAutomationNodeActor>(SpawnParameters) : nullptr;
		FObjectProperty* ProfileProperty =
			FindFProperty<FObjectProperty>(URpgHarvestableComponent::StaticClass(), TEXT("HarvestProfile"));
		if (!Node || !Node->HarvestableNode || !ProfileProperty)
		{
			return nullptr;
		}
		ProfileProperty->SetObjectPropertyValue_InContainer(Node->HarvestableNode, Profile);
		Node->bNetStartup = bLoadedWithMap;
		Node->FinishSpawning(FTransform::Identity);
		if (!Node->HasActorBegunPlay())
		{
			Node->DispatchBeginPlay();
		}
		return Node;
	}

	/** Builds a request the way an ability does, observing the revision now, with WorkScale applied to one section. */
	FRpgHarvestRequest MakeNodeRequest(ARpgHarvestAutomationNodeActor* Node, AActor* Harvester, const float WorkScale)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.WorkScale = WorkScale;
		Request.Hit = FHitResult(Node, nullptr, FVector(0.0, 0.0, 50.0), FVector::UpVector);
		Request.ExpectedRevision = Node && Node->HarvestableNode
			? IRpgHarvestableTarget::Execute_GetHarvestRevision(Node->HarvestableNode, Request.Hit)
			: INDEX_NONE;
		return Request;
	}

	/** Builds a request for the stateless rules; its hit lies on the weak point that MakeWeakPoint returns. */
	FRpgHarvestRequest MakeRulesRequest(AActor* Harvester, const float WorkScale, const int32 RequestedSections = 1)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		Request.WorkScale = WorkScale;
		Request.ExpectedRevision = 0;
		Request.Hit = FHitResult(nullptr, nullptr, FVector(0.0, 0.0, 50.0), FVector::UpVector);
		return Request;
	}

	FRpgHarvestWeakPoint MakeWeakPoint()
	{
		FRpgHarvestWeakPoint WeakPoint;
		WeakPoint.Location = FVector(0.0, 0.0, 50.0);
		WeakPoint.Radius = 30.0f;
		return WeakPoint;
	}

	FRpgHarvestStockSnapshot MakeStock(const int32 SectionCount, const int32 HarvestedSections, const int32 SectionWork)
	{
		FRpgHarvestStockSnapshot Stock;
		Stock.SectionCount = SectionCount;
		Stock.HarvestedSections = HarvestedSections;
		Stock.SectionWork = SectionWork;
		return Stock;
	}

	int32 CountMaterial(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgInventoryManagerComponent* Inventory = Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
		return Inventory
			? Inventory->GetTotalItemCountByDefinition(URpgHarvestAutomationTestStackItemDefinition::StaticClass())
			: 0;
	}

	float GetMiningXP(const ARpgHarvestAutomationTestPlayerState* Harvester)
	{
		const URpgTradeSkillProgressionComponent* TradeSkills =
			Harvester ? Harvester->GetTradeSkillProgressionComponent() : nullptr;
		return TradeSkills ? TradeSkills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Mining) : 0.0f;
	}

	int32 CountWorldDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			Count += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
		return Count;
	}

	/** Adds the persistence component to World's GameState, the way the harvesting GameFeature does on the server. */
	URpgHarvestPersistenceComponent* AddPersistence(UWorld* World)
	{
		AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (World && !GameState)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GameState = World->SpawnActor<AGameStateBase>(SpawnParameters);
			World->SetGameState(GameState);
		}
		if (!GameState)
		{
			return nullptr;
		}
		if (!GameState->HasActorBegunPlay())
		{
			GameState->DispatchBeginPlay();
		}
		URpgHarvestPersistenceComponent* Persistence =
			NewObject<URpgHarvestPersistenceComponent>(GameState, TEXT("HarvestPersistence"));
		Persistence->RegisterComponent();
		return Persistence;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkStockRulesTest,
	"SurvivalRpg.Harvesting.Work.StockRulesQuantizeWorkAndOverkill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkStockRulesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	URpgHarvestProfile* Profile = MakeProfile(World, 4);
	if (!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}

	// Quantization.
	FRpgHarvestRequest Request = MakeRulesRequest(Harvester, 0.25f);
	TestEqual(TEXT("A quarter section is 25 work units"), Request.GetRequestedWork(), 25);
	Request.WorkScale = 0.333f;
	TestEqual(TEXT("Work rounds to whole hundredths of a section"), Request.GetRequestedWork(), 33);
	Request.WorkScale = 0.004f;
	TestEqual(TEXT("Work below half a unit rounds to nothing"), Request.GetRequestedWork(), 0);
	TestTrue(
		TEXT("A request without work is invalid"),
		FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0)).Outcome == ERpgHarvestOutcome::Invalid);
	for (const float InvalidScale : {0.0f, -1.0f, 17.0f, std::numeric_limits<float>::quiet_NaN()})
	{
		Request.WorkScale = InvalidScale;
		TestEqual(TEXT("An invalid work scale brings no work"), Request.GetRequestedWork(), 0);
	}
	Request = MakeRulesRequest(Harvester, 1.0f, 0);
	TestEqual(TEXT("Zero requested sections bring no work"), Request.GetRequestedWork(), 0);
	Request = MakeRulesRequest(Harvester, 16.0f, 1000);
	TestEqual(TEXT("Oversized work is capped"), Request.GetRequestedWork(), RpgHarvestWork::MaxRequestUnits);

	// Partial work stays on the section without completing it.
	FRpgHarvestResult Result = FRpgHarvestStockRules::Evaluate(Profile, MakeRulesRequest(Harvester, 0.4f), MakeStock(4, 0, 0));
	TestTrue(TEXT("Partial work is accepted"), Result.IsSuccess() && Result.WorkApplied == 40);
	TestEqual(TEXT("Partial work completes no section"), Result.SectionsTaken, 0);
	TestEqual(TEXT("Partial work stays as residual work"), Result.SectionWork, 40);
	TestTrue(
		TEXT("Partial work leaves the sections and reduces the remaining work"),
		Result.RemainingSections == 4 && Result.RemainingWork == 4 * Unit - 40 && Result.OverkillWork == 0);
	TestTrue(TEXT("Accepted partial direct work counts as an own hit"), Result.IsAcceptedDirectHit());

	// Residual work completes a section once it adds up.
	Result = FRpgHarvestStockRules::Evaluate(Profile, MakeRulesRequest(Harvester, 0.4f), MakeStock(4, 0, 80));
	TestTrue(
		TEXT("Residual and new work complete one section and keep the rest"),
		Result.SectionsTaken == 1 && Result.SectionWork == 20 && Result.RemainingSections == 3 &&
			Result.RemainingWork == 3 * Unit - 20);
	Result = FRpgHarvestStockRules::Evaluate(Profile, MakeRulesRequest(Harvester, 2.5f), MakeStock(4, 0, 50));
	TestTrue(
		TEXT("One request may complete several sections"),
		Result.SectionsTaken == 3 && Result.SectionWork == 0 && Result.RemainingSections == 1 && Result.RemainingWork == Unit);

	// Only work beyond the remaining work is overkill.
	Result = FRpgHarvestStockRules::Evaluate(Profile, MakeRulesRequest(Harvester, 1.25f), MakeStock(4, 3, 70));
	TestTrue(
		TEXT("The last section takes only the work it still needs"),
		Result.WorkApplied == 30 && Result.SectionsTaken == 1 && Result.RemainingSections == 0 && Result.RemainingWork == 0);
	TestEqual(TEXT("The rest of the work is true overkill"), Result.OverkillWork, 95);
	TestEqual(TEXT("A depleting result keeps no residual work"), Result.SectionWork, 0);
	Request = MakeRulesRequest(Harvester, 1.25f);
	Request.bFinishResource = true;
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 3, 70));
	TestTrue(
		TEXT("Finishing takes exactly the remaining work and manufactures no overkill"),
		Result.WorkApplied == 30 && Result.OverkillWork == 0 && Result.SectionsTaken == 1);
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 1, 10));
	TestTrue(
		TEXT("Finishing takes every remaining section"),
		Result.WorkApplied == 3 * Unit - 10 && Result.SectionsTaken == 3 && Result.OverkillWork == 0);

	// A weak point adds its bonus as work from the same stock.
	Profile->WeakPointBonusSections = 1;
	const FRpgHarvestWeakPoint WeakPoint = MakeWeakPoint();
	Request = MakeRulesRequest(Harvester, 0.5f);
	Request.bCanHitWeakPoint = true;
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0), &WeakPoint);
	TestTrue(
		TEXT("A weak-point hit adds one section of work"),
		Result.bWeakPointHit && Result.WorkApplied == 150 && Result.SectionsTaken == 1 && Result.SectionWork == 50);
	Request.WeakPointWorkScale = 2.0f;
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0), &WeakPoint);
	TestTrue(TEXT("A weak-point work scale multiplies the bonus work"), Result.bWeakPointHit && Result.WorkApplied == 250);
	Request.WeakPointWorkScale = -1.0f;
	TestTrue(
		TEXT("A negative weak-point work scale is invalid"),
		FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0), &WeakPoint).Outcome == ERpgHarvestOutcome::Invalid);
	Request.WeakPointWorkScale = 1.0f;
	Request.bFinishResource = true;
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 3, 0), &WeakPoint);
	TestTrue(TEXT("Finishing never strikes a weak point"), !Result.bWeakPointHit && Result.WorkApplied == Unit);
	Profile->WeakPointBonusSections = 0;

	// Provenance passes through unchanged.
	Request = MakeRulesRequest(Harvester, 0.3f);
	Request.Provenance.RootActionId = 9;
	Request.Provenance = Request.Provenance.MakeSecondary(RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual);
	Result = FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0));
	TestTrue(
		TEXT("The result carries the request's provenance"),
		!Result.Provenance.IsDirect() && Result.Provenance.RootActionId == 9 &&
			Result.Provenance.ProcTag == RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual);
	TestFalse(TEXT("Accepted secondary work is not an own hit"), Result.IsAcceptedDirectHit());
	const int32 FirstRoot = FRpgHarvestProvenance::MakeRootActionId();
	const int32 SecondRoot = FRpgHarvestProvenance::MakeRootActionId();
	TestTrue(TEXT("Root action ids are positive and unique"), FirstRoot > 0 && SecondRoot > 0 && FirstRoot != SecondRoot);

	// Rejections are unchanged.
	FRpgHarvestStockSnapshot Depleted = MakeStock(4, 4, 0);
	Depleted.bActive = false;
	TestTrue(
		TEXT("A depleted stock rejects work"),
		FRpgHarvestStockRules::Evaluate(Profile, MakeRulesRequest(Harvester, 0.3f), Depleted).Outcome ==
			ERpgHarvestOutcome::Depleted);
	Request = MakeRulesRequest(Harvester, 0.3f);
	Request.ExpectedRevision = 1;
	TestTrue(
		TEXT("A request aimed at another revision is stale"),
		FRpgHarvestStockRules::Evaluate(Profile, Request, MakeStock(4, 0, 0)).Outcome == ERpgHarvestOutcome::Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkNodeYieldTest,
	"SurvivalRpg.Harvesting.Work.NodeResidualKeepsTotalYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkNodeYieldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeProfile(World, 4));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	ARpgHarvestAutomationTestPlayerState* Partner = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Node exists"), Node) || !TestNotNull(TEXT("Harvester exists"), Harvester) ||
		!TestNotNull(TEXT("Partner exists"), Partner))
	{
		return false;
	}
	URpgHarvestableComponent* Stock = Node->HarvestableNode;

	// Two harvesters aim at the same stock before either commits.
	const FRpgHarvestRequest First = MakeNodeRequest(Node, Harvester, 0.3f);
	const FRpgHarvestRequest Concurrent = MakeNodeRequest(Node, Partner, 0.3f);
	const FRpgHarvestResult FirstResult = Stock->CommitHarvest_Implementation(First);
	TestTrue(
		TEXT("Partial work is accepted without a reward"),
		FirstResult.IsSuccess() && FirstResult.SectionsTaken == 0 && FirstResult.Delivery == ERpgHarvestDelivery::Empty);
	TestEqual(TEXT("Partial work grants no material"), CountMaterial(Harvester), 0);
	TestTrue(TEXT("Partial work grants no XP"), FMath::IsNearlyZero(GetMiningXP(Harvester)));
	TestEqual(TEXT("Partial work is replicated state"), static_cast<int32>(Stock->GetHarvestState().SectionWork), 30);
	TestEqual(TEXT("Partial work keeps the revision"), Stock->GetHarvestState().Revision, 0);
	const FRpgHarvestResult ConcurrentResult = Stock->CommitHarvest_Implementation(Concurrent);
	TestTrue(
		TEXT("A concurrent request is evaluated against the stock as it is now"),
		ConcurrentResult.IsSuccess() && ConcurrentResult.SectionWork == 60);

	// Keep working the node with the same small swings until it is empty.
	const FRpgHarvestRequest BeforeDepletion = MakeNodeRequest(Node, Harvester, 0.3f);
	int32 Commits = 2;
	int32 SectionsTaken = 0;
	int32 LastOverkill = 0;
	while (Stock->IsHarvestable() && Commits < 100)
	{
		const FRpgHarvestResult Result = Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.3f));
		if (!TestTrue(TEXT("Every swing on the stocked node is accepted"), Result.IsSuccess()))
		{
			return false;
		}
		++Commits;
		SectionsTaken += Result.SectionsTaken;
		LastOverkill = Result.OverkillWork;
		if (Commits == 4)
		{
			TestTrue(
				TEXT("The swing that completes the first section rewards it once"),
				Result.SectionsTaken == 1 && CountMaterial(Harvester) == YieldPerSection &&
					FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(ExperiencePerSection)));
		}
	}
	TestEqual(TEXT("Fourteen swings of 0.3 sections empty four sections"), Commits, 14);
	TestEqual(TEXT("The swings complete every section once"), SectionsTaken, 4);
	TestEqual(TEXT("The last swing reports the work it did not need"), LastOverkill, 14 * 30 - 4 * Unit);
	TestEqual(TEXT("The total yield equals the defined stock"), CountMaterial(Harvester), 4 * YieldPerSection);
	TestTrue(
		TEXT("XP is awarded once per section"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(4 * ExperiencePerSection)));
	TestEqual(TEXT("The partner's partial work earned nothing"), CountMaterial(Partner), 0);
	TestTrue(
		TEXT("Only the depletion advances the revision"),
		Stock->GetHarvestState().Revision == 1 && BeforeDepletion.ExpectedRevision == 0);
	TestEqual(TEXT("A depleted node keeps no residual work"), static_cast<int32>(Stock->GetHarvestState().SectionWork), 0);

	// Restoring clears residual work too.
	TestTrue(TEXT("The emptied node is restored"), Stock->RestoreHarvestStock());
	TestTrue(TEXT("Partial work on the restored node is accepted"), Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.3f)).IsSuccess());
	TestTrue(TEXT("A node with only residual work can be restored"), Stock->RestoreHarvestStock());
	TestEqual(TEXT("Restoring drops the residual work"), static_cast<int32>(Stock->GetHarvestState().SectionWork), 0);
	TestFalse(TEXT("A pristine node has nothing to restore"), Stock->RestoreHarvestStock());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkMasteryTest,
	"SurvivalRpg.Harvesting.Work.SharedMasteryXPOncePerSection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkMasteryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestProfile* Profile = MakeProfile(World, 4);
	Profile->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting;
	Profile->MasteryExperience = 7;
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, Profile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	const URpgTradeSkillProgressionComponent* TradeSkills = Harvester ? Harvester->GetTradeSkillProgressionComponent() : nullptr;
	if (!TestNotNull(TEXT("Node exists"), Node) || !TestNotNull(TEXT("Harvester has trade skills"), TradeSkills))
	{
		return false;
	}
	const FGameplayTag Mastery = RpgTradeSkillGameplayTags::Skill_Mastery_Harvesting;
	URpgHarvestableComponent* Stock = Node->HarvestableNode;

	TestTrue(TEXT("Partial work is accepted"), Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.5f)).IsSuccess());
	TestTrue(TEXT("Work without a completed section awards no mastery XP"), FMath::IsNearlyZero(TradeSkills->GetSkillXPByTag(Mastery)));
	TestTrue(TEXT("The completing work is accepted"), Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.5f)).IsSuccess());
	TestTrue(TEXT("A completed section awards mastery XP once"), FMath::IsNearlyEqual(TradeSkills->GetSkillXPByTag(Mastery), 7.0f));
	TestTrue(TEXT("Two more sections are harvested"), Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 2.0f)).SectionsTaken == 2);
	TestTrue(TEXT("Mastery XP follows the completed sections"), FMath::IsNearlyEqual(TradeSkills->GetSkillXPByTag(Mastery), 21.0f));
	TestTrue(
		TEXT("The profession keeps its own XP"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(3 * ExperiencePerSection)));

	FRpgHarvestRequest Stale = MakeNodeRequest(Node, Harvester, 1.0f);
	Stale.ExpectedRevision = 5;
	TestFalse(TEXT("A stale request is rejected"), Stock->CommitHarvest_Implementation(Stale).IsSuccess());
	TestTrue(TEXT("A rejected request awards no mastery XP"), FMath::IsNearlyEqual(TradeSkills->GetSkillXPByTag(Mastery), 21.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkFailedDeliveryTest,
	"SurvivalRpg.Harvesting.Work.FailedDeliveryKeepsStockAndWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkFailedDeliveryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestProfile* Profile = MakeProfile(World, 4);
	Profile->OverflowDropClass = ARpgHarvestAutomationPartialFailureDropActor::StaticClass();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, Profile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Node exists"), Node) || !TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	URpgInventoryManagerComponent* Inventory = Harvester->GetInventoryManagerComponent();
	Inventory->SetFixedMaxEntries(0);
	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	URpgHarvestableComponent* Stock = Node->HarvestableNode;

	const FRpgHarvestResult Partial = Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.6f));
	TestTrue(TEXT("Work without a completed section needs no delivery"), Partial.IsSuccess() && Partial.SectionWork == 60);

	const FRpgHarvestResult Failed = Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.6f));
	TestTrue(TEXT("A section whose reward cannot be delivered fails"), Failed.Outcome == ERpgHarvestOutcome::DeliveryFailed);
	TestEqual(TEXT("The failed commit keeps the residual work"), static_cast<int32>(Stock->GetHarvestState().SectionWork), 60);
	TestEqual(TEXT("The failed commit keeps the sections"), Stock->GetRemainingSections(), 4);
	TestEqual(TEXT("No partial drop is published"), CountWorldDrops(World), 0);
	TestTrue(TEXT("No XP is awarded"), FMath::IsNearlyZero(GetMiningXP(Harvester)));

	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::Unlimited);
	const FRpgHarvestResult Retried = Stock->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.6f));
	TestTrue(
		TEXT("The retried swing completes the section from the kept work"),
		Retried.IsSuccess() && Retried.SectionsTaken == 1 && Retried.SectionWork == 20 &&
			CountMaterial(Harvester) == YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkInstancesTest,
	"SurvivalRpg.Harvesting.Work.InstancesMatchNodesAndSaveResidualWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkInstancesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHarvestInstanceStockComponent* Stock = AddInstanceStock(World);
	URpgHarvestProfile* Profile = MakeProfile(World, 2, 30.0f);
	ARpgHarvestAutomationInstancesActor* Actor =
		SpawnInstances(World, Profile, {FVector(0.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0)});
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, Profile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Instance stock exists"), Stock) || !TestNotNull(TEXT("Instances exist"), Actor) ||
		!TestNotNull(TEXT("Node exists"), Node) || !TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	TestWorld.PrimeTimerManager();
	URpgHarvestAutomationInstancesComponent* Instances = Actor->Instances;

	// The same swings give the same results on an instance and on an actor node.
	for (const float WorkScale : {0.5f, 0.5f, 1.7f})
	{
		FRpgHarvestRequest InstanceRequest = MakeInstanceRequest(Instances, 0, Harvester);
		InstanceRequest.WorkScale = WorkScale;
		const FRpgHarvestResult InstanceResult = Instances->CommitHarvest_Implementation(InstanceRequest);
		const FRpgHarvestResult NodeResult = Node->HarvestableNode->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, WorkScale));
		TestTrue(
			TEXT("Instances and nodes apply the same work"),
			InstanceResult.IsSuccess() && NodeResult.IsSuccess() &&
				InstanceResult.WorkApplied == NodeResult.WorkApplied &&
				InstanceResult.SectionsTaken == NodeResult.SectionsTaken &&
				InstanceResult.SectionWork == NodeResult.SectionWork &&
				InstanceResult.OverkillWork == NodeResult.OverkillWork &&
				InstanceResult.bDepleted == NodeResult.bDepleted);
		if (WorkScale == 0.5f && InstanceResult.SectionsTaken == 0)
		{
			TestEqual(TEXT("Partial work stores the instance"), Stock->GetNumChangedInstances(), 1);
			TestEqual(TEXT("Partial work keeps the instance's sections"), Instances->GetRemainingSections(0), 2);
			TestEqual(TEXT("Partial work keeps the instance's revision"), IRpgHarvestableTarget::Execute_GetHarvestRevision(Instances, InstanceRequest.Hit), 0);
		}
	}
	TestEqual(TEXT("The instance is depleted"), Instances->GetRemainingSections(0), 0);
	TestEqual(TEXT("Both resources yielded their stock once"), CountMaterial(Harvester), 2 * 2 * YieldPerSection);

	// Residual work alone is saved and restored.
	FRpgHarvestRequest Partial = MakeInstanceRequest(Instances, 1, Harvester);
	Partial.WorkScale = 0.25f;
	TestTrue(TEXT("Partial work on the second instance is accepted"), Instances->CommitHarvest_Implementation(Partial).IsSuccess());
	const FIntVector Key = URpgHarvestInstanceStockComponent::MakeInstanceKey(FVector(200.0, 0.0, 0.0));
	FRpgHarvestSavedStock Saved;
	TestTrue(TEXT("The worked instance exports its stock"), Stock->ExportSavedStock(Key, Saved));
	TestTrue(TEXT("Residual work alone is saved"), Saved.SectionWork == 25 && Saved.HarvestedSections == 0 && !Saved.IsPristine());
	TestTrue(TEXT("The instance is restored"), Stock->RestoreStock(Key));
	TestTrue(TEXT("The saved stock is applied again"), Stock->ApplySavedStock(Key, Saved));
	FRpgHarvestRequest Next = MakeInstanceRequest(Instances, 1, Harvester);
	Next.WorkScale = 0.75f;
	const FRpgHarvestResult Continued = Instances->CommitHarvest_Implementation(Next);
	TestTrue(
		TEXT("Work continues from the restored residual work"),
		Continued.IsSuccess() && Continued.SectionsTaken == 1 && Continued.SectionWork == 0 && Continued.RemainingSections == 1);

	AddInfo(FString::Printf(
		TEXT("Instance stock entry: %d bytes; actor node state: %d bytes; residual work replicates as one byte per changed entry."),
		static_cast<int32>(sizeof(FRpgHarvestInstanceStockEntry)),
		static_cast<int32>(sizeof(FRpgHarvestNodeState))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkPersistenceTest,
	"SurvivalRpg.Harvesting.Work.ResidualWorkSurvivesReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkPersistenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	const FName NodeId(TEXT("Work_PartialVein"));
	FRpgWorldFeatureSaveData Saved;
	FName FirstMapId;
	{
		FScopedTestWorld TestWorld;
		UWorld* World = TestWorld.GetWorld();
		URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
		ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeProfile(World, 3), NodeId, true);
		ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
		if (!TestNotNull(TEXT("Persistence exists"), Persistence) || !TestNotNull(TEXT("Node exists"), Node) ||
			!TestNotNull(TEXT("Harvester exists"), Harvester))
		{
			return false;
		}
		TestWorld.PrimeTimerManager();
		FirstMapId = URpgHarvestPersistenceComponent::MakeMapId(*World);
		TestTrue(TEXT("Partial work is accepted"), Node->HarvestableNode->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.4f)).IsSuccess());
		TestEqual(TEXT("A node with only residual work is recorded"), Persistence->GetNumRecordedResources(), 1);
		if (!TestTrue(TEXT("The stock is captured"), Persistence->CaptureWorldSaveData(Saved)))
		{
			return false;
		}
		FRpgHarvestSaveData Data;
		const FRpgHarvestSavedMap* Map =
			URpgHarvestPersistenceComponent::ReadPayload(Saved, Data) ? Data.Maps.Find(FirstMapId) : nullptr;
		TestTrue(
			TEXT("The save holds the residual work"),
			Map && Map->Nodes.Num() == 1 && Map->Nodes[0].Stock.SectionWork == 40 && Map->Nodes[0].Stock.bActive);
	}

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestAutomationNodeActor* Node = SpawnNode(World, MakeProfile(World, 3), NodeId, true);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	URpgHarvestPersistenceComponent* Persistence = AddPersistence(World);
	if (!TestNotNull(TEXT("Reloaded node exists"), Node) || !TestNotNull(TEXT("Reloaded persistence exists"), Persistence))
	{
		return false;
	}

	// Load the same save into this map, as if the host reloaded the world.
	FRpgHarvestSaveData Data;
	FRpgHarvestSavedMap Map;
	FRpgWorldFeatureSaveData Reloaded;
	if (!TestTrue(TEXT("The payload reads"), URpgHarvestPersistenceComponent::ReadPayload(Saved, Data)) ||
		!TestTrue(TEXT("The payload holds the first map"), Data.Maps.RemoveAndCopyValue(FirstMapId, Map)))
	{
		return false;
	}
	Data.Maps.Add(URpgHarvestPersistenceComponent::MakeMapId(*World), Map);
	TestWorld.PrimeTimerManager();
	if (!TestTrue(TEXT("The payload is written"), URpgHarvestPersistenceComponent::WritePayload(Data, Reloaded)) ||
		!TestTrue(TEXT("The saved stock is restored"), Persistence->RestoreWorldSaveData(&Reloaded)))
	{
		return false;
	}
	TestEqual(TEXT("The node keeps its residual work"), static_cast<int32>(Node->HarvestableNode->GetHarvestState().SectionWork), 40);
	TestEqual(TEXT("The node keeps its sections"), Node->HarvestableNode->GetRemainingSections(), 3);
	const FRpgHarvestResult Continued =
		Node->HarvestableNode->CommitHarvest_Implementation(MakeNodeRequest(Node, Harvester, 0.6f));
	TestTrue(
		TEXT("A request after the restore continues from the saved work"),
		Continued.IsSuccess() && Continued.SectionsTaken == 1 && Continued.SectionWork == 0 &&
			CountMaterial(Harvester) == YieldPerSection);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestWorkLegacyInstancesTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.ResidualWorkKeepsInstanceActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestWorkLegacyInstancesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestWorkTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestableInstancedMeshActor* Actor = World->SpawnActorDeferred<ARpgHarvestableInstancedMeshActor>(
		ARpgHarvestableInstancedMeshActor::StaticClass(),
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	URpgHarvestableInstancedMeshComponent* Component = Actor ? Actor->HarvestableInstances.Get() : nullptr;
	FObjectProperty* ProfileProperty =
		FindFProperty<FObjectProperty>(URpgHarvestableInstancedMeshComponent::StaticClass(), TEXT("HarvestProfile"));
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(World);
	if (!TestNotNull(TEXT("Instanced resource exists"), Component) || !TestNotNull(TEXT("Profile property exists"), ProfileProperty) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester))
	{
		return false;
	}
	Component->AddInstance(FTransform(FVector(0.0, 0.0, 0.0)));
	Component->AddInstance(FTransform(FVector(100.0, 0.0, 0.0)));
	ProfileProperty->SetObjectPropertyValue_InContainer(Component, MakeProfile(Actor, 1, 30.0f));
	Actor->FinishSpawning(FTransform::Identity);
	if (!Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
	TestWorld.PrimeTimerManager();

	auto MakeLegacyRequest = [Component, Harvester](const int32 InstanceIndex, const float WorkScale)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.WorkScale = WorkScale;
		Request.Hit = FHitResult(Component->GetOwner(), Component, FVector(InstanceIndex * 100.0, 0.0, 0.0), FVector::UpVector);
		Request.Hit.Item = InstanceIndex;
		Request.ExpectedRevision = Component->GetResourceInstanceRevision(InstanceIndex);
		return Request;
	};

	const FRpgHarvestResult First = Component->CommitHarvest_Implementation(MakeLegacyRequest(0, 0.4f));
	TestTrue(
		TEXT("Partial work on an instance is accepted without a reward"),
		First.IsSuccess() && First.SectionsTaken == 0 && First.Delivery == ERpgHarvestDelivery::Empty);
	TestTrue(TEXT("The partly worked instance stays active"), Component->IsResourceInstanceActive(0));
	TestEqual(TEXT("Partial work keeps the instance's revision"), Component->GetResourceInstanceRevision(0), 0);
	TestEqual(TEXT("Partial work grants no material"), CountMaterial(Harvester), 0);
	TestEqual(
		TEXT("The next swing continues from the residual work"),
		Component->CommitHarvest_Implementation(MakeLegacyRequest(0, 0.4f)).SectionWork,
		80);
	TestEqual(
		TEXT("Another instance keeps its own work"),
		Component->EvaluateHarvest_Implementation(MakeLegacyRequest(1, 0.4f)).SectionWork,
		40);

	const FRpgHarvestResult Last = Component->CommitHarvest_Implementation(MakeLegacyRequest(0, 0.4f));
	TestTrue(
		TEXT("The completing swing depletes the instance once"),
		Last.IsSuccess() && Last.SectionsTaken == 1 && Last.bDepleted && Last.OverkillWork == 20);
	TestFalse(TEXT("The depleted instance is inactive"), Component->IsResourceInstanceActive(0));
	TestEqual(TEXT("Depletion advances the revision"), Component->GetResourceInstanceRevision(0), 1);
	TestEqual(TEXT("The instance yields its stock once"), CountMaterial(Harvester), YieldPerSection);
	TestTrue(
		TEXT("The instance awards its XP once"),
		FMath::IsNearlyEqual(GetMiningXP(Harvester), static_cast<float>(ExperiencePerSection)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
