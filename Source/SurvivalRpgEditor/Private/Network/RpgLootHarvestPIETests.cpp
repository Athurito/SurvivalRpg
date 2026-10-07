#include "Network/RpgLootHarvestNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "AbilitySystemComponent.h"
#include "Components/PIENetworkComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFeaturesSubsystem.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestableInstancedMeshComponent.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "Harvesting/RpgHarvestSwarm.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgGatheringSet.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgLootHarvestPIETests
{
	constexpr int32 LootQuantity = 1;
	constexpr int32 FirstOverflowQuantity = 3;
	constexpr int32 SecondOverflowQuantity = 2;
	constexpr int32 HarvestExperience = 10;
	constexpr int32 NodeSectionCount = 4;

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationLootFixture* LootFixture = nullptr;
		ARpgNetworkAutomationHarvesterState* Harvester = nullptr;
		ARpgNetworkAutomationHarvestFixture* HarvestFixture = nullptr;
		ARpgNetworkAutomationHarvestNodeFixture* HarvestNode = nullptr;
		ARpgNetworkAutomationHarvestInstancesFixture* InstancesField = nullptr;
		ARpgNetworkAutomationSwarm* Swarm = nullptr;
		APawn* StrideAvatar = nullptr;
		URpgNetworkAutomationStrideAbility* StrideAbility = nullptr;
		ARpgNetworkAutomationHarvesterState* SecondHarvester = nullptr;
		ARpgNetworkAutomationHarvestInstancesFixture* LoadField = nullptr;
		URpgNetworkAutomationStrideAbility* SecondStrideAbility = nullptr;
	};

	/** Authored instance locations of the instanced test field, relative to InstancesFieldLocation. */
	const TArray<FVector> InstancesFieldLocations = {
		FVector(0.0, 0.0, 0.0),
		FVector(300.0, 0.0, 0.0),
		FVector(600.0, 0.0, 0.0)};
	const FVector InstancesFieldLocation(4000.0, 4000.0, 0.0);

	/**
	 * Dense field of the co-op load tests: LoadFieldSize x LoadFieldSize instances, LoadFieldSpacing cm apart. Its middle
	 * lies near the clients' viewpoint, because swarms replicate only within their net cull distance.
	 */
	constexpr int32 LoadFieldSize = 60;
	constexpr double LoadFieldSpacing = 300.0;
	const FVector LoadFieldLocation(-6000.0, -6000.0, 0.0);

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	URpgLootTable* MakeGuaranteedLootTable(
		UObject* Outer,
		const TArray<TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>>& Rewards)
	{
		URpgLootTable* Table = NewObject<URpgLootTable>(Outer);
		if (!Table)
		{
			return nullptr;
		}

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
		return Table;
	}

	URpgHarvestProfile* MakeOverflowHarvestProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		if (!Profile)
		{
			return nullptr;
		}

		Profile->LootTable = MakeGuaranteedLootTable(
			Profile,
			{
				TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
					URpgNetworkAutomationMaterialDefinition::StaticClass(),
					FirstOverflowQuantity),
				TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
					URpgNetworkAutomationSecondMaterialDefinition::StaticClass(),
					SecondOverflowQuantity)
			});
		Profile->SkillTag =
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = HarvestExperience;
		Profile->MinimumRespawnSeconds = 0.0f;
		Profile->MaximumRespawnSeconds = 0.0f;
		Profile->OverflowDropClass = ARpgDroppedInventoryActor::StaticClass();
		return Profile;
	}

	URpgHarvestProfile* MakeSectionedNodeProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		if (!Profile)
		{
			return nullptr;
		}

		Profile->LootTable = MakeGuaranteedLootTable(
			Profile,
			{
				TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
					URpgNetworkAutomationMaterialDefinition::StaticClass(),
					LootQuantity)
			});
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Foraging;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = HarvestExperience;
		Profile->SectionCount = NodeSectionCount;
		Profile->MinimumRespawnSeconds = 0.0f;
		Profile->MaximumRespawnSeconds = 0.0f;
		return Profile;
	}

	FRpgHarvestRequest MakeNodeRequest(
		ARpgNetworkAutomationHarvestNodeFixture* Node,
		AActor* Harvester,
		const int32 RequestedSections)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		Request.Hit = FHitResult(
			Node,
			nullptr,
			Node ? Node->GetActorLocation() : FVector::ZeroVector,
			FVector::UpVector);
		Request.ExpectedRevision = Node && Node->GetHarvestableNode()
			? IRpgHarvestableTarget::Execute_GetHarvestRevision(Node->GetHarvestableNode(), Request.Hit)
			: INDEX_NONE;
		return Request;
	}

	bool HasReplicatedNodeState(
		const FNetworkState& State,
		const int32 ExpectedRemainingSections,
		const int32 ExpectedRevision,
		const bool bExpectedActive)
	{
		const URpgHarvestableComponent* Node = IsValid(State.HarvestNode)
			? State.HarvestNode->GetHarvestableNode()
			: nullptr;
		return Node && !State.HarvestNode->HasAuthority() &&
			Node->GetSectionCount() == NodeSectionCount &&
			Node->GetRemainingSections() == ExpectedRemainingSections &&
			Node->GetHarvestState().Revision == ExpectedRevision &&
			Node->GetHarvestState().bActive == bExpectedActive;
	}

	/** Loads the same instanced field on one machine, the way every machine loads a PCG partition actor. */
	ARpgNetworkAutomationHarvestInstancesFixture* LoadInstancesField(UWorld* World)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ARpgNetworkAutomationHarvestInstancesFixture* Field = World && Cube
			? World->SpawnActorDeferred<ARpgNetworkAutomationHarvestInstancesFixture>(
				ARpgNetworkAutomationHarvestInstancesFixture::StaticClass(),
				FTransform(InstancesFieldLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Field || !Field->ConfigureHarvestProfile(MakeSectionedNodeProfile(Field)))
		{
			return nullptr;
		}

		URpgHarvestableInstancesComponent* Instances = Field->GetHarvestableInstances();
		Instances->SetStaticMesh(Cube);
		for (const FVector& Location : InstancesFieldLocations)
		{
			Instances->AddInstance(FTransform(Location));
		}
		Field->FinishSpawning(FTransform(InstancesFieldLocation));
		return Field;
	}

	FRpgHarvestRequest MakeInstancesRequest(
		const FNetworkState& State,
		const int32 InstanceIndex,
		AActor* Harvester,
		const int32 RequestedSections)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		URpgHarvestableInstancesComponent* Instances =
			IsValid(State.InstancesField) ? State.InstancesField->GetHarvestableInstances() : nullptr;
		FTransform InstanceTransform;
		if (Instances && Instances->GetAuthoredInstanceTransform(InstanceIndex, InstanceTransform, true))
		{
			Request.Hit = FHitResult(State.InstancesField, Instances, InstanceTransform.GetLocation(), FVector::UpVector);
			Request.Hit.Item = InstanceIndex;
			Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Instances, Request.Hit);
		}
		return Request;
	}

	/** True when this machine presents the instance stock: remaining sections per instance, hidden when empty. */
	bool HasInstanceStock(
		const FNetworkState& State,
		const TArray<int32>& ExpectedRemainingSections,
		const int32 ExpectedChangedInstances)
	{
		const URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
		const URpgHarvestableInstancesComponent* Instances =
			IsValid(State.InstancesField) ? State.InstancesField->GetHarvestableInstances() : nullptr;
		if (!Stock || !Instances || Stock->GetNumChangedInstances() != ExpectedChangedInstances ||
			Instances->GetInstanceCount() != ExpectedRemainingSections.Num())
		{
			return false;
		}

		for (int32 InstanceIndex = 0; InstanceIndex < ExpectedRemainingSections.Num(); ++InstanceIndex)
		{
			FTransform Presented;
			if (Instances->GetRemainingSections(InstanceIndex) != ExpectedRemainingSections[InstanceIndex] ||
				!Instances->GetInstanceTransform(InstanceIndex, Presented, false) ||
				Presented.GetScale3D().IsNearlyZero() != (ExpectedRemainingSections[InstanceIndex] == 0))
			{
				return false;
			}
		}
		return true;
	}

	/** Returns the swarm replicated to or spawned in World that has not been destroyed, or null. */
	ARpgHarvestSwarm* FindSwarm(UWorld* World)
	{
		for (TActorIterator<ARpgHarvestSwarm> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
		return nullptr;
	}

	int32 CountSwarmCreatureActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgNetworkAutomationSwarmCreature> It(World); It; ++It)
		{
			Count += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
		return Count;
	}

	/** True when every creature actor is within a short flight distance of a flying creature's replicated location. */
	bool CreatureActorsFollowFlights(UWorld* World)
	{
		const ARpgHarvestSwarm* Swarm = FindSwarm(World);
		if (!Swarm || CountSwarmCreatureActors(World) == 0)
		{
			return false;
		}
		for (TActorIterator<ARpgNetworkAutomationSwarmCreature> It(World); It; ++It)
		{
			bool bFollows = false;
			for (int32 CreatureIndex = 0; CreatureIndex < Swarm->GetCreatures().Num(); ++CreatureIndex)
			{
				FVector Location;
				bFollows |= Swarm->GetCreatureLocation(CreatureIndex, Location) &&
					FVector::Dist(Location, It->GetActorLocation()) < 60.0;
			}
			if (!bFollows)
			{
				return false;
			}
		}
		return true;
	}

	bool IsServerReady(const FNetworkState& State, const int32 ExpectedClients)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_DedicatedServer ||
			!State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const UNetDriver* NetDriver = State.World->GetNetDriver();
		if (!GameState || !GameState->HasMatchStarted() || !NetDriver ||
			!NetDriver->IsServer() ||
			NetDriver->ClientConnections.Num() != ExpectedClients)
		{
			return false;
		}

		for (const UNetConnection* Connection : NetDriver->ClientConnections)
		{
			if (!IsValid(Connection) || !IsValid(Connection->ViewTarget))
			{
				return false;
			}
		}
		return true;
	}

	bool IsClientReady(const FNetworkState& State)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client ||
			!State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const APlayerController* PlayerController =
			State.World->GetFirstPlayerController();
		return GameState && GameState->HasMatchStarted() && PlayerController;
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

	ARpgDroppedInventoryActor* FindOnlyWorldDrop(UWorld* World)
	{
		ARpgDroppedInventoryActor* Result = nullptr;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (Result)
			{
				return nullptr;
			}
			Result = *It;
		}
		return Result;
	}

	bool HasCompleteOverflowDrop(UWorld* World)
	{
		ARpgDroppedInventoryActor* Drop = FindOnlyWorldDrop(World);
		URpgInventoryManagerComponent* DropInventory =
			Drop ? Drop->GetLootInventoryManager() : nullptr;
		return Drop && Drop->IsLootInventoryCanonical() && DropInventory &&
			DropInventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationMaterialDefinition::StaticClass()) ==
				FirstOverflowQuantity &&
			DropInventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationSecondMaterialDefinition::StaticClass()) ==
				SecondOverflowQuantity;
	}

	FRpgHarvestRequest MakeHarvestRequest(
		ARpgNetworkAutomationHarvestFixture* HarvestFixture,
		AActor* Harvester)
	{
		FRpgHarvestRequest Request;
		URpgHarvestableInstancedMeshComponent* Component = HarvestFixture
			? HarvestFixture->GetHarvestableInstances()
			: nullptr;
		FTransform InstanceTransform = FTransform::Identity;
		if (Component)
		{
			Component->GetInstanceTransform(0, InstanceTransform, true);
		}

		Request.Harvester = Harvester;
		Request.AbilityId =
			RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.TraceOrigin = Harvester
			? Harvester->GetActorLocation()
			: FVector::ZeroVector;
		Request.Hit = FHitResult(
			HarvestFixture,
			Component,
			InstanceTransform.GetLocation(),
			FVector::UpVector);
		Request.Hit.Item = 0;
		Request.ExpectedRevision = Component
			? Component->GetResourceInstanceRevision(0)
			: INDEX_NONE;
		Request.HarvestPower = 1.0f;
		return Request;
	}

	bool HasReplicatedLoot(
		const FNetworkState& State,
		const FRpgInventoryItemId& ExpectedItemId)
	{
		if (!IsValid(State.LootFixture) || State.LootFixture->HasAuthority())
		{
			return false;
		}

		const URpgInventoryManagerComponent* Inventory =
			State.LootFixture->GetInventory();
		if (!Inventory ||
			Inventory->GetTotalItemCountByDefinition(
				URpgNetworkAutomationMaterialDefinition::StaticClass()) !=
				LootQuantity)
		{
			return false;
		}

		const TArray<URpgInventoryItemInstance*> Items = Inventory->GetAllItems();
		return Items.Num() == 1 && IsValid(Items[0]) &&
			Items[0]->GetItemId() == ExpectedItemId;
	}

	bool HasReplicatedHarvestState(const FNetworkState& State)
	{
		const URpgHarvestableInstancedMeshComponent* Component =
			IsValid(State.HarvestFixture)
			? State.HarvestFixture->GetHarvestableInstances()
			: nullptr;
		return Component && !State.HarvestFixture->HasAuthority() &&
			Component->GetInstanceCount() == 1 &&
			!Component->IsResourceInstanceActive(0) &&
			Component->GetResourceInstanceRevision(0) == 1;
	}

	/** The Striding Wave cue tag from the project tags; editor modules cannot define native tags. */
	FGameplayTag StrideCueTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.StridingWave"));
	}

	/** Returns whether this machine's copy of the harvester's ability system shows the stride cue. */
	bool HasStrideCue(UWorld* World)
	{
		for (TActorIterator<ARpgNetworkAutomationHarvesterState> It(World); It; ++It)
		{
			const UAbilitySystemComponent* AbilitySystem = It->GetAbilitySystemComponent();
			return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(StrideCueTag());
		}
		return false;
	}

	/** Moves the server-only stride avatar to Offset from the instanced field. */
	void MoveStrideAvatar(const FNetworkState& State, const FVector& Offset)
	{
		if (IsValid(State.StrideAvatar))
		{
			State.StrideAvatar->SetActorLocation(InstancesFieldLocation + Offset);
		}
	}

	/** Loads the co-op load test's dense field on one machine, the way every machine loads a PCG partition actor. */
	ARpgNetworkAutomationHarvestInstancesFixture* LoadDenseField(UWorld* World)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ARpgNetworkAutomationHarvestInstancesFixture* Field = World && Cube
			? World->SpawnActorDeferred<ARpgNetworkAutomationHarvestInstancesFixture>(
				ARpgNetworkAutomationHarvestInstancesFixture::StaticClass(),
				FTransform(LoadFieldLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Field || !Field->ConfigureHarvestProfile(MakeSectionedNodeProfile(Field)))
		{
			return nullptr;
		}

		URpgHarvestableInstancesComponent* Instances = Field->GetHarvestableInstances();
		Instances->SetStaticMesh(Cube);
		TArray<FTransform> Transforms;
		Transforms.Reserve(LoadFieldSize * LoadFieldSize);
		for (int32 Row = 0; Row < LoadFieldSize; ++Row)
		{
			for (int32 Column = 0; Column < LoadFieldSize; ++Column)
			{
				Transforms.Emplace(FVector(Column * LoadFieldSpacing, Row * LoadFieldSpacing, 0.0));
			}
		}
		Instances->AddInstances(Transforms, false);
		Field->FinishSpawning(FTransform(LoadFieldLocation));
		return Field;
	}

	/** Writes the remaining sections of every instance of Field as this machine presents them; false if one is off. */
	bool GetPresentedFieldStock(const ARpgNetworkAutomationHarvestInstancesFixture* Field, TArray<int32>& OutRemaining)
	{
		OutRemaining.Reset();
		const URpgHarvestableInstancesComponent* Instances = IsValid(Field) ? Field->GetHarvestableInstances() : nullptr;
		if (!Instances)
		{
			return false;
		}

		OutRemaining.Reserve(Instances->GetInstanceCount());
		for (int32 InstanceIndex = 0; InstanceIndex < Instances->GetInstanceCount(); ++InstanceIndex)
		{
			FTransform Presented;
			const int32 Remaining = Instances->GetRemainingSections(InstanceIndex);
			if (!Instances->GetInstanceTransform(InstanceIndex, Presented, false) ||
				Presented.GetScale3D().IsNearlyZero() != (Remaining == 0))
			{
				return false;
			}
			OutRemaining.Add(Remaining);
		}
		return true;
	}

	/** True when this machine stores ExpectedChanged changed instances and presents Field with ExpectedRemaining. */
	bool HasDenseFieldStock(const FNetworkState& State, const TArray<int32>& ExpectedRemaining, const int32 ExpectedChanged)
	{
		const URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
		TArray<int32> Presented;
		return Stock && Stock->GetNumChangedInstances() == ExpectedChanged &&
			GetPresentedFieldStock(State.LoadField, Presented) && Presented == ExpectedRemaining;
	}

	/**
	 * Gives Harvester a server-only avatar at Location and a running stride that takes Sections sections from up to four
	 * targets within Radius cm every PulseSeconds, like Striding Wave. Returns the granted stride.
	 */
	URpgNetworkAutomationStrideAbility* StartStride(
		UWorld* World,
		ARpgNetworkAutomationHarvesterState* Harvester,
		const FVector& Location,
		const float Radius,
		const float DurationSeconds,
		const float PulseSeconds,
		const int32 Sections)
	{
		APawn* Avatar = World && Harvester
			? World->SpawnActorDeferred<APawn>(
				APawn::StaticClass(),
				FTransform(Location),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Avatar)
		{
			return nullptr;
		}
		Avatar->SetReplicates(false);
		USceneComponent* Root = NewObject<USceneComponent>(Avatar, TEXT("StrideAvatarRoot"));
		Avatar->SetRootComponent(Root);
		Avatar->FinishSpawning(FTransform(Location));
		Root->RegisterComponent();
		Avatar->SetActorLocation(Location);
		Avatar->SetPlayerState(Harvester);

		UAbilitySystemComponent* AbilitySystem = Harvester->GetAbilitySystemComponent();
		if (!AbilitySystem)
		{
			return nullptr;
		}
		AbilitySystem->InitAbilityActorInfo(Harvester, Avatar);
		const FGameplayAbilitySpecHandle Handle =
			AbilitySystem->GiveAbility(FGameplayAbilitySpec(URpgNetworkAutomationStrideAbility::StaticClass()));
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
		URpgNetworkAutomationStrideAbility* Stride =
			Spec ? Cast<URpgNetworkAutomationStrideAbility>(Spec->GetPrimaryInstance()) : nullptr;
		if (!Stride)
		{
			return nullptr;
		}
		Stride->ConfigureStride(Radius, DurationSeconds, PulseSeconds, Sections, FGameplayTag());
		return AbilitySystem->TryActivateAbility(Handle) && Stride->IsStriding() ? Stride : nullptr;
	}

	/** Builds a request for instance InstanceIndex of the dense field, like MakeInstancesRequest does for the small one. */
	FRpgHarvestRequest MakeDenseFieldRequest(
		const FNetworkState& State,
		const int32 InstanceIndex,
		AActor* Harvester,
		const int32 RequestedSections)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = RpgHarvestingMagicGameplayTags::Ability_Harvesting_Manual;
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		URpgHarvestableInstancesComponent* Instances =
			IsValid(State.LoadField) ? State.LoadField->GetHarvestableInstances() : nullptr;
		FTransform InstanceTransform;
		if (Instances && Instances->GetAuthoredInstanceTransform(InstanceIndex, InstanceTransform, true))
		{
			Request.Hit = FHitResult(State.LoadField, Instances, InstanceTransform.GetLocation(), FVector::UpVector);
			Request.Hit.Item = InstanceIndex;
			Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Instances, Request.Hit);
		}
		return Request;
	}

	/** Summons a swarm of 16 fast creatures for Harvester over the dense field instances InstanceIndices. */
	ARpgNetworkAutomationSwarm* SummonDenseFieldSwarm(
		const FNetworkState& State,
		ARpgNetworkAutomationHarvesterState* Harvester,
		const TArray<int32>& InstanceIndices,
		const FVector& Location)
	{
		URpgHarvestableInstancesComponent* Instances = State.LoadField->GetHarvestableInstances();
		TArray<FRpgHarvestTargetEvaluation> Targets;
		for (const int32 InstanceIndex : InstanceIndices)
		{
			const FRpgHarvestRequest TargetRequest = MakeDenseFieldRequest(State, InstanceIndex, Harvester, 1);
			FRpgHarvestTargetEvaluation& Target = Targets.AddDefaulted_GetRef();
			Target.Receiver = Instances;
			Target.Hit = TargetRequest.Hit;
			Target.bInReach = true;
			Target.Result = Instances->EvaluateHarvest_Implementation(TargetRequest);
		}

		FRpgHarvestSwarmParams Params;
		Params.CreatureCount = 16;
		Params.FlightSpeed = 1500.0f;
		Params.EmergeSeconds = 0.2f;
		Params.LaunchIntervalSeconds = 0.05f;
		Params.StrikeIntervalSeconds = 0.2f;
		Params.MaxReassignments = 8;
		Params.MaxLifetimeSeconds = 30.0f;
		Params.bRequireLineOfSight = false;

		FRpgHarvestRequest RequestTemplate = MakeDenseFieldRequest(State, InstanceIndices[0], Harvester, 1);
		RequestTemplate.Hit = FHitResult();
		RequestTemplate.ExpectedRevision = INDEX_NONE;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgNetworkAutomationSwarm* Swarm = State.World->SpawnActor<ARpgNetworkAutomationSwarm>(
			ARpgNetworkAutomationSwarm::StaticClass(),
			FTransform(Location),
			SpawnParameters);
		return Swarm && Swarm->StartSwarm(Harvester, RequestTemplate, Params, Targets) ? Swarm : nullptr;
	}

	/** Returns the number of swarms replicated to or spawned in World that have not been destroyed. */
	int32 CountSwarms(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgHarvestSwarm> It(World); It; ++It)
		{
			Count += IsValid(*It) && !It->IsActorBeingDestroyed() ? 1 : 0;
		}
		return Count;
	}

	/** Delays and drops packets this machine sends, like a distant player on a poor connection. */
	void EmulatePoorConnection(UWorld* World, const int32 LagMilliseconds, const int32 LossPercent)
	{
#if DO_ENABLE_NET_TEST
		if (UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr)
		{
			FPacketSimulationSettings Settings;
			Settings.PktLag = LagMilliseconds;
			Settings.PktLoss = LossPercent;
			NetDriver->SetPacketSimulationSettings(Settings);
		}
#endif
	}

	/** Returns the bytes the server has sent so far over its ConnectionIndex-th client connection, or -1. */
	int32 GetBytesSentToClient(const FNetworkState& State, const int32 ConnectionIndex)
	{
		const UNetDriver* NetDriver = IsValid(State.World) ? State.World->GetNetDriver() : nullptr;
		const UNetConnection* Connection = NetDriver && NetDriver->ClientConnections.IsValidIndex(ConnectionIndex)
			? NetDriver->ClientConnections[ConnectionIndex].Get()
			: nullptr;
		return Connection ? Connection->OutTotalBytes : -1;
	}

	int32 CountGatheringSets(const FNetworkState& State)
	{
		const UAbilitySystemComponent* AbilitySystem = IsValid(State.Harvester)
			? State.Harvester->GetAbilitySystemComponent()
			: nullptr;
		if (!AbilitySystem)
		{
			return 0;
		}

		int32 Count = 0;
		for (const UAttributeSet* AttributeSet : AbilitySystem->GetSpawnedAttributes())
		{
			if (IsValid(AttributeSet) && AttributeSet->IsA<URpgGatheringSet>())
			{
				++Count;
			}
		}
		return Count;
	}
}

