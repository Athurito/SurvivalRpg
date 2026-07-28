#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Harvesting/RpgHarvestableInstancedMeshActor.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgGatheringSet.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemComponent.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestableInstancedMeshTests
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
			GFrameCounter = CachedFrameCounter;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		void PrimeTimerManager() const
		{
			if (World)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(0.0f);
			}
		}

		void AdvanceTimers(float DeltaSeconds) const
		{
			if (World)
			{
				// Respawn deadlines intentionally use authoritative world time while
				// FTimerManager owns the tick-free wakeup queue. A standalone test
				// world does not advance either clock automatically, so keep them in
				// lockstep without ticking unrelated actors.
				World->TimeSeconds += DeltaSeconds;
				World->UnpausedTimeSeconds += DeltaSeconds;
				World->RealTimeSeconds += DeltaSeconds;
				++GFrameCounter;
				World->GetTimerManager().Tick(DeltaSeconds);
			}
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	ARpgHarvestableInstancedMeshActor* SpawnThreeInstanceFixture(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		ARpgHarvestableInstancedMeshActor* Actor = World->SpawnActorDeferred<ARpgHarvestableInstancedMeshActor>(
			ARpgHarvestableInstancedMeshActor::StaticClass(),
			FTransform::Identity,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor || !Actor->HarvestableInstances)
		{
			return nullptr;
		}

		for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
		{
			const FTransform AuthoredTransform(
				FRotator::ZeroRotator,
				FVector(static_cast<double>(InstanceIndex) * 100.0, 0.0, 0.0),
				FVector::OneVector);
			if (Actor->HarvestableInstances->AddInstance(AuthoredTransform) != InstanceIndex)
			{
				return nullptr;
			}
		}

		Actor->FinishSpawning(FTransform::Identity);
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}

	bool GatherInstanceOption(
		URpgHarvestableInstancedMeshComponent* Component,
		AActor* Requester,
		int32 InstanceIndex,
		FInteractionOption& OutOption)
	{
		if (!Component || !Component->GetOwner())
		{
			return false;
		}

		FTransform InstanceTransform;
		if (!Component->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
		{
			return false;
		}

		FInteractionQuery Query;
		Query.RequestingAvatar = Requester;
		Query.QueryMode = ERpgInteractionQueryMode::Focus;
		Query.QueryOrigin = Requester ? Requester->GetActorLocation() : FVector::ZeroVector;
		Query.CandidateHit = FHitResult(
			Component->GetOwner(),
			Component,
			InstanceTransform.GetLocation(),
			FVector::UpVector);
		Query.CandidateHit.Item = InstanceIndex;

		TScriptInterface<IInteractableTarget> Target;
		Target.SetObject(Component);
		Target.SetInterface(Component);
		TArray<FInteractionOption> Options;
		FInteractionOptionBuilder Builder(Target, Options);
		Component->GatherInteractionOptions(Query, Builder);
		if (Options.Num() != 1)
		{
			return false;
		}

		OutOption = Options[0];
		return true;
	}

	bool SetHarvestProfile(
		URpgHarvestableInstancedMeshComponent* Component,
		URpgHarvestProfile* Profile)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(
			URpgHarvestableInstancedMeshComponent::StaticClass(),
			TEXT("HarvestProfile"));
		if (!Component || !Property)
		{
			return false;
		}

		Property->SetObjectPropertyValue_InContainer(Component, Profile);
		return true;
	}

	URpgHarvestProfile* MakeHarvestProfile(
		UObject* Outer,
		const TArray<TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>>& Rewards,
		float RespawnSeconds)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		for (const TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>& Reward : Rewards)
		{
			FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
			Entry.ItemDefinition = Reward.Key;
			Entry.MinimumQuantity = Reward.Value;
			Entry.MaximumQuantity = Reward.Value;
			Entry.ChancePercent = 100.0f;
		}

		Profile->LootTable = Table;
		Profile->SkillTag =
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = 10;
		Profile->MinimumRespawnSeconds = RespawnSeconds;
		Profile->MaximumRespawnSeconds = RespawnSeconds;
		return Profile;
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
			TEXT("HarvestPlayerState"));
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(
			SpawnParameters);
	}

	FInteractionQuery MakeAuthorityQuery(
		URpgHarvestableInstancedMeshComponent* Component,
		AActor* Requester,
		const FInteractionOption& Option)
	{
		FInteractionQuery Query;
		Query.RequestingAvatar = Requester;
		Query.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
		Query.QueryOrigin = Requester ? Requester->GetActorLocation() : FVector::ZeroVector;
		Query.CandidateHit = FHitResult(
			Component ? Component->GetOwner() : nullptr,
			Component,
			Option.TargetRef.WorldLocation,
			FVector::UpVector);
		Query.CandidateHit.Item = Option.TargetRef.InstanceIndex;
		return Query;
	}

	int32 CountWorldDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestableInstancedMeshStableStateTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.StableIndexRevisionAndAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestableInstancedMeshStableStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancedMeshTests;

	FScopedTestWorld TestWorld;
	ARpgHarvestableInstancedMeshActor* ResourceActor = SpawnThreeInstanceFixture(TestWorld.GetWorld());
	URpgHarvestableInstancedMeshComponent* Component = ResourceActor ? ResourceActor->HarvestableInstances : nullptr;
	FActorSpawnParameters RequesterSpawnParameters;
	RequesterSpawnParameters.Name = MakeUniqueObjectName(TestWorld.GetWorld(), AActor::StaticClass(), TEXT("HarvestRequester"));
	RequesterSpawnParameters.ObjectFlags = RF_Transient;
	AActor* Requester = TestWorld.GetWorld()
		? TestWorld.GetWorld()->SpawnActor<AActor>(RequesterSpawnParameters)
		: nullptr;
	if (!TestNotNull(TEXT("Standalone HISM test world exists"), TestWorld.GetWorld()) ||
		!TestNotNull(TEXT("Harvestable wrapper actor is spawned"), ResourceActor) ||
		!TestNotNull(TEXT("Harvestable HISM component exists"), Component) ||
		!TestNotNull(TEXT("Harvest requester exists"), Requester))
	{
		return false;
	}

	TestTrue(TEXT("Standalone resource actor has server authority"), ResourceActor->HasAuthority());
	TestEqual(TEXT("Fixture owns three stable instance identities"), Component->GetInstanceCount(), 3);
	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		TestTrue(*FString::Printf(TEXT("Instance %d starts active"), InstanceIndex), Component->IsResourceInstanceActive(InstanceIndex));
		TestEqual(*FString::Printf(TEXT("Instance %d starts at revision zero"), InstanceIndex), Component->GetResourceInstanceRevision(InstanceIndex), 0);
	}

	FTransform OriginalFirstTransform;
	FTransform OriginalTargetTransform;
	FTransform OriginalLastTransform;
	TestTrue(TEXT("First authored transform is readable"), Component->GetInstanceTransform(0, OriginalFirstTransform, false));
	TestTrue(TEXT("Target authored transform is readable"), Component->GetInstanceTransform(1, OriginalTargetTransform, false));
	TestTrue(TEXT("Last authored transform is readable"), Component->GetInstanceTransform(2, OriginalLastTransform, false));

	FInteractionOption InitialOption;
	if (!TestTrue(TEXT("Active target instance gathers one option"), GatherInstanceOption(Component, Requester, 1, InitialOption)))
	{
		return false;
	}
	TestTrue(
		TEXT("Manual harvest option uses the feature-owned semantic action"),
		InitialOption.InteractionTag == RpgHarvestingMagicGameplayTags::Rpg_Interaction_Action_Harvest_Manual);
	TestEqual(TEXT("Option preserves the concrete HISM index"), InitialOption.TargetRef.InstanceIndex, 1);
	TestEqual(TEXT("Option starts with revision zero"), InitialOption.TargetRef.Revision, 0);
	TestTrue(TEXT("Option preserves the concrete HISM component"), InitialOption.TargetRef.TargetComponent.Get() == Component);

	ResourceActor->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A simulated proxy cannot deplete an instance"), Component->SetResourceInstanceActive(1, false));
	TestTrue(TEXT("Rejected proxy mutation leaves the instance active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Rejected proxy mutation leaves revision unchanged"), Component->GetResourceInstanceRevision(1), 0);
	ResourceActor->SetRole(ROLE_Authority);

	FInteractionQuery AuthorityQuery;
	AuthorityQuery.RequestingAvatar = Requester;
	AuthorityQuery.QueryMode = ERpgInteractionQueryMode::AuthorityValidation;
	AuthorityQuery.QueryOrigin = Requester->GetActorLocation();
	AuthorityQuery.CandidateHit = FHitResult(
		ResourceActor,
		Component,
		InitialOption.TargetRef.WorldLocation,
		FVector::UpVector);
	AuthorityQuery.CandidateHit.Item = 1;

	TestTrue(TEXT("Authority commits the exact current instance option"), Component->CommitInteraction(AuthorityQuery, InitialOption));
	TestFalse(TEXT("Committed harvest marks only the target instance inactive"), Component->IsResourceInstanceActive(1));
	TestTrue(TEXT("First neighboring instance remains active"), Component->IsResourceInstanceActive(0));
	TestTrue(TEXT("Last neighboring instance remains active"), Component->IsResourceInstanceActive(2));
	TestEqual(TEXT("Depletion advances only the target revision"), Component->GetResourceInstanceRevision(1), 1);
	TestEqual(TEXT("Depletion never removes or reorders instance identities"), Component->GetInstanceCount(), 3);

	FTransform DepletedFirstTransform;
	FTransform DepletedTargetTransform;
	FTransform DepletedLastTransform;
	TestTrue(TEXT("First transform remains readable after depletion"), Component->GetInstanceTransform(0, DepletedFirstTransform, false));
	TestTrue(TEXT("Depleted transform remains addressable by the same index"), Component->GetInstanceTransform(1, DepletedTargetTransform, false));
	TestTrue(TEXT("Last transform remains readable after depletion"), Component->GetInstanceTransform(2, DepletedLastTransform, false));
	TestTrue(TEXT("First neighboring transform is unchanged"), DepletedFirstTransform.Equals(OriginalFirstTransform));
	TestTrue(TEXT("Last neighboring transform is unchanged"), DepletedLastTransform.Equals(OriginalLastTransform));
	TestTrue(TEXT("Depleted instance keeps its location"), DepletedTargetTransform.GetLocation().Equals(OriginalTargetTransform.GetLocation()));
	TestTrue(TEXT("Depleted instance uses zero scale instead of index removal"), DepletedTargetTransform.GetScale3D().IsNearlyZero());

	TestFalse(TEXT("The stale pre-harvest option cannot be replayed"), Component->CommitInteraction(AuthorityQuery, InitialOption));
	FInteractionOption HiddenDepletedOption;
	TestFalse(TEXT("A depleted target instance no longer gathers an option"), GatherInstanceOption(Component, Requester, 1, HiddenDepletedOption));

	TestTrue(TEXT("Authority can reactivate the same stable instance"), Component->SetResourceInstanceActive(1, true));
	TestTrue(TEXT("Reactivated instance is active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Reactivation advances the same instance revision"), Component->GetResourceInstanceRevision(1), 2);
	TestEqual(TEXT("Reactivation preserves instance count"), Component->GetInstanceCount(), 3);
	FTransform ReactivatedTransform;
	TestTrue(TEXT("Reactivated transform is readable"), Component->GetInstanceTransform(1, ReactivatedTransform, false));
	TestTrue(TEXT("Reactivation restores the authored transform"), ReactivatedTransform.Equals(OriginalTargetTransform));

	FInteractionOption ReactivatedOption;
	if (!TestTrue(TEXT("Reactivated target gathers a fresh option"), GatherInstanceOption(Component, Requester, 1, ReactivatedOption)))
	{
		return false;
	}
	TestEqual(TEXT("Fresh option carries the new revision"), ReactivatedOption.TargetRef.Revision, 2);

	FInteractionQuery WrongInstanceQuery = AuthorityQuery;
	WrongInstanceQuery.CandidateHit.Item = 2;
	TestFalse(TEXT("Authority rejects a mismatched hit instance index"), Component->CommitInteraction(WrongInstanceQuery, ReactivatedOption));
	TestTrue(TEXT("Rejected mismatched index leaves the target active"), Component->IsResourceInstanceActive(1));
	TestEqual(TEXT("Rejected mismatched index leaves revision unchanged"), Component->GetResourceInstanceRevision(1), 2);

	AuthorityQuery.CandidateHit.Item = 1;
	AuthorityQuery.CandidateHit.ImpactPoint = ReactivatedOption.TargetRef.WorldLocation;
	AuthorityQuery.CandidateHit.Location = ReactivatedOption.TargetRef.WorldLocation;
	TestTrue(TEXT("Fresh exact option can be committed after reactivation"), Component->CommitInteraction(AuthorityQuery, ReactivatedOption));
	TestEqual(TEXT("Second depletion advances revision monotonically"), Component->GetResourceInstanceRevision(1), 3);
	TestEqual(TEXT("Second depletion still preserves all instance indices"), Component->GetInstanceCount(), 3);

	const FStructProperty* ReplicatedStatesProperty = FindFProperty<FStructProperty>(
		URpgHarvestableInstancedMeshComponent::StaticClass(),
		TEXT("ReplicatedInstanceStates"));
	TestTrue(
		TEXT("Sparse instance state is a replicated property"),
		ReplicatedStatesProperty && ReplicatedStatesProperty->HasAnyPropertyFlags(CPF_Net));
	TestTrue(
		TEXT("Replicated property uses the declared sparse state-list struct"),
		ReplicatedStatesProperty && ReplicatedStatesProperty->Struct == FRpgHarvestedInstanceStateList::StaticStruct());
	TestTrue(
		TEXT("Sparse state list is configured for FastArray net-delta serialization"),
		TStructOpsTypeTraits<FRpgHarvestedInstanceStateList>::WithNetDeltaSerializer != 0);

	TestFalse(TEXT("Invalid negative instance indices are inactive"), Component->IsResourceInstanceActive(INDEX_NONE));
	TestEqual(TEXT("Invalid negative instance indices have no revision"), Component->GetResourceInstanceRevision(INDEX_NONE), INDEX_NONE);
	TestFalse(TEXT("Out-of-bounds instance mutation is rejected"), Component->SetResourceInstanceActive(3, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestRewardExperienceRespawnTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.RewardExperienceStaleAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestRewardExperienceRespawnTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancedMeshTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestableInstancedMeshActor* ResourceActor =
		SpawnThreeInstanceFixture(World);
	URpgHarvestableInstancedMeshComponent* Component =
		ResourceActor ? ResourceActor->HarvestableInstances : nullptr;
	ARpgHarvestAutomationTestPlayerState* Harvester =
		SpawnHarvester(World);
	if (!TestNotNull(TEXT("Reward test resource actor exists"), ResourceActor) ||
		!TestNotNull(TEXT("Reward test component exists"), Component) ||
		!TestNotNull(TEXT("Reward test harvester exists"), Harvester))
	{
		return false;
	}

	URpgHarvestProfile* Profile = MakeHarvestProfile(
		ResourceActor,
		{
			TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
				URpgHarvestAutomationTestStackItemDefinition::StaticClass(),
				2)
		},
		0.01f);
	if (!TestTrue(TEXT("Transient harvest profile is attached"), SetHarvestProfile(Component, Profile)))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		Harvester->GetInventoryManagerComponent();
	URpgTradeSkillProgressionComponent* TradeSkills =
		Harvester->GetTradeSkillProgressionComponent();
	if (!TestNotNull(TEXT("Harvester owns the real inventory"), Inventory) ||
		!TestNotNull(TEXT("Harvester owns trade-skill progression"), TradeSkills))
	{
		return false;
	}

	TestWorld.PrimeTimerManager();
	FInteractionOption InitialOption;
	if (!TestTrue(
		TEXT("Active profile-backed resource gathers an option"),
		GatherInstanceOption(Component, Harvester, 0, InitialOption)))
	{
		return false;
	}
	FInteractionQuery AuthorityQuery =
		MakeAuthorityQuery(Component, Harvester, InitialOption);
	FRpgHarvestRequest InitialHarvestRequest;
	InitialHarvestRequest.Harvester = Harvester;
	InitialHarvestRequest.AbilityId =
		RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
	InitialHarvestRequest.TraceOrigin = AuthorityQuery.QueryOrigin;
	InitialHarvestRequest.Hit = AuthorityQuery.CandidateHit;
	InitialHarvestRequest.ExpectedRevision = InitialOption.TargetRef.Revision;
	InitialHarvestRequest.HarvestPower = 1.0f;
	TestTrue(
		TEXT("The direct request accepts the exact server-observed revision"),
		Component->CanAcceptHarvest_Implementation(InitialHarvestRequest));
	TestTrue(
		TEXT("First current-revision request atomically commits"),
		Component->CommitInteraction(AuthorityQuery, InitialOption));
	TestEqual(
		TEXT("One harvest grants the complete deterministic material batch"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		2);
	TestEqual(
		TEXT("One harvest awards skill XP exactly once"),
		TradeSkills->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		10.0f);
	TestFalse(TEXT("Successful delivery depletes the instance"), Component->IsResourceInstanceActive(0));
	TestEqual(TEXT("Depletion advances revision once"), Component->GetResourceInstanceRevision(0), 1);

	TestFalse(
		TEXT("A concurrent/replayed request cannot commit the depleted revision"),
		Component->CommitInteraction(AuthorityQuery, InitialOption));
	TestEqual(
		TEXT("Rejected replay grants no duplicate reward"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		2);
	TestEqual(
		TEXT("Rejected replay grants no duplicate XP"),
		TradeSkills->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		10.0f);

	TestWorld.AdvanceTimers(0.02f);
	TestTrue(TEXT("The timer queue reactivates the instance"), Component->IsResourceInstanceActive(0));
	TestEqual(TEXT("Respawn advances the stable revision"), Component->GetResourceInstanceRevision(0), 2);
	TestFalse(
		TEXT("CanAccept rejects a pre-respawn direct request against the fresh active revision"),
		Component->CanAcceptHarvest_Implementation(InitialHarvestRequest));
	TestFalse(
		TEXT("Commit rechecks and rejects a pre-respawn direct request against the fresh active revision"),
		Component->CommitHarvest_Implementation(InitialHarvestRequest));
	TestFalse(
		TEXT("The pre-respawn interaction revision remains stale after reactivation"),
		Component->CommitInteraction(AuthorityQuery, InitialOption));

	FInteractionOption FreshOption;
	if (!TestTrue(
		TEXT("Respawned resource produces a fresh interaction revision"),
		GatherInstanceOption(Component, Harvester, 0, FreshOption)))
	{
		return false;
	}
	FInteractionQuery FreshQuery =
		MakeAuthorityQuery(Component, Harvester, FreshOption);
	TestTrue(TEXT("Fresh respawn revision can be harvested once"), Component->CommitInteraction(FreshQuery, FreshOption));
	TestEqual(
		TEXT("Second lifecycle grants exactly one additional batch"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		4);
	TestEqual(
		TEXT("Second lifecycle grants exactly one additional XP award"),
		TradeSkills->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		20.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestOverflowWorldDropTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.CompleteBatchOverflowWorldDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestOverflowWorldDropTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancedMeshTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestableInstancedMeshActor* ResourceActor =
		SpawnThreeInstanceFixture(World);
	URpgHarvestableInstancedMeshComponent* Component =
		ResourceActor ? ResourceActor->HarvestableInstances : nullptr;
	ARpgHarvestAutomationTestPlayerState* Harvester =
		SpawnHarvester(World);
	if (!TestNotNull(TEXT("Overflow resource component exists"), Component) ||
		!TestNotNull(TEXT("Overflow harvester exists"), Harvester))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		Harvester->GetInventoryManagerComponent();
	Inventory->SetFixedMaxEntries(0);
	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	URpgHarvestProfile* Profile = MakeHarvestProfile(
		ResourceActor,
		{
			TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
				URpgHarvestAutomationTestStackItemDefinition::StaticClass(),
				3),
			TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
				URpgHarvestAutomationTestSecondMaterialDefinition::StaticClass(),
				2)
		},
		0.0f);
	if (!TestTrue(TEXT("Overflow profile is attached"), SetHarvestProfile(Component, Profile)))
	{
		return false;
	}

	FInteractionOption Option;
	if (!TestTrue(
		TEXT("Overflow resource gathers an option"),
		GatherInstanceOption(Component, Harvester, 1, Option)))
	{
		return false;
	}
	FInteractionQuery AuthorityQuery =
		MakeAuthorityQuery(Component, Harvester, Option);
	TestTrue(
		TEXT("Harvest succeeds through the world-drop fallback"),
		Component->CommitInteraction(AuthorityQuery, Option));
	TestEqual(
		TEXT("Overflow leaves the full player inventory untouched"),
		Inventory->GetUsedEntryCount(),
		0);
	TestEqual(TEXT("Exactly one replicated drop carries the complete batch"), CountWorldDrops(World), 1);

	ARpgDroppedInventoryActor* Drop = nullptr;
	for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
	{
		Drop = *It;
		break;
	}
	URpgInventoryManagerComponent* DropInventory =
		Drop ? Drop->GetLootInventoryManager() : nullptr;
	if (!TestNotNull(TEXT("Overflow drop owns its canonical inventory"), DropInventory))
	{
		return false;
	}
	TestEqual(
		TEXT("World drop contains every unit from the first reward row"),
		DropInventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		3);
	TestEqual(
		TEXT("World drop contains every unit from the second reward row"),
		DropInventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestSecondMaterialDefinition::StaticClass()),
		2);

	TestFalse(
		TEXT("Replaying the spent revision cannot spawn another overflow drop"),
		Component->CommitInteraction(AuthorityQuery, Option));
	TestEqual(TEXT("Rejected replay leaves one world drop"), CountWorldDrops(World), 1);
	TestEqual(
		TEXT("Overflow delivery still awards exactly one skill-XP grant"),
		Harvester->GetTradeSkillProgressionComponent()->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestSkillGateGatheringBonusTest,
	"SurvivalRpg.Interaction.Harvesting.HISM.MinimumSkillGateAndGatheringSetYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestSkillGateGatheringBonusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableInstancedMeshTests;

	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgHarvestableInstancedMeshActor* ResourceActor =
		SpawnThreeInstanceFixture(World);
	URpgHarvestableInstancedMeshComponent* Component =
		ResourceActor ? ResourceActor->HarvestableInstances : nullptr;
	ARpgHarvestAutomationTestPlayerState* Harvester =
		SpawnHarvester(World);
	if (!TestNotNull(TEXT("Skill-gated resource component exists"), Component) ||
		!TestNotNull(TEXT("Skill-gated harvester exists"), Harvester))
	{
		return false;
	}

	URpgHarvestProfile* Profile = MakeHarvestProfile(
		ResourceActor,
		{
			TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
				URpgHarvestAutomationTestStackItemDefinition::StaticClass(),
				2)
		},
		0.0f);
	Profile->MinimumSkillLevel = 2;
	Profile->LootTable->Groups[0].Entries[0].bScaleQuantityWithYield = true;
	if (!TestTrue(TEXT("Skill-gated harvest profile is attached"), SetHarvestProfile(Component, Profile)))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		Harvester->GetInventoryManagerComponent();
	URpgTradeSkillProgressionComponent* TradeSkills =
		Harvester->GetTradeSkillProgressionComponent();
	UAbilitySystemComponent* AbilitySystem = Harvester->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Skill-gated harvester owns an inventory"), Inventory) ||
		!TestNotNull(TEXT("Skill-gated harvester owns progression"), TradeSkills) ||
		!TestNotNull(TEXT("Skill-gated harvester owns an ASC"), AbilitySystem))
	{
		return false;
	}

	FInteractionOption Option;
	if (!TestTrue(
		TEXT("Active skill-gated resource still exposes its interaction option"),
		GatherInstanceOption(Component, Harvester, 2, Option)))
	{
		return false;
	}
	const FInteractionQuery AuthorityQuery =
		MakeAuthorityQuery(Component, Harvester, Option);
	TestFalse(
		TEXT("A level-one harvester cannot commit a level-two resource"),
		Component->CommitInteraction(AuthorityQuery, Option));
	TestEqual(
		TEXT("Rejected skill gate grants no material"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		0);
	TestEqual(
		TEXT("Rejected skill gate grants no XP"),
		TradeSkills->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		0.0f);
	TestTrue(TEXT("Rejected skill gate leaves the node active"), Component->IsResourceInstanceActive(2));
	TestEqual(TEXT("Rejected skill gate preserves the revision"), Component->GetResourceInstanceRevision(2), 0);

	FTradeSkillState ForagingState;
	ForagingState.SkillTag =
		RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
	ForagingState.Level = 2;
	ForagingState.XP = 0.0f;
	TestTrue(
		TEXT("Authoritative progression restores the required skill level"),
		TradeSkills->RestoreSkillStates({ForagingState}));

	URpgGatheringSet* GatheringSet =
		NewObject<URpgGatheringSet>(AbilitySystem);
	AbilitySystem->AddAttributeSetSubobject(GatheringSet);
	const float SkillYield = TradeSkills->GetSkillYieldMultiplier(
		RpgTradeSkillGameplayTags::Skill_Gathering_Foraging);
	const float GatheringBonus = 1.5f / SkillYield - 1.0f;
	AbilitySystem->SetNumericAttributeBase(
		URpgGatheringSet::GetYieldBonusAttribute(),
		GatheringBonus);
	if (!TestNotNull(
		TEXT("Gathering attributes are registered on the player-owned ASC"),
		AbilitySystem->GetSet<URpgGatheringSet>()))
	{
		return false;
	}

	TestTrue(
		TEXT("The same current revision commits after satisfying the skill gate"),
		Component->CommitInteraction(AuthorityQuery, Option));
	TestEqual(
		TEXT("Skill and GatheringSet multipliers scale the complete deterministic batch"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		3);
	TestEqual(
		TEXT("Successful gated harvest grants XP exactly once"),
		TradeSkills->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		10.0f);
	TestFalse(TEXT("Successful gated harvest depletes the node"), Component->IsResourceInstanceActive(2));
	TestEqual(TEXT("Successful gated harvest advances the revision once"), Component->GetResourceInstanceRevision(2), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