NETWORK_TEST_CLASS(LootHarvestPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgLootHarvestPIETests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FRpgInventoryItemId LootItemId;
	FString HarvestingPluginURL;
	bool bPluginTransitionComplete = false;
	bool bPluginTransitionSucceeded = false;

	/** Co-op load test: the server's stock of the dense field, which every client must end up presenting. */
	TArray<int32> ExpectedLoadFieldStock;
	int32 ExpectedChangedInstances = 0;
	int32 BytesBeforeMassChange = 0;
	double MassChangeStartSeconds = 0.0;
	double LateJoinStartSeconds = 0.0;
	int32 BytesBeforeSwarms = 0;
	double SwarmStartSeconds = 0.0;

	BEFORE_EACH()
	{
		HarvestingPluginURL.Reset();
		bPluginTransitionComplete = false;
		bPluginTransitionSucceeded = false;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(ServerAuthorityOverflowReplicationAndLateJoin)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server match and first connection are ready"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client owns a ready pawn"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationLootFixture,
				&FNetworkState::LootFixture>(
				[this](ARpgNetworkAutomationLootFixture& Fixture)
				{
					URpgLootTable* Table = MakeGuaranteedLootTable(
						&Fixture,
						{
							TPair<TSubclassOf<URpgInventoryItemDefinition>, int32>(
								URpgNetworkAutomationMaterialDefinition::StaticClass(),
								LootQuantity)
						});
					if (!Table || !Fixture.GetLootSource())
					{
						TestRunner->AddError(
							TEXT("Failed to configure the transient loot-source fixture."));
						return;
					}
					Fixture.GetLootSource()->ConfigureLootTable(Table);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Client cannot populate authoritative loot"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.LootFixture));
					ASSERT_THAT(IsNotNull(State.LootFixture->GetInventory()));
					ASSERT_THAT(IsNotNull(State.LootFixture->GetLootSource()));
					ASSERT_THAT(IsTrue(!State.LootFixture->HasAuthority()));
					State.LootFixture->GetLootSource()->PopulateLoot();
					ASSERT_THAT(AreEqual(
						State.LootFixture->GetInventory()
							->GetTotalItemCountByDefinition(
								URpgNetworkAutomationMaterialDefinition::StaticClass()),
						0));
				})
			.ThenServer(
				TEXT("Server populates the loot source exactly once"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.LootFixture));
					ASSERT_THAT(IsTrue(State.LootFixture->HasAuthority()));
					URpgInventoryManagerComponent* Inventory =
						State.LootFixture->GetInventory();
					URpgNetworkAutomationLootSourceComponent* LootSource =
						State.LootFixture->GetLootSource();
					ASSERT_THAT(IsNotNull(Inventory));
					ASSERT_THAT(IsNotNull(LootSource));

					LootSource->PopulateLoot();
					LootSource->PopulateLoot();
					ASSERT_THAT(AreEqual(
						Inventory->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						LootQuantity));
					const TArray<URpgInventoryItemInstance*> Items =
						Inventory->GetAllItems();
					ASSERT_THAT(AreEqual(Items.Num(), 1));
					ASSERT_THAT(IsNotNull(Items[0]));
					LootItemId = Items[0]->GetItemId();
					ASSERT_THAT(IsTrue(LootItemId.IsValid()));
				})
			.UntilClient(
				TEXT("Initial client receives the concrete loot item"),
				0,
				[this](FNetworkState& State)
				{
					return HasReplicatedLoot(State, LootItemId);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[this](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					URpgInventoryManagerComponent* Inventory =
						Harvester.GetInventoryManagerComponent();
					if (!Inventory)
					{
						TestRunner->AddError(
							TEXT("Harvester fixture has no inventory manager."));
						return;
					}
					Inventory->SetFixedMaxEntries(0);
					Inventory->SetCapacityMode(
						ERpgInventoryCapacityMode::FixedEntries);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvestFixture,
				&FNetworkState::HarvestFixture>(
				[this](ARpgNetworkAutomationHarvestFixture& Fixture)
				{
					URpgHarvestProfile* Profile =
						MakeOverflowHarvestProfile(&Fixture);
					if (!Profile || !Fixture.ConfigureHarvestProfile(Profile))
					{
						TestRunner->AddError(
							TEXT("Failed to configure the transient harvest profile."));
					}
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Client cannot commit the harvest"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestFixture));
					ASSERT_THAT(IsNotNull(State.Harvester));
					URpgHarvestableInstancedMeshComponent* Component =
						State.HarvestFixture->GetHarvestableInstances();
					ASSERT_THAT(IsNotNull(Component));
					const FRpgHarvestRequest Request = MakeHarvestRequest(
						State.HarvestFixture,
						State.Harvester);
					ASSERT_THAT(IsTrue(
						!Component->CommitHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(IsTrue(Component->IsResourceInstanceActive(0)));
					ASSERT_THAT(AreEqual(
						Component->GetResourceInstanceRevision(0),
						0));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
				})
			.ThenServer(
				TEXT("Server harvest atomically creates one complete overflow drop"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestFixture));
					ASSERT_THAT(IsNotNull(State.Harvester));
					URpgHarvestableInstancedMeshComponent* Component =
						State.HarvestFixture->GetHarvestableInstances();
					URpgInventoryManagerComponent* HarvesterInventory =
						State.Harvester->GetInventoryManagerComponent();
					URpgTradeSkillProgressionComponent* TradeSkills =
						State.Harvester->GetTradeSkillProgressionComponent();
					ASSERT_THAT(IsNotNull(Component));
					ASSERT_THAT(IsNotNull(HarvesterInventory));
					ASSERT_THAT(IsNotNull(TradeSkills));

					const FRpgHarvestRequest Request = MakeHarvestRequest(
						State.HarvestFixture,
						State.Harvester);
					ASSERT_THAT(IsTrue(
						Component->EvaluateHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(IsTrue(
						Component->CommitHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(IsTrue(
						!Component->IsResourceInstanceActive(0)));
					ASSERT_THAT(AreEqual(
						Component->GetResourceInstanceRevision(0),
						1));
					ASSERT_THAT(AreEqual(
						HarvesterInventory->GetUsedEntryCount(),
						0));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						TradeSkills->GetSkillXPByTag(
							RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
						static_cast<float>(HarvestExperience))));

					ASSERT_THAT(IsTrue(
						!Component->CommitHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(
						TradeSkills->GetSkillXPByTag(
							RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
						static_cast<float>(HarvestExperience))));
				})
			.UntilClient(
				TEXT("Initial client receives depleted HISM and complete overflow"),
				0,
				[](FNetworkState& State)
				{
					return HasReplicatedHarvestState(State) &&
						HasCompleteOverflowDrop(State.World);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second server connection"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late-joining PIE client owns a ready pawn"),
				1,
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late join reconstructs loot, harvest state, and overflow"),
				1,
				[this](FNetworkState& State)
				{
					return HasReplicatedLoot(State, LootItemId) &&
						HasReplicatedHarvestState(State) &&
						HasCompleteOverflowDrop(State.World);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Late join never re-rolls or re-grants rewards"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(IsServerReady(State, 2)));
					ASSERT_THAT(AreEqual(
						State.LootFixture->GetInventory()
							->GetTotalItemCountByDefinition(
								URpgNetworkAutomationMaterialDefinition::StaticClass()),
						LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 1));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
				})
			.ThenClients(
				TEXT("Both clients expose identical replicated terminal state"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(IsClientReady(State)));
					ASSERT_THAT(IsTrue(
						HasReplicatedLoot(State, LootItemId)));
					ASSERT_THAT(IsTrue(HasReplicatedHarvestState(State)));
					ASSERT_THAT(IsTrue(HasCompleteOverflowDrop(State.World)));
				});
	}

	TEST_METHOD(ActorNodeSectionsReplicateAndLateJoin)
	{
		using namespace RpgLootHarvestPIETests;

		// SpawnAndReplicate begins play before the fixture's profile is assigned; content assigns it beforehand.
		TestRunner->AddExpectedMessage(
			TEXT("has no harvest profile and rejects every harvest request"),
			ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains,
			1);

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the node test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the node test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvestNodeFixture,
				&FNetworkState::HarvestNode>(
				[this](ARpgNetworkAutomationHarvestNodeFixture& Node)
				{
					if (!Node.ConfigureHarvestProfile(MakeSectionedNodeProfile(&Node)))
					{
						TestRunner->AddError(TEXT("Failed to configure the server node profile."));
					}
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Clients load the same static node profile and see the full stock"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestNode));
					ASSERT_THAT(IsTrue(State.HarvestNode->ConfigureHarvestProfile(
						MakeSectionedNodeProfile(State.HarvestNode))));
					ASSERT_THAT(IsTrue(HasReplicatedNodeState(State, NodeSectionCount, 0, true)));
				})
			.ThenClient(
				TEXT("A client cannot extract stock"),
				0,
				[this](FNetworkState& State)
				{
					URpgHarvestableComponent* Node = State.HarvestNode->GetHarvestableNode();
					const FRpgHarvestRequest Request = MakeNodeRequest(State.HarvestNode, State.Harvester, 1);
					ASSERT_THAT(IsTrue(Node->EvaluateHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(IsTrue(
						Node->CommitHarvest_Implementation(Request).Outcome == ERpgHarvestOutcome::Invalid));
					ASSERT_THAT(AreEqual(Node->GetRemainingSections(), NodeSectionCount));
				})
			.ThenServer(
				TEXT("Two concurrent server hits share the stock of the dormant node"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.HarvestNode));
					ASSERT_THAT(IsNotNull(State.Harvester));
					URpgHarvestableComponent* Node = State.HarvestNode->GetHarvestableNode();
					const FRpgHarvestRequest First = MakeNodeRequest(State.HarvestNode, State.Harvester, 1);
					const FRpgHarvestRequest Second = MakeNodeRequest(State.HarvestNode, State.Harvester, 1);
					ASSERT_THAT(IsTrue(Node->CommitHarvest_Implementation(First).IsSuccess()));
					ASSERT_THAT(IsTrue(Node->CommitHarvest_Implementation(Second).IsSuccess()));
					ASSERT_THAT(AreEqual(Node->GetRemainingSections(), NodeSectionCount - 2));
					ASSERT_THAT(AreEqual(Node->GetHarvestState().Revision, 0));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						2 * LootQuantity));
				})
			.UntilClient(
				TEXT("The flushed dormant node replicates its partial stock"),
				0,
				[](FNetworkState& State)
				{
					return HasReplicatedNodeState(State, NodeSectionCount - 2, 0, true);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second connection for the node test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late-joining client is ready and received the node"),
				1,
				[](FNetworkState& State)
				{
					return IsClientReady(State) && IsValid(State.HarvestNode);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Late-joining client loads the static node profile"),
				1,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(State.HarvestNode->ConfigureHarvestProfile(
						MakeSectionedNodeProfile(State.HarvestNode))));
				})
			.UntilClient(
				TEXT("Late join reconstructs the partial stock"),
				1,
				[](FNetworkState& State)
				{
					return HasReplicatedNodeState(State, NodeSectionCount - 2, 0, true);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("An oversized request takes the remaining stock and depletes the node"),
				[this](FNetworkState& State)
				{
					URpgHarvestableComponent* Node = State.HarvestNode->GetHarvestableNode();
					const FRpgHarvestResult Result =
						Node->CommitHarvest_Implementation(MakeNodeRequest(State.HarvestNode, State.Harvester, 3));
					ASSERT_THAT(IsTrue(Result.IsSuccess()));
					ASSERT_THAT(AreEqual(Result.SectionsTaken, 2));
					ASSERT_THAT(IsTrue(Result.bDepleted));
					ASSERT_THAT(AreEqual(Node->GetHarvestState().Revision, 1));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						NodeSectionCount * LootQuantity));
				})
			.UntilClients(
				TEXT("Both clients see the depleted node"),
				[](FNetworkState& State)
				{
					return HasReplicatedNodeState(State, 0, 1, false);
				},
				NetworkTimeout());
	}

	TEST_METHOD(InstanceStockReplicatesThroughGameStateAndLateJoins)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the instance test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the instance test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The harvesting GameFeature adds the instance stock to the GameState"),
				[this](FNetworkState& State)
				{
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
					ASSERT_THAT(IsTrue(Stock->HasStockAuthority()));
				})
			.UntilClients(
				TEXT("Clients receive the replicated instance stock"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the instanced field"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenClients(
				TEXT("Clients load the same field without replicating it"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
					ASSERT_THAT(IsFalse(State.InstancesField->GetIsReplicated()));
					ASSERT_THAT(IsTrue(HasInstanceStock(State, {NodeSectionCount, NodeSectionCount, NodeSectionCount}, 0)));
				})
			.ThenClient(
				TEXT("A client cannot extract instance stock although it owns its local field"),
				0,
				[this](FNetworkState& State)
				{
					URpgHarvestableInstancesComponent* Instances = State.InstancesField->GetHarvestableInstances();
					const FRpgHarvestRequest Request = MakeInstancesRequest(State, 0, State.Harvester, 1);
					ASSERT_THAT(IsTrue(State.InstancesField->HasAuthority()));
					ASSERT_THAT(IsTrue(Instances->EvaluateHarvest_Implementation(Request).IsSuccess()));
					ASSERT_THAT(IsTrue(Instances->CommitHarvest_Implementation(Request).Outcome == ERpgHarvestOutcome::Invalid));
					ASSERT_THAT(AreEqual(Instances->GetRemainingSections(0), NodeSectionCount));
				})
			.ThenServer(
				TEXT("The server empties one instance and harvests a section of another"),
				[this](FNetworkState& State)
				{
					URpgHarvestableInstancesComponent* Instances = State.InstancesField->GetHarvestableInstances();
					ASSERT_THAT(IsTrue(Instances->CommitHarvest_Implementation(
						MakeInstancesRequest(State, 0, State.Harvester, NodeSectionCount)).bDepleted));
					ASSERT_THAT(IsTrue(Instances->CommitHarvest_Implementation(
						MakeInstancesRequest(State, 1, State.Harvester, 1)).IsSuccess()));
					ASSERT_THAT(IsTrue(HasInstanceStock(State, {0, NodeSectionCount - 1, NodeSectionCount}, 2)));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						(NodeSectionCount + 1) * LootQuantity));
				})
			.UntilClient(
				TEXT("The client presents the replicated instance stock"),
				0,
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount - 1, NodeSectionCount}, 2);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second connection for the instance test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late-joining client is ready and received the instance stock"),
				1,
				[](FNetworkState& State)
				{
					const URpgHarvestInstanceStockComponent* Stock =
						URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					return IsClientReady(State) && Stock && Stock->GetNumChangedInstances() == 2;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The late joiner streams in the field and presents the stored stock"),
				1,
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
					ASSERT_THAT(IsTrue(HasInstanceStock(State, {0, NodeSectionCount - 1, NodeSectionCount}, 2)));
				})
			.ThenServer(
				TEXT("A respawn restores the empty instance"),
				[this](FNetworkState& State)
				{
					URpgHarvestableInstancesComponent* Instances = State.InstancesField->GetHarvestableInstances();
					URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					FIntVector Key;
					ASSERT_THAT(IsNotNull(Stock));
					ASSERT_THAT(IsTrue(Instances->GetInstanceKey(0, Key)));
					ASSERT_THAT(IsTrue(Stock->RestoreStock(Key)));
					ASSERT_THAT(IsTrue(HasInstanceStock(State, {NodeSectionCount, NodeSectionCount - 1, NodeSectionCount}, 1)));
				})
			.UntilClients(
				TEXT("Both clients show the restored instance and keep the partial one"),
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {NodeSectionCount, NodeSectionCount - 1, NodeSectionCount}, 1);
				},
				NetworkTimeout());
	}

	TEST_METHOD(SwarmReplicatesFlightsAndLateJoins)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the swarm test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the swarm test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The harvesting GameFeature adds the instance stock for the swarm"),
				[this](FNetworkState& State)
				{
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
				})
			.UntilClients(
				TEXT("Clients receive the instance stock for the swarm"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the instanced field for the swarm"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenClients(
				TEXT("Clients load the field the swarm will harvest"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenServer(
				TEXT("The server summons a slow swarm of three creatures over the field"),
				[this](FNetworkState& State)
				{
					// The swarm works on the first instance only: two creatures share it, the third finds nothing.
					URpgHarvestableInstancesComponent* Instances = State.InstancesField->GetHarvestableInstances();
					TArray<FRpgHarvestTargetEvaluation> Targets;
					const FRpgHarvestRequest TargetRequest = MakeInstancesRequest(State, 0, State.Harvester, 2);
					FRpgHarvestTargetEvaluation& Target = Targets.AddDefaulted_GetRef();
					Target.Receiver = Instances;
					Target.Hit = TargetRequest.Hit;
					Target.bInReach = true;
					Target.Result = Instances->EvaluateHarvest_Implementation(TargetRequest);

					// Departures are four seconds apart, so a late joiner arrives while the swarm is still at work.
					FRpgHarvestSwarmParams Params;
					Params.CreatureCount = 3;
					Params.FlightSpeed = 300.0f;
					Params.EmergeSeconds = 1.0f;
					Params.LaunchIntervalSeconds = 4.0f;
					Params.MaxReassignments = 0;
					Params.MaxLifetimeSeconds = 20.0f;
					Params.bRequireLineOfSight = false;

					FRpgHarvestRequest RequestTemplate = MakeInstancesRequest(State, 0, State.Harvester, 2);
					RequestTemplate.Hit = FHitResult();
					RequestTemplate.ExpectedRevision = INDEX_NONE;
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					ARpgNetworkAutomationSwarm* Swarm = State.World->SpawnActor<ARpgNetworkAutomationSwarm>(
						ARpgNetworkAutomationSwarm::StaticClass(),
						FTransform(InstancesFieldLocation + FVector(150.0, 300.0, 0.0)),
						SpawnParameters);
					ASSERT_THAT(IsNotNull(Swarm));
					ASSERT_THAT(IsTrue(Swarm->StartSwarm(State.Harvester, RequestTemplate, Params, Targets)));
					ASSERT_THAT(AreEqual(Swarm->GetCreatures().Num(), 3));
					ASSERT_THAT(IsTrue(Swarm->GetCreatures()[0].State == ERpgHarvestSwarmCreatureState::Flying));
					ASSERT_THAT(IsTrue(Swarm->GetCreatures()[1].State == ERpgHarvestSwarmCreatureState::Flying));
					ASSERT_THAT(IsTrue(Swarm->GetCreatures()[2].State == ERpgHarvestSwarmCreatureState::Searching));
					ASSERT_THAT(IsTrue(Swarm->GetBeneficiary() == State.Harvester));
					// A dedicated server has no players to present creatures to.
					ASSERT_THAT(AreEqual(CountSwarmCreatureActors(State.World), 0));
					State.Swarm = Swarm;
				})
			.UntilClients(
				TEXT("Clients receive the swarm and present one creature actor per flying creature"),
				[](FNetworkState& State)
				{
					const ARpgHarvestSwarm* Swarm = FindSwarm(State.World);
					return Swarm && Swarm->GetCreatures().Num() == 3 && CountSwarmCreatureActors(State.World) == 3;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The client moves each creature along its replicated flight"),
				0,
				[](FNetworkState& State)
				{
					return CreatureActorsFollowFlights(State.World);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("The first creature harvests its instance when it arrives"),
				[](FNetworkState& State)
				{
					return IsValid(State.Swarm) &&
						State.Swarm->GetCreatures()[0].Strikes == 1 &&
						HasInstanceStock(State, {2, NodeSectionCount, NodeSectionCount}, 1);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The first creature found nothing more to take; the others are still at work"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(State.Swarm->GetCreatures()[0].State == ERpgHarvestSwarmCreatureState::Harvested));
					ASSERT_THAT(IsFalse(State.Swarm->GetCreatures()[1].IsFinished()));
					ASSERT_THAT(IsFalse(State.Swarm->GetCreatures()[2].IsFinished()));
				})
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second connection for the swarm test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The late joiner receives the swarm at work"),
				1,
				[](FNetworkState& State)
				{
					const ARpgHarvestSwarm* Swarm = FindSwarm(State.World);
					return IsClientReady(State) && Swarm && Swarm->GetCreatures().Num() == 3;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The late joiner presents only creatures that have not finished"),
				1,
				[this](FNetworkState& State)
				{
					const ARpgHarvestSwarm* Swarm = FindSwarm(State.World);
					ASSERT_THAT(IsNotNull(Swarm));
					ASSERT_THAT(IsTrue(Swarm->GetCreatures()[0].IsFinished()));
					int32 Unfinished = 0;
					for (const FRpgHarvestSwarmCreature& Creature : Swarm->GetCreatures())
					{
						Unfinished += Creature.IsFinished() ? 0 : 1;
					}
					ASSERT_THAT(AreEqual(CountSwarmCreatureActors(State.World), Unfinished));

					// The late joiner streams in the field the swarm works on, as every machine loads PCG partitions.
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.UntilServer(
				TEXT("The swarm emptied its instance and finished"),
				[](FNetworkState& State)
				{
					return IsValid(State.Swarm) && State.Swarm->IsFinished() &&
						HasInstanceStock(State, {0, NodeSectionCount, NodeSectionCount}, 1);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The summoner received the rewards of all strikes in its inventory"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(State.Swarm->GetDelivery() == ERpgHarvestDelivery::Inventory));
					ASSERT_THAT(AreEqual(State.Swarm->GetHarvestedSections(), NodeSectionCount));
					ASSERT_THAT(IsTrue(State.Swarm->GetCreatures()[2].State == ERpgHarvestSwarmCreatureState::Dissipated));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						NodeSectionCount * LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
				})
			.UntilClients(
				TEXT("Both clients show the harvested field and retire the swarm and its creatures"),
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount, NodeSectionCount}, 1) &&
						FindSwarm(State.World) == nullptr &&
						CountSwarmCreatureActors(State.World) == 0;
				},
				NetworkTimeout());
	}

	TEST_METHOD(StrideHarvestsAlongThePathAndReplicatesItsCue)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the stride test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the stride test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The harvesting GameFeature adds the instance stock for the stride"),
				[this](FNetworkState& State)
				{
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
				})
			.UntilClients(
				TEXT("Clients receive the instance stock for the stride"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the field the stride walks through"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenClients(
				TEXT("Clients load the field the stride walks through"),
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenServer(
				TEXT("The harvester starts a stride before the field"),
				[this](FNetworkState& State)
				{
					// A server-only avatar walks for the harvester; the player state receives the rewards.
					APawn* Avatar = State.World->SpawnActorDeferred<APawn>(
						APawn::StaticClass(),
						FTransform(InstancesFieldLocation + FVector(-400.0, 0.0, 0.0)),
						nullptr,
						nullptr,
						ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					ASSERT_THAT(IsNotNull(Avatar));
					Avatar->SetReplicates(false);
					USceneComponent* Root = NewObject<USceneComponent>(Avatar, TEXT("StrideAvatarRoot"));
					Avatar->SetRootComponent(Root);
					Avatar->FinishSpawning(FTransform(InstancesFieldLocation + FVector(-400.0, 0.0, 0.0)));
					Root->RegisterComponent();
					Avatar->SetActorLocation(InstancesFieldLocation + FVector(-400.0, 0.0, 0.0));
					Avatar->SetPlayerState(State.Harvester);
					State.StrideAvatar = Avatar;

					UAbilitySystemComponent* AbilitySystem = State.Harvester->GetAbilitySystemComponent();
					ASSERT_THAT(IsNotNull(AbilitySystem));
					AbilitySystem->InitAbilityActorInfo(State.Harvester, Avatar);
					const FGameplayAbilitySpecHandle Handle =
						AbilitySystem->GiveAbility(FGameplayAbilitySpec(URpgNetworkAutomationStrideAbility::StaticClass()));
					const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
					State.StrideAbility = Spec ? Cast<URpgNetworkAutomationStrideAbility>(Spec->GetPrimaryInstance()) : nullptr;
					ASSERT_THAT(IsNotNull(State.StrideAbility));

					// 200 cm reach one instance at a time; the 15 s stride outlasts a late join.
					State.StrideAbility->ConfigureStride(200.0f, 15.0f, 0.25f, NodeSectionCount, StrideCueTag());
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Handle)));
					ASSERT_THAT(IsTrue(State.StrideAbility->IsStriding()));
					ASSERT_THAT(IsTrue(HasStrideCue(State.World)));
					ASSERT_THAT(IsTrue(HasInstanceStock(State, {NodeSectionCount, NodeSectionCount, NodeSectionCount}, 0)));
				})
			.UntilClients(
				TEXT("Clients receive the stride cue"),
				[](FNetworkState& State)
				{
					return HasStrideCue(State.World);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The harvester walks onto the first instance"),
				[](FNetworkState& State)
				{
					MoveStrideAvatar(State, FVector::ZeroVector);
				})
			.UntilServer(
				TEXT("A pulse empties the instance where the harvester is, and only that one"),
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount, NodeSectionCount}, 1);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The client presents the emptied instance"),
				0,
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount, NodeSectionCount}, 1);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second connection for the stride test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The late joiner receives the running stride's cue"),
				1,
				[](FNetworkState& State)
				{
					return IsClientReady(State) && HasStrideCue(State.World);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("The late joiner streams in the field"),
				1,
				[this](FNetworkState& State)
				{
					State.InstancesField = LoadInstancesField(State.World);
					ASSERT_THAT(IsNotNull(State.InstancesField));
				})
			.ThenServer(
				TEXT("The stride is still running, and the harvester skips ahead to the last instance"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(State.StrideAbility->IsStriding()));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						0));
					MoveStrideAvatar(State, FVector(600.0, 0.0, 0.0));
				})
			.UntilServer(
				TEXT("A pulse empties the last instance; the one it skipped stays"),
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount, 0}, 2);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("The stride ends after its duration"),
				[](FNetworkState& State)
				{
					return IsValid(State.StrideAbility) && !State.StrideAbility->IsStriding();
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The rewards of every pulse arrive in the inventory once, at the end"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsFalse(State.StrideAbility->IsActive()));
					ASSERT_THAT(IsFalse(HasStrideCue(State.World)));
					ASSERT_THAT(AreEqual(
						State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
							URpgNetworkAutomationMaterialDefinition::StaticClass()),
						2 * NodeSectionCount * LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
				})
			.UntilClients(
				TEXT("Both clients show the stride's path and remove its cue"),
				[](FNetworkState& State)
				{
					return HasInstanceStock(State, {0, NodeSectionCount, 0}, 2) && !HasStrideCue(State.World);
				},
				NetworkTimeout());
	}

	TEST_METHOD(CoopHarvestersShareStockAndLateJoinUnderLoad)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the co-op load test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the co-op load test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("A second player harvests in the same session over a poor connection"),
				[this](FNetworkState& State)
				{
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					State.SecondHarvester = State.World->SpawnActor<ARpgNetworkAutomationHarvesterState>(SpawnParameters);
					ASSERT_THAT(IsNotNull(State.SecondHarvester));
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
					// Every packet the host sends arrives 100 ms late and 2 % never arrive.
					EmulatePoorConnection(State.World, 100, 2);
				})
			.UntilClients(
				TEXT("Clients receive the instance stock for the co-op load test"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the dense field"),
				[this](FNetworkState& State)
				{
					State.LoadField = LoadDenseField(State.World);
					ASSERT_THAT(IsNotNull(State.LoadField));
				})
			.ThenClients(
				TEXT("Clients load the dense field, and their packets arrive late too"),
				[this](FNetworkState& State)
				{
					State.LoadField = LoadDenseField(State.World);
					ASSERT_THAT(IsNotNull(State.LoadField));
					EmulatePoorConnection(State.World, 100, 2);
				})
			.ThenServer(
				TEXT("Both players start a stride over the same instances in the same frame"),
				[this](FNetworkState& State)
				{
					// Both strides stand on the same instance and pulse in the same frames. Each pulse, the first takes
					// three sections of the four nearest instances and the second takes what the first left.
					const FVector Center = LoadFieldLocation + FVector(10.0 * LoadFieldSpacing, 10.0 * LoadFieldSpacing, 0.0);
					State.StrideAbility = StartStride(State.World, State.Harvester, Center, 650.0f, 2.0f, 0.25f, 3);
					State.SecondStrideAbility = StartStride(State.World, State.SecondHarvester, Center, 650.0f, 2.0f, 0.25f, 3);
					ASSERT_THAT(IsNotNull(State.StrideAbility));
					ASSERT_THAT(IsNotNull(State.SecondStrideAbility));
				})
			.UntilServer(
				TEXT("Both strides end"),
				[](FNetworkState& State)
				{
					return IsValid(State.StrideAbility) && !State.StrideAbility->IsActive() &&
						IsValid(State.SecondStrideAbility) && !State.SecondStrideAbility->IsActive();
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("The server has presented every instance the strides emptied"),
				[](FNetworkState& State)
				{
					TArray<int32> Presented;
					return GetPresentedFieldStock(State.LoadField, Presented);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Every section of the shared instances was taken exactly once"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(GetPresentedFieldStock(State.LoadField, ExpectedLoadFieldStock)));
					int32 TakenSections = 0;
					int32 DepletedInstances = 0;
					int32 ChangedInstances = 0;
					for (const int32 Remaining : ExpectedLoadFieldStock)
					{
						TakenSections += NodeSectionCount - Remaining;
						DepletedInstances += Remaining == 0 ? 1 : 0;
						ChangedInstances += Remaining < NodeSectionCount ? 1 : 0;
					}
					const int32 FirstShare = State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
						URpgNetworkAutomationMaterialDefinition::StaticClass());
					const int32 SecondShare = State.SecondHarvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
						URpgNetworkAutomationMaterialDefinition::StaticClass());
					ASSERT_THAT(IsTrue(DepletedInstances >= 8));
					ASSERT_THAT(IsTrue(FirstShare > 0 && SecondShare > 0));
					ASSERT_THAT(AreEqual(FirstShare + SecondShare, TakenSections * LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
					ExpectedChangedInstances = URpgHarvestInstanceStockComponent::FindForWorld(State.World)->GetNumChangedInstances();
					ASSERT_THAT(AreEqual(ExpectedChangedInstances, ChangedInstances));
					TestRunner->AddInfo(FString::Printf(
						TEXT("Co-op strides: %d instances depleted, %d sections taken; the players received %d and %d."),
						DepletedInstances,
						TakenSections,
						FirstShare,
						SecondShare));
				})
			.UntilClients(
				TEXT("Both clients present the shared instances like the server"),
				[this](FNetworkState& State)
				{
					return HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Thousands of instances change in one frame"),
				[this](FNetworkState& State)
				{
					// A busy saved world: more changed instances than one FastArray update may carry (2,048 by default).
					URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					URpgHarvestableInstancesComponent* Instances = State.LoadField->GetHarvestableInstances();
					ASSERT_THAT(IsNotNull(Stock));
					for (int32 InstanceIndex = 0; InstanceIndex < ExpectedLoadFieldStock.Num(); ++InstanceIndex)
					{
						if (ExpectedLoadFieldStock[InstanceIndex] != NodeSectionCount || InstanceIndex % 6 == 5)
						{
							continue;
						}
						FIntVector Key;
						ASSERT_THAT(IsTrue(Instances->GetInstanceKey(InstanceIndex, Key)));
						const int32 Sections = InstanceIndex % 5 == 0 ? 1 : NodeSectionCount;
						ASSERT_THAT(IsTrue(Stock->ExtractSections(Key, NodeSectionCount, Sections, 0.0f)));
						ExpectedLoadFieldStock[InstanceIndex] = NodeSectionCount - Sections;
					}
					ExpectedChangedInstances = Stock->GetNumChangedInstances();
					ASSERT_THAT(IsTrue(ExpectedChangedInstances > 2500));
					ASSERT_THAT(IsTrue(HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances)));
					BytesBeforeMassChange = GetBytesSentToClient(State, 0);
					MassChangeStartSeconds = FPlatformTime::Seconds();
				})
			.UntilClients(
				TEXT("The connected client receives every change"),
				[this](FNetworkState& State)
				{
					return HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server records what the mass change cost the connected client"),
				[this](FNetworkState& State)
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("Co-op load: %d changed instances reached the connected client within %.2f s; the server sent it %.1f KB meanwhile."),
						ExpectedChangedInstances,
						FPlatformTime::Seconds() - MassChangeStartSeconds,
						(GetBytesSentToClient(State, 0) - BytesBeforeMassChange) / 1024.0));
					LateJoinStartSeconds = FPlatformTime::Seconds();
				})
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second connection for the co-op load test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("The late joiner receives the whole stock"),
				1,
				[this](FNetworkState& State)
				{
					const URpgHarvestInstanceStockComponent* Stock =
						URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					return IsClientReady(State) && Stock && Stock->GetNumChangedInstances() == ExpectedChangedInstances;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server records what the late join cost"),
				[this](FNetworkState& State)
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("Co-op load: a late joiner had all %d changed instances %.2f s after its join started; the server sent it %.1f KB in total."),
						ExpectedChangedInstances,
						FPlatformTime::Seconds() - LateJoinStartSeconds,
						GetBytesSentToClient(State, 1) / 1024.0));
				})
			.ThenClient(
				TEXT("The late joiner streams in the dense field and presents the stored stock"),
				1,
				[this](FNetworkState& State)
				{
					State.LoadField = LoadDenseField(State.World);
					ASSERT_THAT(IsNotNull(State.LoadField));
					ASSERT_THAT(IsTrue(HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances)));
				})
			.ThenServer(
				TEXT("Thousands of depleted instances respawn in one frame"),
				[this](FNetworkState& State)
				{
					URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					URpgHarvestableInstancesComponent* Instances = State.LoadField->GetHarvestableInstances();
					ASSERT_THAT(IsNotNull(Stock));
					int32 Restored = 0;
					for (int32 InstanceIndex = 0; InstanceIndex < ExpectedLoadFieldStock.Num(); ++InstanceIndex)
					{
						FIntVector Key;
						if (ExpectedLoadFieldStock[InstanceIndex] == 0 && Instances->GetInstanceKey(InstanceIndex, Key) &&
							Stock->RestoreStock(Key))
						{
							ExpectedLoadFieldStock[InstanceIndex] = NodeSectionCount;
							++Restored;
						}
					}
					ASSERT_THAT(IsTrue(Restored > 2048));
					ExpectedChangedInstances = Stock->GetNumChangedInstances();
					ASSERT_THAT(IsTrue(HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances)));
				})
			.UntilClients(
				TEXT("Every client presents the respawned field"),
				[this](FNetworkState& State)
				{
					return HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances);
				},
				NetworkTimeout());
	}

	TEST_METHOD(CoopSwarmsShareTargetsAndStayWithinBandwidth)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and first connection are ready for the co-op swarm test"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial PIE client is ready for the co-op swarm test"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("A second player joins the swarm test over a poor connection"),
				[this](FNetworkState& State)
				{
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					State.SecondHarvester = State.World->SpawnActor<ARpgNetworkAutomationHarvesterState>(SpawnParameters);
					ASSERT_THAT(IsNotNull(State.SecondHarvester));
					AGameStateBase* GameState = State.World->GetGameState();
					ASSERT_THAT(IsNotNull(GameState));
					URpgHarvestInstanceStockComponent* Stock =
						NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
					Stock->RegisterComponent();
					EmulatePoorConnection(State.World, 100, 2);
				})
			.UntilClients(
				TEXT("Clients receive the instance stock for the co-op swarm test"),
				[](FNetworkState& State)
				{
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) != nullptr;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("The server loads the dense field for the swarms"),
				[this](FNetworkState& State)
				{
					State.LoadField = LoadDenseField(State.World);
					ASSERT_THAT(IsNotNull(State.LoadField));
				})
			.ThenClients(
				TEXT("Clients load the dense field for the swarms"),
				[this](FNetworkState& State)
				{
					State.LoadField = LoadDenseField(State.World);
					ASSERT_THAT(IsNotNull(State.LoadField));
					EmulatePoorConnection(State.World, 100, 2);
				})
			.ThenServer(
				TEXT("Two players summon two swarms each over the same 32 instances"),
				[this](FNetworkState& State)
				{
					// 64 creatures reserve stock independently; whoever arrives first takes it, the rest move on.
					TArray<int32> InstanceIndices;
					for (int32 Row = 20; Row < 24; ++Row)
					{
						for (int32 Column = 20; Column < 28; ++Column)
						{
							InstanceIndices.Add(Row * LoadFieldSize + Column);
						}
					}
					const FVector Center = LoadFieldLocation + FVector(23.5 * LoadFieldSpacing, 21.5 * LoadFieldSpacing, 0.0);
					const ARpgHarvestSwarm* Swarms[] = {
						SummonDenseFieldSwarm(State, State.Harvester, InstanceIndices, Center + FVector(-600.0, -600.0, 0.0)),
						SummonDenseFieldSwarm(State, State.Harvester, InstanceIndices, Center + FVector(600.0, -600.0, 0.0)),
						SummonDenseFieldSwarm(State, State.SecondHarvester, InstanceIndices, Center + FVector(-600.0, 600.0, 0.0)),
						SummonDenseFieldSwarm(State, State.SecondHarvester, InstanceIndices, Center + FVector(600.0, 600.0, 0.0))};
					for (const ARpgHarvestSwarm* Swarm : Swarms)
					{
						ASSERT_THAT(IsNotNull(Swarm));
					}
					BytesBeforeSwarms = GetBytesSentToClient(State, 0);
					SwarmStartSeconds = FPlatformTime::Seconds();
				})
			.UntilClients(
				TEXT("Clients present the creatures of all four swarms"),
				[](FNetworkState& State)
				{
					return CountSwarms(State.World) == 4 && CountSwarmCreatureActors(State.World) > 0;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Every swarm finished and delivered"),
				[](FNetworkState& State)
				{
					return CountSwarms(State.World) == 0;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Every section of the shared instances was taken and delivered exactly once"),
				[this](FNetworkState& State)
				{
					const double Seconds = FPlatformTime::Seconds() - SwarmStartSeconds;
					const int32 Bytes = GetBytesSentToClient(State, 0) - BytesBeforeSwarms;
					ASSERT_THAT(IsTrue(GetPresentedFieldStock(State.LoadField, ExpectedLoadFieldStock)));
					int32 TakenSections = 0;
					int32 DepletedInstances = 0;
					for (const int32 Remaining : ExpectedLoadFieldStock)
					{
						TakenSections += NodeSectionCount - Remaining;
						DepletedInstances += Remaining == 0 ? 1 : 0;
					}
					const int32 FirstShare = State.Harvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
						URpgNetworkAutomationMaterialDefinition::StaticClass());
					const int32 SecondShare = State.SecondHarvester->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(
						URpgNetworkAutomationMaterialDefinition::StaticClass());
					ASSERT_THAT(AreEqual(DepletedInstances, 32));
					ASSERT_THAT(AreEqual(TakenSections, 32 * NodeSectionCount));
					ASSERT_THAT(IsTrue(FirstShare > 0 && SecondShare > 0));
					ASSERT_THAT(AreEqual(FirstShare + SecondShare, TakenSections * LootQuantity));
					ASSERT_THAT(AreEqual(CountWorldDrops(State.World), 0));
					ExpectedChangedInstances = URpgHarvestInstanceStockComponent::FindForWorld(State.World)->GetNumChangedInstances();
					ASSERT_THAT(AreEqual(ExpectedChangedInstances, 32));
					TestRunner->AddInfo(FString::Printf(
						TEXT("Co-op swarms: 4 swarms of 16 creatures emptied 32 instances in %.2f s; the players received %d and %d. The server sent the client %.1f KB meanwhile, %.1f KB/s."),
						Seconds,
						FirstShare,
						SecondShare,
						Bytes / 1024.0,
						Bytes / 1024.0 / FMath::Max(Seconds, 0.001)));
				})
			.UntilClients(
				TEXT("The client shows the emptied instances and retires every swarm and creature"),
				[this](FNetworkState& State)
				{
					return HasDenseFieldStock(State, ExpectedLoadFieldStock, ExpectedChangedInstances) &&
						CountSwarms(State.World) == 0 &&
						CountSwarmCreatureActors(State.World) == 0;
				},
				NetworkTimeout());
	}

	TEST_METHOD(GatheringSetGameFeatureReactivationIsIdempotent)
	{
		using namespace RpgLootHarvestPIETests;

		Network
			.UntilServer(
				TEXT("Dedicated server and initial client are ready for GameFeature lifecycle"),
				[](FNetworkState& State)
				{
					return IsServerReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("PIE client is ready for GameFeature lifecycle"),
				[](FNetworkState& State)
				{
					return IsClientReady(State);
				},
				NetworkTimeout())
			.SpawnAndReplicate<
				ARpgNetworkAutomationHarvesterState,
				&FNetworkState::Harvester>(
				[](ARpgNetworkAutomationHarvesterState& Harvester)
				{
					(void)Harvester;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Activate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.Harvester));
					UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					if (!Subsystem.GetPluginURLByName(
							TEXT("GF_Harvesting_Magic"),
							HarvestingPluginURL))
					{
						TestRunner->AddError(
							TEXT("Could not resolve GF_Harvesting_Magic plugin URL."));
						bPluginTransitionComplete = true;
						return;
					}
					Subsystem.LoadAndActivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginLoadComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature activation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Duplicate readiness events do not duplicate GatheringSet"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(bPluginTransitionSucceeded));
					ASSERT_THAT(IsNotNull(State.Harvester));
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
				})
			.UntilServer(
				TEXT("Server owns exactly one GatheringSet after activation"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client receives exactly one replicated GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Deactivate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginDeactivateComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature deactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Server removes every GatheringSet on deactivation"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionSucceeded && CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client removes the replicated GatheringSet on deactivation"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Reactivate the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginLoadComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Harvesting GameFeature reactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Reactivated feature receives the normal ability-ready event"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(bPluginTransitionSucceeded));
					UGameFrameworkComponentManager::
						SendGameFrameworkComponentExtensionEvent(
							State.Harvester,
							ARpgBasePlayerState::NAME_RpgAbilityReady);
				})
			.UntilServer(
				TEXT("Reactivation grants exactly one server GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Reactivation replicates exactly one client GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cleanup deactivates the harvesting GameFeature"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(
						HarvestingPluginURL,
						FGameFeaturePluginDeactivateComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("Cleanup deactivation completes without residual server grants"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete && bPluginTransitionSucceeded &&
						CountGatheringSets(State) == 0;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Cleanup leaves no replicated GatheringSet"),
				[](FNetworkState& State)
				{
					return CountGatheringSets(State) == 0;
				},
				NetworkTimeout());
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
